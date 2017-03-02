//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_hou_utils.h"
#include "vfh_plugin_exporter.h"

#include <QtGui/QtGui>
#include <boost/bind.hpp>

#define PRINT_CALLBACK_CALLS  0


using namespace VRayForHoudini;
using namespace VRayForHoudini::Attrs;


struct AppSdkInit
{
public:
	static AppSdkInit& getInstance()
	{
		static AppSdkInit instance;
		return instance;
	}

	operator bool () const
	{
		return (m_vrayInit != nullptr);
	}

private:
	// don't allow public construct/destruct
	AppSdkInit():
		m_vrayInit(nullptr)
	{
		try {
#ifdef __APPLE__
			bool VFBEnabled = false;
#else
			bool VFBEnabled = HOU::isUIAvailable();
#endif
			m_vrayInit = new VRay::VRayInit(VFBEnabled);
			if (!m_vrayInit) {
				throw VRay::VRayException("Unable to initialize V-Ray AppSDK.");
			}

			VRay::RendererOptions options;
			options.enableFrameBuffer = false;
			options.showFrameBuffer = false;
			m_dummyRenderer = new VRay::VRayRenderer(options);

			Log::getLog().info("Using V-Ray AppSDK %s", VRay::getSDKVersionDetails());
			Log::getLog().info("Using V-Ray %s", VRay::getVRayVersionDetails());
		}
		catch (VRay::VRayException &e) {
			Log::getLog().error("Error while initializing V-Ray AppSDK library:\n%s",
								e.what());
			cleanup();
		}
	}

	~AppSdkInit()
	{
		Log::getLog().debug("~AppSdkInit()");
		cleanup();
	}

	void cleanup()
	{
		FreePtr(m_dummyRenderer);
		FreePtr(m_vrayInit);
	}

	// disable copy & assignment
	AppSdkInit(AppSdkInit const&);
	AppSdkInit& operator=(AppSdkInit const&);

private:
	// needed to initialize V-Ray renderer context
	VRay::VRayInit       *m_vrayInit;
	// dummy renderer keeps references to shared plugin libraries
	// when resetting/removing/destroying true renderers
	// dummy renderer instance will prevent unloading of shared plugin libraries
	VRay::VRayRenderer   *m_dummyRenderer;
};


