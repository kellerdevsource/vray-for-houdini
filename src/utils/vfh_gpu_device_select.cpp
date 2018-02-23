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

#include "vfh_defines.h"
#include "vfh_gpu_device_select.h"
#include "vfh_log.h"
#include "vfh_vray_instances.h"
#include "vfh_hou_utils.h"

#include <HOM/HOM_qt.h>
#include <HOM/HOM_Module.h>
#include <OP/OP_Director.h>
#include <RE/RE_Window.h>

#include <QDebug>
#include <QMainWindow>
#include <QListWidget>
#include <QStringList>
#include <QCheckBox>
#include <QDirIterator>
#include <QVBoxLayout>

using namespace VRayForHoudini;

typedef std::vector<VRay::ComputeDeviceInfo> ComputeDeviceInfoList;

static char vfhGpuDevices[] = "VFH_GPU_DEVICES";

static const QString vfhCheckBox(
"QCheckBox, QRadioButton {"
"	color: #AFBDC4;"
"}"
""
"QCheckBox::indicator::unchecked  {"
"	background-color: #263238;"
"	border: 1px solid #536D79;"
"}"
""
"QCheckBox::indicator::checked, QTreeView::indicator::checked {"
"	background-color: qradialgradient(cx:0.5, cy:0.5, fx:0.25, fy:0.15, radius:0.3, stop:0 #80CBC4, stop:1 #263238);"
"	border: 1px solid #536D79;"
"}"
""
"QCheckBox::indicator:disabled, QTreeView::indicator:disabled {"
"	background-color: #444444;"
"}"
""
"QCheckBox::indicator::checked:disabled, QTreeView::indicator::checked:disabled {  "
"	background-color: qradialgradient(cx:0.5, cy:0.5, fx:0.25, fy:0.15, radius:0.3, stop:0 #BBBBBB, stop:1 #444444);"
"}");

/// Available device list.
static QStringList availableDeviceList;
static QString hdkStyleSheet;
static QStyle *hdkStyle = nullptr;

static void dumpStyleSheet(QWidget *object)
{
#if 0
	QDirIterator it(":", QDirIterator::Subdirectories);
	while (it.hasNext()) {
		qDebug() << it.next();
	}
#endif
	for (QObject *child : object->children()) {
		QWidget *widget = qobject_cast<QWidget*>(child);
		if (widget) {
			hdkStyleSheet.append(widget->styleSheet());

			if (qobject_cast<QCheckBox*>(widget)) {
				Log::getLog().error("%p", widget->style());
			}

			dumpStyleSheet(widget);
		}
	}
}

static void findHdkCheckbox()
{
	QWidget *mainWindow = RE_Window::mainQtWindow();
	vassert(mainWindow);

	hdkStyle = mainWindow->style();

	dumpStyleSheet(mainWindow);
}

static void appendDeviceToList(const ComputeDeviceInfoList &deviceInfoList, QStringList &devicesList)
{
	for (const VRay::ComputeDeviceInfo &devInfo : deviceInfoList) {
		devicesList.append(devInfo.name.c_str());
	}
}

static void getAvailableDeviceList(QStringList &devicesList)
{
	if (availableDeviceList.empty()) {
		if (newVRayInit()) {
			const VRay::RendererOptions &options = getDefaultOptions(HOU::isUIAvailable());
			VRay::VRayRenderer *renderer = newVRayRenderer(options);
			if (renderer) {
				try {
					appendDeviceToList(renderer->getComputeDevicesCUDA(), availableDeviceList);
				}
				catch (...) {
					VRay::Error err = renderer->getLastError();
					if (err != VRay::SUCCESS) {
						Log::getLog().error("Last error: %s", err.toString());
					}
				}

				try {
					appendDeviceToList(renderer->getComputeDevicesOpenCL(), availableDeviceList);
				}
				catch (...) {
					VRay::Error err = renderer->getLastError();
					if (err != VRay::SUCCESS) {
						Log::getLog().error("Last error: %s", err.toString());
					}
				}

				deleteVRayRenderer(renderer);
			}
		}
	}

	devicesList = availableDeviceList;
}

static void setStoredDeviceList(const QStringList &devicesList)
{
	OP_CommandManager &cmdMan = *OPgetDirector()->getCommandManager();

	cmdMan.setVariable(vfhGpuDevices, _toChar(devicesList.join(';')), false);
}

static void getStoredDeviceList(QStringList &devicesList)
{
	OP_CommandManager &cmdMan = *OPgetDirector()->getCommandManager();

	UT_String envGpuDevices;
	if (!cmdMan.getVariable(vfhGpuDevices, envGpuDevices))
		return;

	const QString gpuDevices(envGpuDevices.buffer());
	devicesList = gpuDevices.split(';');
}

struct VfhGpuDeviceSelect
	: QMainWindow
{
	Q_OBJECT

public:
	VfhGpuDeviceSelect(VfhGpuDeviceSelect* &windowInstance, QWidget *parent)
		: QMainWindow(parent)
		, windowInstance(windowInstance)
	{
		setWindowTitle(tr("V-Ray GPU Device Select"));
		setAttribute(Qt::WA_DeleteOnClose);
		setWindowFlags(Qt::Window|Qt::Tool);
		setStyle(hdkStyle);

		initUI();
		fillDeviceList();

		resize(1024, 768);
	}

	~VfhGpuDeviceSelect() VRAY_DTOR_OVERRIDE {
		windowInstance = nullptr;
	}

	void initUI() {
		listWidget = new QListWidget(this);

		QCheckBox *box = new QCheckBox(this);
		box->setStyle(hdkStyle);
		box->setMaximumSize(QSize(32,32));

		QVBoxLayout *mainLayout = new QVBoxLayout;
		mainLayout->addWidget(box);
		mainLayout->addWidget(listWidget);

		QWidget *centralWidget = new QWidget(this);
		centralWidget->setLayout(mainLayout);

		setCentralWidget(centralWidget);
	}

	void fillDeviceList() const {
		QStringList availDeviceList;
		getAvailableDeviceList(availDeviceList);

		QStringList selectedDeviceList;
		getStoredDeviceList(selectedDeviceList);

		listWidget->clear();
		listWidget->setStyleSheet(vfhCheckBox);

		for (const QString &devName : availDeviceList) {
			Log::getLog().debug("GPU: %s", _toChar(devName));

			QListWidgetItem *item = new QListWidgetItem(devName, listWidget);
			item->setFlags(item->flags()|Qt::ItemIsUserCheckable);

			if (selectedDeviceList.contains(devName)) {
				item->setCheckState(Qt::Checked);
			}
			else {
				item->setCheckState(Qt::Unchecked);
			}

			listWidget->addItem(item);
		}
	}

protected:
	QListWidget *listWidget = nullptr;

private:
	VfhGpuDeviceSelect* &windowInstance;
};

/// We allow only one window instance.
static VfhGpuDeviceSelect *windowInstance = nullptr;

void VRayForHoudini::showGpuDeviceSelectUI()
{
	if (!windowInstance) {
		findHdkCheckbox();

		windowInstance = new VfhGpuDeviceSelect(windowInstance, RE_Window::mainQtWindow());
		windowInstance->show();
	}
}

#ifndef Q_MOC_RUN
#include <vfh_gpu_device_select.cpp.moc>
#endif
