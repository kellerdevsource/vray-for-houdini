//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//  https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_vray_cloud.h"
#include "vfh_log.h"
#include "vfh_defines.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegExp>
#include <QMainWindow>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QTextBrowser>
#include <QProgressBar>
#include <QPushButton>
#include <QQueue>
#include <QCloseEvent>

#include <RE/RE_Window.h>

using namespace VRayForHoudini;
using namespace Cloud;

/// JSON config key for the executable file path.
static const QString keyExecutable("executable");

/// Resolved V-Ray Cloud Client binary file path.
static QString vrayCloudClient;

/// Flag indicatig that we've already tried to resolved
/// V-Ray Cloud Client binary file path.
static int vrayCloudClientChecked(false);

/// Not allowed characters regular expression.
static QRegExp cloudNameFilter("[^a-zA-Z\\d\\s\\_\\-.,\\(\\)\\[\\]]");

/// HTML log block format.
static const QString block("<pre>%1</pre>");

/// URL match to replace with link.
static const QRegExp urlMatch("((?:https?|ftp)://\\S+)");
static const QString urlMatchReplace("<a href=\"\\1\">\\1</a>");

/// Only letters, digits, spaces and _-.,()[] allowed.
/// NOTE: QString::remove is non-const.
static QString filterName(QString value)
{
	return value.remove(cloudNameFilter);
}

/// Parses vcloud.json and extracts executable file path.
static void findVRayCloudClient()
{
	if (vrayCloudClientChecked)
		return;

	QString cgrHome; 
#ifdef _WIN32
	const QProcessEnvironment &env = QProcessEnvironment::systemEnvironment();
	cgrHome = env.value("APPDATA");
#else
	cgrHome = QDir::homePath();
#endif

	QStringList vcloudDirList;
	vcloudDirList << cgrHome;
#ifdef _WIN32
	vcloudDirList << "Chaos Group";
#else
	vcloudDirList << ".ChaosGroup";
#endif
	vcloudDirList << "vcloud";
	vcloudDirList << "client";
	vcloudDirList << "vcloud.json";

	const QString vcloudFilePath = vcloudDirList.join(QDir::separator());

	QFileInfo vcloudFileInfo(vcloudFilePath);
	if (vcloudFileInfo.exists()) {
		QFile vcloudFile(vcloudFileInfo.absoluteFilePath());
		if (vcloudFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
			QJsonDocument vcloudJson = QJsonDocument::fromJson(vcloudFile.readAll());
			vcloudFile.close();

			if (vcloudJson.isObject()) {
				const QJsonObject vcloudObject = vcloudJson.object();
				if (vcloudObject.contains(keyExecutable)) {
					vrayCloudClient = vcloudObject[keyExecutable].toString();
				}
			}
		}
	}

	if (vrayCloudClient.isEmpty()) {
		Log::getLog().error("V-Ray Cloud Client is not found! Please, register at https://vray.cloud!");
	}

	vrayCloudClientChecked = true;
}

JobFilePath::JobFilePath(const QString &filePath)
	: filePath(filePath)
{}

JobFilePath::~JobFilePath()
{
	removeFilePath(filePath);
}

QString JobFilePath::createFilePath()
{
	QString tmpFilePath;

	QFileInfo tempDir(QDir::tempPath());
	if (!tempDir.isDir() || !tempDir.isWritable()) {
		Log::getLog().error("Temporary location \"%s\" is not writable!",
							_toChar(tempDir.absolutePath()));
	}
	else {
		QFileInfo tempFile(tempDir.absoluteFilePath(), "vfhVRayCloud.vrscene");
		if (tempFile.exists()) {
			removeFilePath(tempFile.absoluteFilePath());
		}

		tmpFilePath = tempFile.absoluteFilePath();
	}

	return tmpFilePath;
}

void JobFilePath::removeFilePath(const QString &filePath)
{
	const int removeRes = QFile::remove(filePath);
	if (!removeRes) {
		Log::getLog().error("Failed to remove \"%s\"!", _toChar(filePath));
	}
}

Job::Job(const QString &sceneFile)
	: sceneFile(sceneFile)
{}

void Job::setProject(const QString &value)
{
	project = filterName(value);
}

void Job::setName(const QString &value)
{
	name = filterName(value);
}

void Job::toArguments(const Job &job, QStringList &arguments)
{
	arguments << "--project" << job.getProject();
	arguments << "--name" << job.getName();
	arguments << "--sceneFile" << job.sceneFile;

	arguments << "--renderMode" << job.renderMode;

	arguments << "--width" << QString::number(job.width);
	arguments << "--height" << QString::number(job.height);

	if (job.animation) {
		arguments << "--animation";
		arguments << "--frameRange" << QString::number(job.frameRange.first) << QString::number(job.frameRange.second);
		arguments << "--frameStep" << QString::number(job.frameStep);
	}

	if (job.ignoreWarnings) {
		arguments << "--ignoreWarnings";
	}
	if (job.noProgress) {
		arguments << "--no-progress";
	}
	if (job.vr) {
		arguments << "--vr";
	}
	if (!job.colorCorrectionsFile.isEmpty()) {
		arguments << "--colorCorrectionsFile" << job.colorCorrectionsFile;
	}
}

