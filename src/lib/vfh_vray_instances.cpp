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

#include "vfh_vray.h"
#include "vfh_log.h"
#include "vfh_vray_instances.h"
#include "vfh_hou_utils.h"

#include <algorithm>

#include <QSharedMemory>
#include <QApplication>
#include <QWidget>

using namespace VRayForHoudini;

const QString &memKey = QString("VRayInstances%1").arg(QCoreApplication::applicationPid());
static const int maxInstances = 256;
static QRegExp cssRgbMatch("(\\d+)");

struct InstancesStorage {
	InstancesStorage()
		: instanceCount(0)
		, vrayInit(nullptr)
		, dummyRenderer(nullptr)
	{
		memset(vrayInstances, 0, sizeof(vrayInstances));
	}

	/// Number of allocated renderers inside vrayInstances
	int instanceCount;
	/// Pointer to the appsdk vray context
	VRay::VRayInit* vrayInit;
	/// Pointer to one dummy renderer, if instanceCount == 0 this must be point to valid renderer
	VRay::VRayRenderer* dummyRenderer;
	/// All vray renderer that would exist are stored here
	VRay::VRayRenderer* vrayInstances[maxInstances];
};

static const int memSize = sizeof(InstancesStorage);

static QSharedMemory vrayInstances(memKey);

static QColor getSelectionColor(const QString &styleSheet)
{
	QColor selectionColor;

	// Find the first occurence of "selection-background-color: rgb(184, 133, 32);"
	QStringList lines = styleSheet.split('\n');
	for (QString line : lines) {
		if (line.contains("selection-background-color")) {
			QStringList colors;
			int pos = 0;

			while ((pos = cssRgbMatch.indexIn(line, pos)) != -1) {
				colors << cssRgbMatch.cap(1);
				pos += cssRgbMatch.matchedLength();
			}

			selectionColor.setRed(colors[0].toInt());
			selectionColor.setGreen(colors[1].toInt());
			selectionColor.setBlue(colors[2].toInt());

			break;
		}
	}

	return selectionColor;
}

/// VFB will take colors from QApplication::palette(),
/// but Houdini's real palette is not stored there.
/// Must be called before VRay::VRayInit().
static void initVFBTheme()
{
	QWidget *mainWindow = HOU::getMainQtWindow();
	if (mainWindow) {
		QPalette houdiniPalette = mainWindow->palette();

		// Pressed button color is incorrect in palette...
		QString styleSheet = mainWindow->styleSheet();

		houdiniPalette.setBrush(QPalette::Highlight,
								QBrush(getSelectionColor(styleSheet)));

		QApplication::setPalette(houdiniPalette);

		QApplication::setFont(mainWindow->font());
	}
}

static void clearSharedMemory()
{
	::memset(vrayInstances.data(), 0, memSize);
}

static int getCreateSharedMemory()
{
	if (vrayInstances.isAttached())
		return 1;

	if (vrayInstances.create(memSize)) {
		Log::getLog().debug("InstancesStorage: \"%s\"", memKey.toLocal8Bit().constData());
		clearSharedMemory();
	}
	else {
		Log::getLog().debug("InstancesStorage: %s",
							vrayInstances.errorString().toLocal8Bit().constData());
		switch (vrayInstances.error()) {
			case QSharedMemory::NoError:
			case QSharedMemory::AlreadyExists: {
				vrayInstances.attach();
				break;
			}
			default: {
				Log::getLog().error("InstancesStorage error: %s",
									vrayInstances.errorString().toLocal8Bit().constData());
				return 0;
			}
		}
	}

	return 1;
}

int VRayForHoudini::newVRayInit()
{
	Log::getLog().debug("newVRayInit()");

	getCreateSharedMemory();
	
	vassert(vrayInstances.isAttached());

	int vrayInitExist = false;

	if (vrayInstances.lock()) {
		InstancesStorage *is = reinterpret_cast<InstancesStorage*>(vrayInstances.data());
		if (is->vrayInit) {
			vrayInitExist = true;
		}
		else {
			initVFBTheme();

			try {
				is->vrayInit = new VRay::VRayInit(true);
				vrayInitExist = true;

				Log::getLog().info("Using V-Ray AppSDK %s", VRay::getSDKVersionDetails());

				VRay::RendererOptions options;
				options.enableFrameBuffer = false;
				options.showFrameBuffer = false;
				options.useDefaultVfbTheme = false;
				options.vfbDrawStyle = VRay::RendererOptions::ThemeStyleMaya;

				is->dummyRenderer = new VRay::VRayRenderer(options);;

				Log::getLog().info("Using V-Ray %s", VRay::getVRayVersionDetails());
			} catch (VRay::InitErr &e) {
				Log::getLog().error("Error initializing V-Ray AppSDK library:\n%s",
									e.what());
			} catch (VRay::InstantiationErr &e) {
				Log::getLog().error("Error initializing VRay::VRayRenderer instance:\n%s",
				                     e.what());
			}

		}

		vrayInstances.unlock();
	}

	return vrayInitExist;
}

