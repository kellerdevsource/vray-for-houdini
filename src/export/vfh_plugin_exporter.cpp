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
#include "vfh_vray_instances.h"

#include <QtGui>
#include <QWidget>
#include <QApplication>
#include <boost/bind.hpp>

#define PRINT_CALLBACK_CALLS  0


using namespace VRayForHoudini;
using namespace VRayForHoudini::Attrs;

namespace
{
template <typename T>
void CallVoidCallbacks(const CbBase<T> * callbacks)
{
	for (auto & cb : callbacks->m_cbVoid) {
		if (cb) {
			cb();
		}
	}
}

template <typename T, typename ...Args>
void CallCallbacks(const CbBase<T> * callbacks, Args && ...args)
{
	for (auto & cb : callbacks->m_cbTyped) {
		if (cb) {
			cb(std::forward<Args>(args)...);
		}
	}
	CallVoidCallbacks(callbacks);
}

void OnDumpMessage(VRay::VRayRenderer &renderer, const char *msg, int level, void *userData)
{
	CbSetOnDumpMessage *callbacks = reinterpret_cast<CbSetOnDumpMessage*>(userData);
	CallCallbacks(callbacks, renderer, msg, level);
}


void OnProgress(VRay::VRayRenderer &renderer, const char *msg, int elementNumber, int elementsCount, void *userData)
{
	CbSetOnProgress *callbacks = reinterpret_cast<CbSetOnProgress*>(userData);
	CallCallbacks(callbacks, renderer, msg, elementNumber, elementsCount);
}


void OnRendererClose(VRay::VRayRenderer &renderer, void *userData)
{
#if PRINT_CALLBACK_CALLS
	Log::getLog().warning("VRayPluginRenderer::OnRendererClose()");
#endif
	CbSetOnRendererClose *callbacks = reinterpret_cast<CbSetOnRendererClose*>(userData);
	CallCallbacks(callbacks, renderer);
}


void OnImageReady(VRay::VRayRenderer &renderer, void *userData)
{
#if PRINT_CALLBACK_CALLS
	Log::getLog().warning("VRayPluginRenderer::OnImageReady()");
#endif
	CbSetOnImageReady *callbacks = reinterpret_cast<CbSetOnImageReady*>(userData);
	CallCallbacks(callbacks, renderer);
}


void OnRTImageUpdated(VRay::VRayRenderer &renderer, VRay::VRayImage *img , void *userData)
{
#if PRINT_CALLBACK_CALLS
	Log::getLog().warning("VRayPluginRenderer::OnRTImageUpdated()");
#endif
	CbSetOnRTImageUpdated *callbacks = reinterpret_cast<CbSetOnRTImageUpdated*>(userData);
	CallCallbacks(callbacks, renderer, img);
}


void OnBucketInit(VRay::VRayRenderer &renderer, int x, int y, int w, int h, const char *host, void *userData)
{
#if PRINT_CALLBACK_CALLS
	Log::getLog().warning("VRayPluginRenderer::OnBucketReady()");
#endif
	CbSetOnBucketInit *callbacks = reinterpret_cast<CbSetOnBucketInit*>(userData);
	CallCallbacks(callbacks, renderer, x, y, w, h, host);
}


void OnBucketFailed(VRay::VRayRenderer &renderer, int x, int y, int w, int h, const char *host, void *userData)
{
#if PRINT_CALLBACK_CALLS
	Log::getLog().warning("VRayPluginRenderer::OnBucketReady()");
#endif
	CbSetOnBucketFailed *callbacks = reinterpret_cast<CbSetOnBucketFailed*>(userData);
	CallCallbacks(callbacks, renderer, x, y, w, h, host);
}


void OnBucketReady(VRay::VRayRenderer &renderer, int x, int y, const char *host, VRay::VRayImage *img, void *userData)
{
#if PRINT_CALLBACK_CALLS
	Log::getLog().warning("VRayPluginRenderer::OnBucketReady()");
#endif
	CbSetOnBucketReady *callbacks = reinterpret_cast<CbSetOnBucketReady*>(userData);
	CallCallbacks(callbacks, renderer, x, y, host, img);
}

}