class CloudWindow
	: public QMainWindow
{
	Q_OBJECT

public:
	struct CloudCommand {
		QString command;
		QStringList arguments; 
	};

	typedef QQueue<CloudCommand> CloudCommands;

	CloudWindow(CloudWindow* &cloudWindowInstance, const QString &jobFile, QWidget *parent)
		: QMainWindow(parent)
		, jobFile(jobFile)
		, cloudWindowInstance(cloudWindowInstance)
	{
		setWindowTitle(tr("V-Ray Cloud Client: Scene upload"));
		setAttribute(Qt::WA_DeleteOnClose);
		setWindowFlags(Qt::Window|Qt::Tool);

		setupUI();

		connectSlotsUI();
		connectSlotsProcess();

		resize(1024, 768);
	}

	~CloudWindow() VRAY_DTOR_OVERRIDE {
		terminateProcess();

		cloudWindowInstance = nullptr;
	}

	void setupUI() {
		editor = new QTextBrowser(this);
		editor->setReadOnly(true);
		editor->setOpenLinks(true);
		editor->setOpenExternalLinks(true);

		progress = new QProgressBar(this);

		stopButton = new QPushButton("Abort", this);

		QHBoxLayout *progressLayout = new QHBoxLayout;
		progressLayout->setMargin(0);
		progressLayout->setSpacing(10);
		progressLayout->addWidget(progress);
		progressLayout->addWidget(stopButton);

		QVBoxLayout *mainLayout = new QVBoxLayout;
		mainLayout->setMargin(10);
		mainLayout->setSpacing(10);
		mainLayout->addWidget(editor);
		mainLayout->addLayout(progressLayout);

		QWidget *centralWidget = new QWidget(this);
		centralWidget->setLayout(mainLayout);

		setCentralWidget(centralWidget);
	}

	void connectSlotsUI() const {
		connect(stopButton, SIGNAL(clicked()),
		        this, SLOT(onPressAbort()));
	}

	void connectSlotsProcess() const {
		connect(&proc, SIGNAL(readyReadStandardOutput()),
		        this, SLOT(onProcStdOutput()));

		connect(&proc, SIGNAL(readyReadStandardError()),
		        this, SLOT(onProcStdError()));

		connect(&proc, SIGNAL(error(QProcess::ProcessError)),
		        this, SLOT(onProcError()));

		connect(&proc, SIGNAL(finished(int)),
		        this, SLOT(onProcFinished()));
	}

	void uploadScene(const Job &job) {
		{
			CloudCommand cmd;
			cmd.command = vrayCloudClient;
			cmd.arguments << "project" << "create" << "--name" << job.getProject();

			commands.enqueue(cmd);
		}
		{
			CloudCommand cmd;
			cmd.command = vrayCloudClient;
			cmd.arguments << "job" << "submit";

			Job::toArguments(job, cmd.arguments);

			commands.enqueue(cmd);
		}

		// Busy progress.
		progress->setMinimum(0);
		progress->setMaximum(0);

		executeVRayCloudClient();
	}

	void executeVRayCloudClient() {
		if (commands.empty())
			return;

		const CloudCommand cmd = commands.dequeue();

		Log::getLog().info("Calling V-Ray Cloud Client: %s", _toChar(cmd.arguments.join(" ")));

		proc.start(cmd.command, cmd.arguments, QIODevice::ReadOnly);
	}

	void uploadCompleted() const {
		progress->hide();
		stopButton->hide();
	}

private Q_SLOTS:
	void onPressAbort() {
		close();
	}

	void onProcError() {
		commands.clear();

		uploadCompleted();
	}

	void onProcFinished() {
		if (commands.isEmpty()) {
			uploadCompleted();
			return;
		}

		executeVRayCloudClient();
	}

	void onProcStdError() {
		appendText(proc.readAllStandardError());
	}

	void onProcStdOutput() {
		appendText(proc.readAllStandardOutput());
	}

protected:
	void closeEvent(QCloseEvent *ev) VRAY_OVERRIDE {
		QMessageBox::StandardButton res(QMessageBox::Yes);

		if (proc.state() != QProcess::NotRunning ||
		    !commands.isEmpty())
		{
			res = QMessageBox::warning(this, tr("V-Ray Cloud Client: Abort upload"),
			                           tr("This will abort scene upload. Are you sure?"),
			                           QMessageBox::Cancel | QMessageBox::Yes,
			                           QMessageBox::Yes);
		}

		if (res != QMessageBox::Yes) {
			ev->ignore();
		}
		else {
			terminateProcess();
			ev->accept();
		}
	}

	void appendText(const QByteArray &data) const {
		QString text(QString(data).trimmed());
		if (text.isEmpty())
			return;

		text = text.replace(urlMatch, urlMatchReplace);

		editor->insertHtml(block.arg(text));

		// XXX: Ugly; haven't figured out how to add a new via insertHtml();
		editor->insertPlainText("\n");
	}

	void terminateProcess() {
		if (proc.state() == QProcess::NotRunning)
			return;

		progress->setFormat("Aborting upload...");
		progress->setMinimum(0);
		progress->setMaximum(1);
		progress->setValue(1);

		stopButton->setEnabled(false);

		commands.clear();
		proc.terminate();
		proc.waitForFinished(2000);
	}

	QTextBrowser *editor = nullptr;
	QProgressBar *progress = nullptr;
	QPushButton *stopButton = nullptr;

	QProcess proc;
	CloudCommands commands;

private:
	JobFilePath jobFile;
	CloudWindow* &cloudWindowInstance;
};

static CloudWindow *cloudWindowInstance = nullptr;

int VRayForHoudini::Cloud::submitJob(const Job &job)
{
	findVRayCloudClient();

	if (vrayCloudClient.isEmpty())
		return false;

	Log::getLog().info("Using V-Ray Cloud Client: \"%s\"", _toChar(vrayCloudClient));

	if (!cloudWindowInstance) {
		cloudWindowInstance = new CloudWindow(cloudWindowInstance, job.sceneFile, RE_Window::mainQtWindow());
		cloudWindowInstance->show();
		cloudWindowInstance->uploadScene(job);
	}

	return true;
}

#ifndef Q_MOC_RUN
#include <vfh_vray_cloud.cpp.moc>
#endif