void VRayForHoudini::deleteVRayInit()
{
	Log::getLog().debug("deleteVRayInit()");

	if (!vrayInstances.isAttached()) {
		if (!vrayInstances.attach())
			return;
	}

	dumpSharedMemory();

	if (vrayInstances.lock()) {
		InstancesStorage *is = reinterpret_cast<InstancesStorage*>(vrayInstances.data());

		if (is->dummyRenderer) {
			vassert(is->instanceCount == 0 && "Dummy renderer is non NULL and there are instances");
			Log::getLog().debug("Deleting VRayRenderer: 0x%p", is->dummyRenderer);
			FreePtr(is->dummyRenderer);
		}

		for (int i = 0; i < maxInstances && is->instanceCount; ++i) {
			if (is->vrayInstances[i]) {
				Log::getLog().debug("Deleting VRayRenderer: 0x%p", is->vrayInstances[i]);
				FreePtr(is->vrayInstances[i]);
				is->instanceCount--;
			}
		}

		FreePtr(is->vrayInit);

		vrayInstances.unlock();
	}

	vrayInstances.detach();
}

VRay::VRayRenderer* VRayForHoudini::newVRayRenderer(const VRay::RendererOptions &options)
{
	Log::getLog().debug("newVRayRenderer()");

	vassert(vrayInstances.isAttached());

	VRay::VRayRenderer *instance = nullptr;

	if (vrayInstances.lock()) {
		InstancesStorage *is = reinterpret_cast<InstancesStorage*>(vrayInstances.data());

		if (is->dummyRenderer) {
			is->instanceCount++;
			is->dummyRenderer->setOptions(options);
			std::swap(is->dummyRenderer, instance);
		} else {
			// TODO: no point in using exception if we use it like error code return
			try {
				instance = new VRay::VRayRenderer(options);
				for (int i = 0; i < maxInstances; ++i) {
					VRay::VRayRenderer* &vrayInstance = is->vrayInstances[i];
					if (vrayInstance == nullptr) {
						vrayInstance = reinterpret_cast<VRay::VRayRenderer*>(instance);
						break;
					}
				}
			} catch (VRay::VRayException &e) {
				Log::getLog().error("Error initializing VRay::VRayRenderer instance:\n%s",
					e.what());
			}
		}

		vrayInstances.unlock();
	}

	return instance;
}

void VRayForHoudini::deleteVRayRenderer(VRay::VRayRenderer* &instance)
{
	Log::getLog().debug("deleteVRayRenderer()");

	if (!instance)
		return;

	if (!vrayInstances.isAttached()) {
		vrayInstances.attach();
	}

	vassert(vrayInstances.isAttached());

	if (vrayInstances.lock()) {
		InstancesStorage *is = reinterpret_cast<InstancesStorage*>(vrayInstances.data());
		auto iter = std::find(std::begin(is->vrayInstances), std::end(is->vrayInstances), nullptr);
		if (iter == std::end(is->vrayInstances)) {
			Log::getLog().error("Could not find renderer %p to free", instance);
		} else {
			VRay::VRayRenderer *&vrayInstance = *iter;
			// When instanceCount == 0 and there is no dummy renderer then we must keep this one alive
			// so we put it in the dummy pointer
			if (is->instanceCount == 1) {
				Log::getLog().debug("Stashing last renderer to keep references to plugins");
				vassert(!is->dummyRenderer && "There should not be dummy renderer if there are real ones");
				is->instanceCount = 0;

				std::swap(is->dummyRenderer, instance);
				// TODO: if stop and reset do not clear all state from the object then we will need to re-instantiate renderer here
				is->dummyRenderer->stop();
				is->dummyRenderer->reset();
				// Clear this instance from the pool
				vrayInstance = nullptr;
			} else {
				--is->instanceCount;
				FreePtr(instance);
				vrayInstance = nullptr;
			}
		}

		vrayInstances.unlock();
	}
}

void VRayForHoudini::dumpSharedMemory()
{
	Log::getLog().debug("dumpSharedMemory()");

	if (!vrayInstances.isAttached())
		return;

	if (vrayInstances.lock()) {
		InstancesStorage *is = reinterpret_cast<InstancesStorage*>(vrayInstances.data());

		Log::getLog().debug("VRayInit: 0x%p", is->vrayInit);
		Log::getLog().debug("  VRayRenderer: 0x%p", is->dummyRenderer);

		for (int i = 0; i < maxInstances; ++i) {
			if (is->vrayInstances[i]) {
				Log::getLog().debug("  VRayRenderer: 0x%p", is->vrayInstances[i]);
			}
		}

		vrayInstances.unlock();
	}
}
