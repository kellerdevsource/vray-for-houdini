//
// Copyright (c) 2015-2018, Chaos Software Ltd
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
#include "vfh_includes.h"

#include <HOM/HOM_qt.h>
#include <HOM/HOM_ui.h>
#include <HOM/HOM_Module.h>
#include <OP/OP_Director.h>
#include <RE/RE_Window.h>

#include <QDebug>
#include <QMainWindow>
#include <QListWidget>
#include <QCheckBox>
#include <QPainter>
#include <QGroupBox>
#include <QPushButton>
#include <QHBoxLayout>

using namespace VRayForHoudini;

typedef std::vector<VRay::ComputeDeviceInfo> ComputeDeviceInfoList;

static char vfhGpuCudaDevices[]   = "VFH_GPU_CUDA_DEVICES";
static char vfhGpuOpenClDevices[] = "VFH_GPU_OPENCL_DEVICES";

/// Available device list.
static int availableDeviceListInitialized = false;
static QStringList availableCudaDeviceList;
static QStringList availableOpenClDeviceList;

static void appendDeviceToList(const ComputeDeviceInfoList &deviceInfoList, QStringList &devicesList)
{
	for (const VRay::ComputeDeviceInfo &devInfo : deviceInfoList) {
		devicesList.append(devInfo.name.c_str());
	}
}

