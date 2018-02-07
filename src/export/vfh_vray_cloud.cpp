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

using namespace VRayForHoudini;
using namespace Cloud;

static const QString keyExecutable = "executable";

static QString vrayCloudClient;
static int vrayCloudClientChecked = false;

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

JobFilePath::JobFilePath()
{
	createFilePath();
}

JobFilePath::~JobFilePath()
{
	removeFilePath();
}

static int deleteFilePath(const QString &filePath)
{
	const int removeRes = QFile::remove(filePath);
	if (!removeRes) {
		Log::getLog().error("Failed to remove \"%s\"!", _toChar(filePath));
	}
	return removeRes;
}

void JobFilePath::createFilePath()
{
	QFileInfo tempDir(QDir::tempPath());
	if (!tempDir.isDir() || !tempDir.isWritable()) {
		Log::getLog().error("Temporary location \"%s\" is not writable!",
							_toChar(tempDir.absolutePath()));
	}
	else {
		QFileInfo tempFile(tempDir.absoluteFilePath(), "vfhVRayCloud.vrscene");
		if (tempFile.exists()) {
			deleteFilePath(tempFile.absoluteFilePath());
		}

		filePath = tempFile.absoluteFilePath();
	}
}

void JobFilePath::removeFilePath()
{
	if (!isValid())
		return;

	deleteFilePath(filePath);

	filePath.clear();
}

Job::Job(const QString &sceneFile)
	: sceneFile(sceneFile)
{}

static QRegExp cloudNameFilter("[^a-zA-Z\\d\\s]");

/// Only letters, digits, spaces and _-.,()[] allowed.
/// NOTE: QString::remove is non-const.
static QString filterName(QString value)
{
	return value.remove(cloudNameFilter);
}

void Job::setName(const QString &value)
{
	name = filterName(value);
}

void Job::toArguments(const Job &job, QStringList &arguments)
{
	arguments << "--project" << job.project;
	arguments << "--name" << job.getName();
	arguments << "--sceneFile" << job.sceneFile;

	arguments << "--renderMode" << job.renderMode;

	if (job.exr) {
		arguments << "--exr";
	}
	else if (job.jpg) {
		arguments << "--jpg";
	}
	else if (job.png) {
		arguments << "--png";
		if (job.preserveAlpha) {
			arguments << "--preserveAlpha";
		}
	}

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

static void executeVRayCloudClient(const QStringList &args)
{
	Log::getLog().info("Calling V-Ray Cloud Client: %s", _toChar(args.join(" ")));

	QProcess vrayCloudClientProc;
	vrayCloudClientProc.setProcessChannelMode(QProcess::ForwardedChannels);
	vrayCloudClientProc.start(vrayCloudClient, args, QIODevice::ReadOnly);
	vrayCloudClientProc.waitForFinished();
}

int VRayForHoudini::Cloud::submitJob(const Job &job)
{
	findVRayCloudClient();

	if (vrayCloudClient.isEmpty())
		return false;

	Log::getLog().info("Using V-Ray Cloud Client: \"%s\"", _toChar(vrayCloudClient));

	{
		QStringList projectArgs;
		projectArgs << "project" << "create" << "--name" << job.project;

		executeVRayCloudClient(projectArgs);
	}
	{
		QStringList submitArgs;
		submitArgs << "job" << "submit";

		Job::toArguments(job, submitArgs);

		executeVRayCloudClient(submitArgs);
	}

	return true;
}