bool VRayPluginRenderer::hasVRScansGUILicense(VRay::ScannedMaterialLicenseError &err)
{
	if (!newVRayInit()) {
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
	if (!newVRayInit())
		return 0;

	if (reInit) {
		freeMem();
		resetCallbacks();
	}
	else if (m_vray) {
		// Workaround to make render regions persist trough renders
		m_savedRegion.saved = false;

		const bool gotRegion = m_vray->getRenderRegion(m_savedRegion.left, m_savedRegion.top, m_savedRegion.width, m_savedRegion.height);
		int width = 0, height = 0;
		if (gotRegion && m_vray->getImageSize(width, height)) {
			if (m_savedRegion.width != width || m_savedRegion.height != height) {
				m_savedRegion.saved = true;
			}
		}

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
			options.rtNoiseThreshold = 0.0f;
			options.rtSampleLevel = INT_MAX;
			options.rtTimeout = 0;

			m_vray = newVRayRenderer(options);
		}
		catch (VRay::VRayException &e) {
			Log::getLog().error("Error initializing V-Ray! Error: \"%s\"",
								e.what());
			freeMem();
		}

		if (m_vray) {
			m_vray->setOnDumpMessage(OnDumpMessage,       &m_callbacks.m_cbOnDumpMessage);
			m_vray->setOnProgress(OnProgress,             &m_callbacks.m_cbOnProgress);
			m_vray->setOnRendererClose(OnRendererClose,   &m_callbacks.m_cbOnRendererClose);

			m_vray->setOnImageReady([](VRay::VRayRenderer &renderer, void *context) {
				VRayPluginRenderer * self = reinterpret_cast<VRayPluginRenderer*>(context);
				if (self->m_vray->getOptions().showFrameBuffer) {
					OnImageReady(renderer, &self->m_callbacks.m_cbOnImageReady);
				}

				// Stopping rendering should clear scene data from memory
				if (renderer.isAborted()) {
					self->m_vray->reset();
				}
			}, this);

			if (hasUI) {
				m_vray->setOnRTImageUpdated(OnRTImageUpdated, &m_callbacks.m_cbOnRTImageUpdated);
				m_vray->setOnBucketInit(OnBucketInit,         &m_callbacks.m_cbOnBucketInit);
				m_vray->setOnBucketFailed(OnBucketFailed,     &m_callbacks.m_cbOnBucketFailed);
				m_vray->setOnBucketReady(OnBucketReady,       &m_callbacks.m_cbOnBucketReady);
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
	}

	deleteVRayRenderer(m_vray);
}


void VRayPluginRenderer::setImageSize(const int w, const int h)
{
	if (m_vray) {
		m_vray->setImageSize(w, h);
	}
}


void VRayPluginRenderer::showVFB(bool show, const char *title)
{
	if (!m_vray ||
		!m_vray->getOptions().enableFrameBuffer ||
		!HOU::isUIAvailable())
	{
		return;
	}

	VRay::VRayRenderer::VFB &vfb = m_vray->vfb;

	if (!show) {
		vfb.show(false, false);
	}
	else {
		if (!vfb.isShown()) {
			QWidget *mainWindow = HOU::getMainQtWindow();
			vassert(mainWindow);

			vfb.setParentWindow(mainWindow);
			vfb.show(show, show);

			QWidget *vfbWindow = reinterpret_cast<QWidget*>(m_vray->vfb.getWindowHandle());
			vassert(vfbWindow);

			Qt::WindowFlags windowFlags = vfbWindow->windowFlags();
			windowFlags |= Qt::Window |
// Need to set this flag only for Linux / Qt4
#if !defined(_WIN32) && (UT_MAJOR_VERSION_INT < 16)
			               Qt::WindowStaysOnTopHint |
#endif
			               Qt::WindowTitleHint |
			               Qt::WindowMinMaxButtonsHint |
			               Qt::WindowCloseButtonHint;
			vfbWindow->setWindowFlags(windowFlags);
		}

		vassert(vfb.isShown());

		vfb.setTitlePrefix(title);
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
	FOR_CONST_IT (PluginAttrs, pIt, pluginDesc.pluginAttrs) {
		const PluginAttr &p = pIt.data();

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
		Log::getLog().debug("Setting plugin parameter: \"%s\" %s.%s = %s",
							pluginDesc.pluginName.c_str(), pluginDesc.pluginID.c_str(), p.paramName.c_str(), plugin.getValue(p.paramName).toString().c_str());
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
			options.rtNoiseThreshold = 0.0f;
			options.rtSampleLevel = INT_MAX;
			options.rtTimeout = 0;

			m_vray->setOptions(options);
		}

		m_vray->setRenderMode(renderMode);
	}
}


void VRayPluginRenderer::removePlugin(const Attrs::PluginDesc &pluginDesc)
{
	removePlugin(pluginDesc.pluginName);
}


void VRayPluginRenderer::removePlugin(VRay::Plugin plugin)
{
	vassert(m_vray);

	m_vray->removePlugin(plugin);
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

		// TODO: unhack this when appsdk saves render regions after m_vray->reset()
		if (m_savedRegion.isValid()) {
			m_vray->setRenderRegion(m_savedRegion.left, m_savedRegion.top, m_savedRegion.width, m_savedRegion.height);
		}

		m_vray->startSync();

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

void VRayPluginRenderer::reset() const
{
	if (!m_vray)
		return;

	m_vray->stop();
	m_vray->reset();
}