static void OnDumpMessage(VRay::VRayRenderer &renderer, const char *msg, int level, void *userData)
{
	CbSetOnDumpMessage *callbacks = reinterpret_cast<CbSetOnDumpMessage*>(userData);
	for (CbSetOnDumpMessage::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer, msg, level);
	}
	for (CbSetOnDumpMessage::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnProgress(VRay::VRayRenderer &renderer, const char *msg, int elementNumber, int elementsCount, void *userData)
{
	CbSetOnProgress *callbacks = reinterpret_cast<CbSetOnProgress*>(userData);
	for (CbSetOnProgress::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer, msg, elementNumber, elementsCount);
	}
	for (CbSetOnProgress::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnRendererClose(VRay::VRayRenderer &renderer, void *userData)
{
#if PRINT_CALLBACK_CALLS
	Log::getLog().warning("VRayPluginRenderer::OnRendererClose()");
#endif
	CbSetOnRendererClose *callbacks = reinterpret_cast<CbSetOnRendererClose*>(userData);
	for (CbSetOnRendererClose::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer);
	}
	for (CbSetOnRendererClose::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnImageReady(VRay::VRayRenderer &renderer, void *userData)
{
#if PRINT_CALLBACK_CALLS
	Log::getLog().warning("VRayPluginRenderer::OnImageReady()");
#endif
	CbSetOnImageReady *callbacks = reinterpret_cast<CbSetOnImageReady*>(userData);
	for (CbSetOnImageReady::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer);
	}
	for (CbSetOnImageReady::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnRTImageUpdated(VRay::VRayRenderer &renderer, VRay::VRayImage *img , void *userData)
{
#if PRINT_CALLBACK_CALLS
	Log::getLog().warning("VRayPluginRenderer::OnRTImageUpdated()");
#endif
	CbSetOnRTImageUpdated *callbacks = reinterpret_cast<CbSetOnRTImageUpdated*>(userData);
	for (CbSetOnRTImageUpdated::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer, img);
	}
	for (CbSetOnRTImageUpdated::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnBucketInit(VRay::VRayRenderer &renderer, int x, int y, int w, int h, const char *host, void *userData)
{
#if PRINT_CALLBACK_CALLS
	Log::getLog().warning("VRayPluginRenderer::OnBucketReady()");
#endif
	CbSetOnBucketInit *callbacks = reinterpret_cast<CbSetOnBucketInit*>(userData);
	for (CbSetOnBucketInit::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer, x, y, w, h, host);
	}
	for (CbSetOnBucketInit::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnBucketFailed(VRay::VRayRenderer &renderer, int x, int y, int w, int h, const char *host, void *userData)
{
#if PRINT_CALLBACK_CALLS
	Log::getLog().warning("VRayPluginRenderer::OnBucketReady()");
#endif
	CbSetOnBucketFailed *callbacks = reinterpret_cast<CbSetOnBucketFailed*>(userData);
	for (CbSetOnBucketFailed::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer, x, y, w, h, host);
	}
	for (CbSetOnBucketFailed::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnBucketReady(VRay::VRayRenderer &renderer, int x, int y, const char *host, VRay::VRayImage *img, void *userData)
{
#if PRINT_CALLBACK_CALLS
	Log::getLog().warning("VRayPluginRenderer::OnBucketReady()");
#endif
	CbSetOnBucketReady *callbacks = reinterpret_cast<CbSetOnBucketReady*>(userData);
	for (CbSetOnBucketReady::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer, x, y, host, img);
	}
	for (CbSetOnBucketReady::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


bool VRayPluginRenderer::initialize()
{
	return static_cast<bool>(AppSdkInit::getInstance());
}


bool VRayPluginRenderer::hasVRScansGUILicense(VRay::ScannedMaterialLicenseError &err)
{
	if (!initialize()) {
		err.errorCode = VRay::LicenseError::vrlauth_notInitializedError;
		return false;
	}

	static bool isGUIAvailable = false;
	if (isGUIAvailable) {
		err.errorCode = VRay::LicenseError::vrlauth_noError;
		return isGUIAvailable;
	}

	VRay::ScannedMaterialParams dummyParms;
	VRay::IntList dummyData;
	const bool res = VRay::encodeScannedMaterialParams(dummyParms, dummyData, err);
	if (!res) {
		const char *errMsg = (err.error())? err.toString() : "V-Ray AppSDK internal error";
		if (!err.error()) {
			// This should not happen in general but let's adjust the error code just in case
			err.errorCode = VRay::LicenseError::vrlauth_notInitializedError;
		}
		Log::getLog().warning("Unable to obtain VRScans GUI license: %s", errMsg);
	}

	isGUIAvailable = res && !err.error();
	return isGUIAvailable;
}


VRayPluginRenderer::VRayPluginRenderer()
	: m_vray(nullptr)
{
	Log::getLog().debug("VRayPluginRenderer()");
}


VRayPluginRenderer::~VRayPluginRenderer()
{
	Log::getLog().debug("~VRayPluginRenderer()");

	freeMem();
}


int VRayPluginRenderer::initRenderer(int hasUI, int reInit)
{
	// VFB will take colors from QApplication::palette(),
	// but Houdini's real palette is not stored there for some reason.
	QWidget *mainWindow = HOU::getMainQtWindow();
	if (mainWindow) {
		// Must be called before VRay::VRayRenderer(options)
		QApplication::setPalette(mainWindow->palette());
		QApplication::setFont(mainWindow->font());
	}

	if (initialize()) {
		if (reInit) {
			freeMem();
			resetCallbacks();
		}
		else if (m_vray) {
			m_vray->stop();
			m_vray->reset();
		}

		if (!m_vray) {
			try {
				VRay::RendererOptions options;
				options.enableFrameBuffer = hasUI;
				options.showFrameBuffer = false;
				options.useDefaultVfbTheme = false;
				options.vfbDrawStyle = VRay::RendererOptions::ThemeStyleMaya;
				options.keepRTRunning = true;

				m_vray = new VRay::VRayRenderer(options);

				m_vray->setOnDumpMessage(OnDumpMessage,       (void*)&m_callbacks.m_cbOnDumpMessage);
				m_vray->setOnProgress(OnProgress,             (void*)&m_callbacks.m_cbOnProgress);
				m_vray->setOnRendererClose(OnRendererClose,   (void*)&m_callbacks.m_cbOnRendererClose);

				if (hasUI) {
					m_vray->setOnImageReady(OnImageReady,         (void*)&m_callbacks.m_cbOnImageReady);
					m_vray->setOnRTImageUpdated(OnRTImageUpdated, (void*)&m_callbacks.m_cbOnRTImageUpdated);
					m_vray->setOnBucketInit(OnBucketInit,         (void*)&m_callbacks.m_cbOnBucketInit);
					m_vray->setOnBucketFailed(OnBucketFailed,     (void*)&m_callbacks.m_cbOnBucketFailed);
					m_vray->setOnBucketReady(OnBucketReady,       (void*)&m_callbacks.m_cbOnBucketReady);
				}
			}
			catch (VRay::VRayException &e) {
				Log::getLog().error("Error initializing V-Ray! Error: \"%s\"",
									e.what());
				freeMem();
			}
		}
	}

	return !!(m_vray);
}


void VRayPluginRenderer::freeMem()
{
	Log::getLog().debug("VRayPluginRenderer::freeMem()");

	if (m_vray) {
		m_vray->stop();
		m_vray->setOnImageReady(NULL);
		m_vray->setOnDumpMessage(NULL);
		m_vray->setOnProgress(NULL);
		m_vray->setOnRendererClose(NULL);
		m_vray->setOnImageReady(NULL);
		m_vray->setOnRTImageUpdated(NULL);
		m_vray->setOnBucketInit(NULL);
		m_vray->setOnBucketFailed(NULL);
		m_vray->setOnBucketReady(NULL);
	}

	FreePtr(m_vray);
}


void VRayPluginRenderer::setImageSize(const int w, const int h)
{
	if (m_vray) {
		m_vray->setImageSize(w, h);
	}
}


void VRayPluginRenderer::showVFB(bool show, const char *title)
{
	if (   !m_vray
		|| !m_vray->getOptions().enableFrameBuffer
		|| show == m_vray->vfb.isShown() )
	{
		return;
	}

	QWidget *mainWindow = HOU::getMainQtWindow();
	if (mainWindow) {
		// first set VFB parent
		if (show) {
			m_vray->vfb.setParentWindow(mainWindow);
		}

		m_vray->vfb.show(show, show);

		// last set VFB window flags to float on top of main Houdini window.
		// Flags and title should be set after first show of the VFB as
		// its window handle does not exists beforehand
		if (show) {
			QWidget *vfb = reinterpret_cast<QWidget*>(m_vray->vfb.getWindowHandle());
			if (vfb) {
				Qt::WindowFlags windowFlags = vfb->windowFlags();
				windowFlags |= (  Qt::Window
#ifndef _WIN32
								| Qt::WindowStaysOnTopHint
#endif
								| Qt::WindowTitleHint
								| Qt::WindowMinMaxButtonsHint
								| Qt::WindowCloseButtonHint);
				vfb->setWindowFlags(windowFlags);
				m_vray->vfb.setTitlePrefix(title);
			}
		}
	}
}


VRay::Plugin VRayPluginRenderer::getPlugin(const char *pluginName)
{
	if (!m_vray) {
		return VRay::Plugin();
	}

	return m_vray->getPlugin(pluginName);
}


VRay::Plugin VRayPluginRenderer::exportPlugin(const Attrs::PluginDesc &pluginDesc)
{
#define CGR_DEBUG_APPSDK_VALUES  0
	if (NOT(m_vray)) {
		return VRay::Plugin();
	}

	if (pluginDesc.pluginID.empty()) {
		// NOTE: Could be done intentionally to skip plugin creation
		Log::getLog().warning("[%s] PluginDesc.pluginID is not set!",
							  pluginDesc.pluginName.c_str());
		return VRay::Plugin();
	}

	VRay::Plugin plug = m_vray->getOrCreatePlugin(pluginDesc.pluginName, pluginDesc.pluginID);
	if (!plug) {
		VRay::Error err = m_vray->getLastError();
		if (err != VRay::SUCCESS) {
			Log::getLog().error("Error creating plugin: %s",
								err.toString());
		}
	}
	else {
		exportPluginProperties(plug, pluginDesc);
	}

	return plug;
}


void VRayPluginRenderer::exportPluginProperties(VRay::Plugin &plugin, const Attrs::PluginDesc &pluginDesc)
{
	for (const auto &pIt : pluginDesc.pluginAttrs) {
		const PluginAttr &p = pIt;

		if (p.paramType == PluginAttr::AttrTypeIgnore) {
			continue;
		}

		if (p.paramType == PluginAttr::AttrTypeInt) {
			plugin.setValue(p.paramName, p.paramValue.valInt);
		}
		else if (p.paramType == PluginAttr::AttrTypeFloat) {
			plugin.setValue(p.paramName, p.paramValue.valFloat);
		}
		else if (p.paramType == PluginAttr::AttrTypeColor) {
			plugin.setValue(p.paramName, VRay::Color(p.paramValue.valVector[0], p.paramValue.valVector[1], p.paramValue.valVector[2]));
		}
		else if (p.paramType == PluginAttr::AttrTypeVector) {
			plugin.setValue(p.paramName, VRay::Vector(p.paramValue.valVector[0], p.paramValue.valVector[1], p.paramValue.valVector[2]));
		}
		else if (p.paramType == PluginAttr::AttrTypeAColor) {
			plugin.setValue(p.paramName, VRay::AColor(p.paramValue.valVector[0], p.paramValue.valVector[1], p.paramValue.valVector[2], p.paramValue.valVector[3]));
		}
		else if (p.paramType == PluginAttr::AttrTypePlugin) {
			if (p.paramValue.valPluginOutput.empty()) {
				plugin.setValue(p.paramName, p.paramValue.valPlugin);
			}
			else {
				plugin.setValue(p.paramName, p.paramValue.valPlugin, p.paramValue.valPluginOutput);
			}
		}
		else if (p.paramType == PluginAttr::AttrTypeTransform) {
			plugin.setValue(p.paramName, p.paramValue.valTransform);
		}
		else if (p.paramType == PluginAttr::AttrTypeMatrix) {
			plugin.setValue(p.paramName, p.paramValue.valTransform.matrix);
		}
		else if (p.paramType == PluginAttr::AttrTypeString) {
			plugin.setValue(p.paramName, p.paramValue.valString);
#if CGR_DEBUG_APPSDK_VALUES
			Log::getLog().debug("AttrTypeString:  %s [%s] = %s",
								p.paramName.c_str(), plug.getType().c_str(), plug.getValueAsString(p.paramName).c_str());
#endif
		}
		else if (p.paramType == PluginAttr::AttrTypeListPlugin) {
			plugin.setValue(p.paramName, VRay::Value(p.paramValue.valListValue));
		}
		else if (p.paramType == PluginAttr::AttrTypeListInt) {
			plugin.setValue(p.paramName, VRay::Value(p.paramValue.valListInt));
#if CGR_DEBUG_APPSDK_VALUES
			Log::getLog().debug("AttrTypeListInt:  %s [%s] = %s",
								p.paramName.c_str(), plug.getType().c_str(), plug.getValueAsString(p.paramName).c_str());
#endif
		}
		else if (p.paramType == PluginAttr::AttrTypeListFloat) {
			plugin.setValue(p.paramName, VRay::Value(p.paramValue.valListFloat));
#if CGR_DEBUG_APPSDK_VALUES
			Log::getLog().debug("AttrTypeListFloat:  %s [%s] = %s",
								p.paramName.c_str(), plug.getType().c_str(), plug.getValueAsString(p.paramName).c_str());
#endif
		}
		else if (p.paramType == PluginAttr::AttrTypeListVector) {
			plugin.setValue(p.paramName, VRay::Value(p.paramValue.valListVector));
		}
		else if (p.paramType == PluginAttr::AttrTypeListColor) {
			plugin.setValue(p.paramName, VRay::Value(p.paramValue.valListColor));
		}
		else if (p.paramType == PluginAttr::AttrTypeListValue) {
			plugin.setValue(p.paramName, VRay::Value(p.paramValue.valListValue));
#if CGR_DEBUG_APPSDK_VALUES
			Log::getLog().debug("AttrTypeListValue:  %s [%s] = %s",
								p.paramName.c_str(), plug.getType().c_str(), plug.getValueAsString(p.paramName).c_str());
#endif
		}
		else if (p.paramType == PluginAttr::AttrTypeRawListInt) {
			plugin.setValue(p.paramName, p.paramValue.valRawListInt);
		}
		else if (p.paramType == PluginAttr::AttrTypeRawListFloat) {
			plugin.setValue(p.paramName, p.paramValue.valRawListFloat);
		}
		else if (p.paramType == PluginAttr::AttrTypeRawListVector) {
			plugin.setValue(p.paramName, p.paramValue.valRawListVector);
		}
		else if (p.paramType == PluginAttr::AttrTypeRawListColor) {
			plugin.setValue(p.paramName, p.paramValue.valRawListColor);
		}
		else if (p.paramType == PluginAttr::AttrTypeRawListCharString) {
			plugin.setValue(p.paramName, p.paramValue.valRawListCharString);
		}
		else if (p.paramType == PluginAttr::AttrTypeRawListValue) {
			plugin.setValue(p.paramName, p.paramValue.valRawListValue);
		}

#if CGR_DEBUG_APPSDK_VALUES
		Log::getLog().debug("Setting plugin parameter: \"%s\" %s_%s = %s",
							pluginDesc.pluginName.c_str(), pluginDesc.pluginID.c_str(), p.paramName.c_str(), plug.getValue(p.paramName).toString().c_str());
#endif
	}
}


void VRayPluginRenderer::commit()
{
	if (m_vray) {
		m_vray->commit();
	}
}


void VRayPluginRenderer::setCamera(VRay::Plugin camera)
{
	if (m_vray) {
		m_vray->setCamera(camera);
	}
}


void VRayPluginRenderer::setRendererMode(int mode)
{
	if (m_vray) {
		VRay::RendererOptions::RenderMode renderMode = static_cast<VRay::RendererOptions::RenderMode>(mode);

		if (renderMode >= VRay::RendererOptions::RENDER_MODE_RT_CPU) {
			VRay::RendererOptions options = m_vray->getOptions();
			options.numThreads = VUtils::getNumProcessors() - 1;
			options.keepRTRunning = true;

			m_vray->setOptions(options);
		}

		m_vray->setRenderMode(renderMode);
	}
}


void VRayPluginRenderer::removePlugin(const Attrs::PluginDesc &pluginDesc)
{
	removePlugin(pluginDesc.pluginName);
}


void VRayPluginRenderer::removePlugin(const std::string &pluginName)
{
	if (m_vray) {
		VRay::Plugin plugin = m_vray->getPlugin(pluginName);
		if (!plugin) {
			Log::getLog().warning("VRayPluginRenderer::removePlugin: Plugin \"%s\" is not found!",
								  pluginName.c_str());
		}
		else {
			m_vray->removePlugin(plugin);

			VRay::Error err = m_vray->getLastError();
			if (err != VRay::SUCCESS) {
				Log::getLog().error("Error removing plugin: %s",
									err.toString());
			}
		}
	}
}

int VRayPluginRenderer::exportScene(const std::string &filepath, VRay::VRayExportSettings &settings)
{
	int res = 0;

	if (HOU::isIndie()) {
		Log::getLog().error("Export to the *.vrscene files is not allowed in Houdini Indie");
	}
	else if (m_vray) {
		Log::getLog().info("Starting export to \"%s\"...", filepath.c_str());

		res = m_vray->exportScene(filepath.c_str(), settings);

		VRay::Error err = m_vray->getLastError();
		if (err != VRay::SUCCESS) {
			Log::getLog().error("Error exporting scene: %s",
								err.toString());
		}
	}

	return res;
}


int VRayPluginRenderer::startRender(int locked)
{
	if (m_vray) {
		Log::getLog().info("Starting render for frame %.3f...", m_vray->getCurrentTime());

		m_vray->start();

		if (locked) {
			m_vray->waitForImageReady();
		}
	}

	return 0;
}


int VRayPluginRenderer::startSequence(int start, int end, int step, int locked)
{
	if (m_vray) {
		Log::getLog().info("Starting sequence render (%i-%i,%i)...", start, end, step);

		VRay::SubSequenceDesc seq;
		seq.start = start;
		seq.end   = end;
		seq.step  = step;

		m_vray->renderSequence(&seq, 1);
		if (locked) {
			m_vray->waitForSequenceDone();
		}
	}

	return 0;
}


void VRayPluginRenderer::stopRender()
{
	if (m_vray) {
		m_vray->stop();
	}
}


void VRayPluginRenderer::resetCallbacks()
{
	m_callbacks.clear();
}


void VRayForHoudini::VRayPluginRenderer::setAnimation(bool on)
{
	if (m_vray) {
		m_vray->useAnimatedValues(on);
	}
}


void VRayForHoudini::VRayPluginRenderer::setCurrentTime(fpreal fframe)
{
	if (m_vray) {
		m_vray->setCurrentTime(fframe);
	}
}


void VRayPluginRenderer::clearFrames(float toTime)
{
	if (m_vray) {
		m_vray->clearAllPropertyValuesUpToTime(toTime);
	}
}


void VRayPluginRenderer::setAutoCommit(const bool enable)
{
	if (m_vray) {
		m_vray->setAutoCommit(enable);
	}
}