static void initAvailableDeviceList()
{
	if (availableDeviceListInitialized)
		return;

	if (newVRayInit()) {
		const VRay::RendererOptions &options = getDefaultOptions(HOU::isUIAvailable());
		VRay::VRayRenderer *renderer = newVRayRenderer(options);
		if (renderer) {
			try {
				appendDeviceToList(renderer->getComputeDevicesCUDA(), availableCudaDeviceList);
			}
			catch (...) {
				VRay::Error err = renderer->getLastError();
				if (err != VRay::SUCCESS) {
					Log::getLog().error("Last error: %s", err.toString());
				}
			}

			try {
				appendDeviceToList(renderer->getComputeDevicesOpenCL(), availableOpenClDeviceList);
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

	availableDeviceListInitialized = true;
}

static void setStoredDeviceList(const char *deviceVarName, const QStringList &devicesList)
{
	OP_CommandManager &cmdMan = *OPgetDirector()->getCommandManager();

	cmdMan.setVariable(deviceVarName, _toChar(devicesList.join(';')), false);
}

static void getStoredDeviceList(const char *deviceVarName, QStringList &devicesList)
{
	OP_CommandManager &cmdMan = *OPgetDirector()->getCommandManager();

	UT_String envGpuDevices;
	if (!cmdMan.getVariable(deviceVarName, envGpuDevices))
		return;

	const QString gpuDevices(envGpuDevices.buffer());
	devicesList = gpuDevices.split(';');
}

struct HouPalette
{
	HouPalette() = default;

	static QColor getHouColor(const char *name) {
		const void *ptr = HOM().qt()._getColor(name);
		if (!ptr)
			return {};
		return *reinterpret_cast<const QColor*>(ptr);
	}

	void init() {
		if (initialized)
			return;

		{
#if defined(HDK_16_5)
			const int _iconSize = HOM().ui().scaledSize(14);
#else
			const int _iconSize = 14;
#endif
			iconSize = QSize(_iconSize, _iconSize);
		}

		buttonText = getHouColor("ButtonText");
		disabledTextColor = getHouColor("DisabledTextColor");
		border = getHouColor("TextboxBorderPrimary");
		disabledBorder = getHouColor("TextboxBorderHighlight");
		textboxBG = getHouColor("TextboxBG");
		textboxBorderPrimary = getHouColor("TextboxBorderPrimary");
		textboxBorderHighlight = getHouColor("TextboxBorderHighlight");
		checkLocated = getHouColor("CheckLocated");
		checkColor = getHouColor("CheckColor");

		initialized = true;
	}

	QColor buttonText;
	QColor disabledTextColor;
	QColor border;
	QColor disabledBorder;
	QColor textboxBG;
	QColor textboxBorderPrimary;
	QColor textboxBorderHighlight;
	QColor checkLocated;
	QColor checkColor;

	QSize iconSize;

	int initialized = false;

	Q_DISABLE_COPY(HouPalette)
};

static HouPalette houPalette;

/// Based on $HFS/houdini/python2.7libs/houpythonportion.py
struct HouCheckBox
	: QCheckBox
{
	Q_OBJECT

public:
	explicit HouCheckBox(QWidget *parent=nullptr)
		: QCheckBox(parent)
	{
		setAttribute(Qt::WA_Hover, true);
		setIconSize(houPalette.iconSize);
	}

	QSize sizeHint() const Q_DECL_OVERRIDE {
		QSize _sizeHint = houPalette.iconSize;
		_sizeHint.setWidth(_sizeHint.width() + 10);
		return _sizeHint;
	}

protected:
	void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE {
		QPainter painter(this);

		const int box_width = iconSize().width() - 1;
		const int box_height = iconSize().height() - 1;
		_drawBox(painter, 0, 0, box_width, box_height);

		const int text_x = box_width + 8;
		const int text_y = 0;
		const int text_w = width() - text_x;
		const int text_h = height() - 4;
		_drawText(painter, text_x, text_y, text_w, text_h);

		if (isChecked()) {
			const int check_x = 3;
			const int check_y = 2;
			const int check_width = box_width - 2;
			const int check_height = box_height - 2;
			_drawCheckMark(painter, check_x, check_y, check_width, check_height);
		}
	}

private:
	void _drawText(QPainter &painter, int x, int y, int w, int h) const {
		const int textAlign = Qt::AlignVCenter | Qt::AlignLeft | Qt::TextShowMnemonic;
		const QColor textPen = isEnabled() ? houPalette.buttonText : houPalette.disabledTextColor;

		painter.setPen(textPen);
		painter.drawText(x, y, w, h, textAlign, text());
	}

	static void _drawBox(QPainter &painter, int x, int y, int w, int h) {
		painter.setPen(houPalette.textboxBorderPrimary);
		painter.drawRect(x, y, w, h);

		painter.setPen(houPalette.textboxBorderHighlight);
		painter.drawLine(x + 1, y + 1, x + w - 1, y + 1);
		painter.drawLine(x + 1, y + 1, x + 1, y + h - 1);

		painter.fillRect(x + 2, y + 2, x + w - 2, y + h - 2,
		                 houPalette.textboxBG);
	}

	void _drawCheckMark(QPainter &painter, int x, int y, int w, int h) const {
		painter.setRenderHints(QPainter::Antialiasing);

		const QColor check_mark_color = underMouse() ? houPalette.checkLocated : houPalette.checkColor;

		painter.setPen(check_mark_color);
		painter.setBrush(check_mark_color);

		static QPointF point_percentages[] = {
			{ 0.00, 0.50 },
			{ 0.20, 0.30 },
			{ 0.35, 0.50 },
			{ 0.50, 0.25 },
			{ 0.75, 0.10 },
			{ 1.00, 0.00 },
			{ 0.80, 0.20 },
			{ 0.55, 0.50 },
			{ 0.35, 1.00 },
			{ 0.20, 0.60 },
		};

		QVector<QPointF> points(COUNT_OF(point_percentages));
		int i = 0;
		for (const QPointF &point_pct : point_percentages) {
			const QPointF point(x + point_pct.x() * w,
			                    y + point_pct.y() * h);
			points[i++] = point;
		}

		const QPolygonF polygon(points);
		painter.drawPolygon(polygon);
	}
};

struct VfhGpuDeviceListWidget
	: QGroupBox
{
	Q_OBJECT

public:
	VfhGpuDeviceListWidget(const QString &title, const char *deviceVarName, const QStringList &deviceList, QWidget *parent)
		: QGroupBox(parent)
		, deviceVarName(deviceVarName)
		, deviceList(deviceList)
	{
		setTitle(title);

		fillWidgetList();

		widgetButtonApply = new QPushButton("Apply", this);

		connect(
			widgetButtonApply, SIGNAL(clicked()),
			this, SLOT(storeVariable())
		);

		QVBoxLayout *mainLayout = new QVBoxLayout;
		for (QWidget *widget : widgetList) {
			mainLayout->addWidget(widget);
		}
		mainLayout->addStretch();
		mainLayout->addWidget(widgetButtonApply);

		setLayout(mainLayout);
	}

	void fillWidgetList() {
		widgetList.clear();

		QStringList storedDeviceList;
		getStoredDeviceList(deviceVarName, storedDeviceList);

		for (const QString &devName : deviceList) {
			HouCheckBox *item = new HouCheckBox(this);
			item->setText(devName);

			if (!storedDeviceList.empty() && storedDeviceList.contains(devName)) {
				item->setCheckState(Qt::Checked);
			}

			widgetList.append(item);
		}
	}

private Q_SLOTS:
	void storeVariable() {
		QStringList storeDeviceList;

		for (int i = 0; i < widgetList.count(); ++i) {
			const HouCheckBox *item = static_cast<HouCheckBox*>(widgetList[i]);
			vassert(item);
			if (item->isChecked()) {
				storeDeviceList.append(item->text());
			}
		}

		setStoredDeviceList(deviceVarName, storeDeviceList);

		close();
	}

private:
	QPushButton *widgetButtonApply = nullptr;
	QWidgetList widgetList;
	const char *deviceVarName;
	const QStringList &deviceList;
};

struct VfhGpuDeviceSelectWindow
	: QMainWindow
{
	Q_OBJECT

public:
	VfhGpuDeviceSelectWindow(VfhGpuDeviceSelectWindow* &windowInstance, QWidget *parent)
		: QMainWindow(parent)
		, windowInstance(windowInstance)
	{
		setWindowTitle(tr("V-Ray GPU Device Select"));
		setAttribute(Qt::WA_DeleteOnClose);
		setWindowFlags(Qt::Window|Qt::Tool);
		setWindowModality(Qt::ApplicationModal);

		initUI();

		resize(1024, 768);
	}

	~VfhGpuDeviceSelectWindow() VRAY_DTOR_OVERRIDE {
		windowInstance = nullptr;
	}

	void initUI() {
		listCuda = new VfhGpuDeviceListWidget("CUDA", vfhGpuCudaDevices, availableCudaDeviceList, this);
		listOpenCl = new VfhGpuDeviceListWidget("OpenCL", vfhGpuOpenClDevices, availableOpenClDeviceList, this);

		QHBoxLayout *mainLayout = new QHBoxLayout;
		mainLayout->addWidget(listCuda);
		mainLayout->addWidget(listOpenCl);

		QWidget *centralWidget = new QWidget(this);
		centralWidget->setLayout(mainLayout);

		setCentralWidget(centralWidget);
	}

protected:
	VfhGpuDeviceListWidget *listCuda = nullptr;
	VfhGpuDeviceListWidget *listOpenCl = nullptr;

private:
	VfhGpuDeviceSelectWindow* &windowInstance;
};

/// We allow only one window instance.
static VfhGpuDeviceSelectWindow *windowInstance = nullptr;

void VRayForHoudini::getGpuDeviceIdList(int isCUDA, ComputeDeviceIdList &list)
{
	initAvailableDeviceList();

	const QStringList &availableDeviceList =
		isCUDA ? availableCudaDeviceList : availableOpenClDeviceList;

	const char *deviceVarName =
		isCUDA ? vfhGpuCudaDevices : vfhGpuOpenClDevices;

	QStringList storedDeviceList;
	getStoredDeviceList(deviceVarName, storedDeviceList);

	for (int i = 0; i < availableDeviceList.count(); ++i) {
		const QString &availableDeviceName = availableDeviceList[i];
		if (storedDeviceList.contains(availableDeviceName)) {
			list.push_back(i);
		}
	}
}

void VRayForHoudini::showGpuDeviceSelectUI()
{
	initAvailableDeviceList();

	if (!HOU::isUIAvailable())
		return;

	houPalette.init();

	if (!windowInstance) {
		windowInstance = new VfhGpuDeviceSelectWindow(windowInstance, RE_Window::mainQtWindow());
		windowInstance->show();
	}
}

#ifndef Q_MOC_RUN
#include <vfh_gpu_device_select.cpp.moc>
#endif
