//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_hou_utils.h"
#include "vfh_plugin_exporter.h"
#include "vfh_vray_instances.h"
#include "vfh_gpu_device_select.h"
#include "vfh_log.h"

#include <QWidget>

using namespace VRayForHoudini;
using namespace Attrs;

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

	const VRay::ScannedMaterialParams dummyParms;
	VRay::IntList dummyData;
	const bool res = VRay::encodeScannedMaterialParams(dummyParms, dummyData, err);
	if (!res) {
		const char *errMsg = err.error() ? err.toString() : "V-Ray AppSDK internal error";
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
{
	Log::getLog().debug("VRayPluginRenderer()");
}


VRayPluginRenderer::~VRayPluginRenderer()
{
	Log::getLog().debug("~VRayPluginRenderer()");
	freeMem();
}

int VRayPluginRenderer::initRenderer(int enableVFB, int /*reInit*/)
{
	if (!newVRayInit())
		return 0;
#if 0
	if (reInit) {
		freeMem();
	}
#endif

	m_enableVFB = enableVFB;

	if (m_vray) {
		reset();
	}
	else {
		try {
			const VRay::RendererOptions options = getDefaultOptions(m_enableVFB);
			m_vray = newVRayRenderer(options);
		}
		catch (VRay::VRayException &e) {
			Log::getLog().error("Error initializing V-Ray! Error: \"%s\"",
								e.what());
			freeMem();
		}
	}

	return !!m_vray;
}


void VRayPluginRenderer::freeMem()
{
	if (!m_vray)
		return;

	Log::getLog().debug("VRayPluginRenderer::freeMem()");

	unbindCallbacks(true);

	reset();

	deleteVRayRenderer(m_vray);
}


void VRayPluginRenderer::setImageSize(const int w, const int h)
{
	if (!m_vray)
		return;

	m_vray->setImageSize(w, h);
}


void VRayPluginRenderer::showVFB(bool show, const char *title)
{
	if (!m_vray || !m_enableVFB || !HOU::isUIAvailable()) {
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
#ifndef _WIN32
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
			               Qt::WindowStaysOnTopHint |
#endif
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
	if (!m_vray) {
		return VRay::Plugin();
	}

	if (pluginDesc.pluginID.isEmpty()) {
		// NOTE: Could be done intentionally to skip plugin creation
		Log::getLog().debug("[%s] PluginDesc.pluginID is not set!",
		                    qPrintable(pluginDesc.pluginName));
		return VRay::Plugin();
	}

	VRay::Plugin plug = m_vray->getOrCreatePlugin(qPrintable(pluginDesc.pluginName),
	                                              qPrintable(pluginDesc.pluginID));
	if (plug.isEmpty()) {
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
		const PluginAttr &p = pIt.value();

		if (p.paramType == AttrTypeIgnore)
			continue;

		const std::string paramName(p.paramName.toStdString());
		
		int setValueRes = true;

		if (p.paramType == AttrTypeInt) {
			setValueRes = plugin.setValue(paramName, p.paramValue.valInt);
		}
		else if (p.paramType == AttrTypeFloat) {
			setValueRes = plugin.setValue(paramName, p.paramValue.valVector[0]);
		}
		else if (p.paramType == AttrTypeColor) {
			setValueRes = plugin.setValue(paramName, VRay::Color(p.paramValue.valVector[0], p.paramValue.valVector[1], p.paramValue.valVector[2]));
		}
		else if (p.paramType == AttrTypeVector) {
			setValueRes = plugin.setValue(paramName, VRay::Vector(p.paramValue.valVector[0], p.paramValue.valVector[1], p.paramValue.valVector[2]));
		}
		else if (p.paramType == AttrTypeAColor) {
			setValueRes = plugin.setValue(paramName, VRay::AColor(p.paramValue.valVector[0], p.paramValue.valVector[1], p.paramValue.valVector[2], p.paramValue.valVector[3]));
		}
		else if (p.paramType == AttrTypePlugin) {
			if (p.paramValue.valPluginOutput.isEmpty()) {
				setValueRes = plugin.setValue(paramName, p.paramValue.valPlugin);
			}
			else {
				setValueRes = plugin.setValue(paramName, VRay::PluginRef(p.paramValue.valPlugin, qPrintable(p.paramValue.valPluginOutput)));
			}
		}
		else if (p.paramType == AttrTypeTransform) {
			setValueRes = plugin.setValue(paramName, p.paramValue.valTransform);
		}
		else if (p.paramType == AttrTypeMatrix) {
			setValueRes = plugin.setValue(paramName, p.paramValue.valTransform.matrix);
		}
		else if (p.paramType == AttrTypeString) {
			setValueRes = plugin.setValue(paramName, p.paramValue.valString.toStdString());
		}
		else if (p.paramType == AttrTypeRawListInt) {
			setValueRes = plugin.setValue(paramName, p.paramValue.valRawListInt);
		}
		else if (p.paramType == AttrTypeRawListFloat) {
			setValueRes = plugin.setValue(paramName, p.paramValue.valRawListFloat);
		}
		else if (p.paramType == AttrTypeRawListVector) {
			setValueRes = plugin.setValue(paramName, p.paramValue.valRawListVector);
		}
		else if (p.paramType == AttrTypeRawListColor) {
			setValueRes = plugin.setValue(paramName, p.paramValue.valRawListColor);
		}
		else if (p.paramType == AttrTypeRawListCharString) {
			setValueRes = plugin.setValue(paramName, p.paramValue.valRawListCharString);
		}
		else if (p.paramType == AttrTypeRawListValue) {
			const bool curAnimValue = m_vray->getUseAnimatedValuesState();

			// Force animated generic list key-frames.
			if (curAnimValue && !p.paramValue.isAnimatedGenericList) {
				m_vray->useAnimatedValues(false);
			}

			setValueRes = plugin.setValue(paramName, p.paramValue.valRawListValue);

			m_vray->useAnimatedValues(curAnimValue);
		}

		if (!setValueRes) {
			const VRay::Error err = m_vray->getLastError();
			if (err != VRay::SUCCESS) {
				Log::getLog().error("Error setting parameter %s::%s: \"%s\"",
				                    qPrintable(pluginDesc.pluginID), paramName.c_str(), err.toString());
			}
		}
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

/// Sets plugin parameters from SettingsRTEngine.
/// @param self Settings.
/// @param plugin SettingsRTEngine plugin instance.
static void fillSettingsRTEnginePlugin(const SettingsRTEngine &self, VRay::Plugin plugin)
{
	plugin.setValue("coherent_tracing", self.coherent_tracing);
	plugin.setValue("cpu_bundle_size", self.cpu_bundle_size);
	plugin.setValue("cpu_samples_per_pixel", self.cpu_samples_per_pixel);
	plugin.setValue("disable_render_elements", self.disable_render_elements);
	plugin.setValue("enable_cpu_interop", self.enable_cpu_interop);
	plugin.setValue("enable_mask", self.enable_mask);
	plugin.setValue("gi_depth", self.gi_depth);
	plugin.setValue("gpu_bundle_size", self.gpu_bundle_size);
	plugin.setValue("gpu_samples_per_pixel", self.gpu_samples_per_pixel);
	plugin.setValue("low_gpu_thread_priority", self.low_gpu_thread_priority);
	plugin.setValue("max_draw_interval", self.max_draw_interval);
	plugin.setValue("max_render_time", self.max_render_time);
	plugin.setValue("max_sample_level", self.max_sample_level);
	plugin.setValue("min_draw_interval", self.min_draw_interval);
	plugin.setValue("noise_threshold", self.noise_threshold);
	plugin.setValue("opencl_resizeTextures", self.opencl_resizeTextures);
	plugin.setValue("opencl_texsize", self.opencl_texsize);
	plugin.setValue("opencl_textureFormat", self.opencl_textureFormat);
	plugin.setValue("progressive_samples_per_pixel", self.progressive_samples_per_pixel);
	plugin.setValue("stereo_eye_distance", self.stereo_eye_distance);
	plugin.setValue("stereo_focus", self.stereo_focus);
	plugin.setValue("stereo_mode", self.stereo_mode);
	plugin.setValue("trace_depth", self.trace_depth);
	plugin.setValue("undersampling", self.undersampling);
}

void VRayPluginRenderer::setRendererMode(const SettingsRTEngine &settingsRTEngine, VRay::VRayRenderer::RenderMode mode)
{
	if (!m_vray)
		return;

	const int isInteractiveMode =
		mode >= VRay::VRayRenderer::RENDER_MODE_INTERACTIVE &&
		mode <= VRay::VRayRenderer::RENDER_MODE_INTERACTIVE_CUDA;

	VRay::RendererOptions options(m_vray->getOptions());

	m_vray->setRenderMode(mode);

	if (isInteractiveMode) {
#pragma message("TODO: Reimplement numThreads")
		options.numThreads = VUtils::Max(1, VUtils::getNumProcessors() - 1);

		m_vray->setKeepInteractiveRunning(true);
		m_vray->setInteractiveTimeout(0);
		m_vray->setInteractiveNoiseThreshold(0.0f);
		m_vray->setInteractiveSampleLevel(INT_MAX);
	}
	else {
#pragma message("TODO: Reimplement numThreads")
		options.numThreads = 0;

		m_vray->setKeepInteractiveRunning(false);
		m_vray->setInteractiveTimeout(settingsRTEngine.max_render_time * 60 * 1000);
		m_vray->setInteractiveNoiseThreshold(settingsRTEngine.noise_threshold);
		m_vray->setInteractiveSampleLevel(settingsRTEngine.max_sample_level);
	}

	fillSettingsRTEnginePlugin(settingsRTEngine, m_vray->getInstanceOrCreate("SettingsRTEngine"));

	const VRay::RendererState renderState = m_vray->getState();
	vassert(renderState >= VRay::IDLE_INITIALIZED && renderState <= VRay::IDLE_DONE);

	const int isCPU =
		mode == VRay::VRayRenderer::RENDER_MODE_PRODUCTION ||
		mode == VRay::VRayRenderer::RENDER_MODE_INTERACTIVE;

	if (!isCPU) {
		const int isCUDA =
			mode == VRay::VRayRenderer::RENDER_MODE_INTERACTIVE_CUDA ||
			mode == VRay::VRayRenderer::RENDER_MODE_PRODUCTION_CUDA;

		ComputeDeviceIdList deviceList;
		getGpuDeviceIdList(isCUDA, deviceList);

		m_vray->setComputeDevicesCurrentEngine(deviceList);
	}
}


void VRayPluginRenderer::removePlugin(const VRay::Plugin &plugin) const
{
	vassert(m_vray);

	m_vray->deletePlugin(plugin);

	const VRay::Error err = m_vray->getLastError();
	if (err.getCode() != VRay::SUCCESS) {
		Log::getLog().debug("Error removing plugin: %s", err.toString());
	}
}


void VRayPluginRenderer::removePlugin(const QString &pluginName) const
{
	vassert(m_vray);

	m_vray->deletePlugin(qPrintable(pluginName));

	const VRay::Error err = m_vray->getLastError();
	if (err.getCode() != VRay::SUCCESS) {
		Log::getLog().debug("Error removing plugin \"%s\": %s", qPrintable(pluginName), err.toString());
	}
}

int VRayPluginRenderer::exportScene(const QString &filepath, VRay::VRayExportSettings &settings)
{
	int res = 0;

	if (HOU::isIndie()) {
		Log::getLog().error("Export to the *.vrscene files is not allowed in Houdini Indie");
	}
	else if (m_vray) {
		Log::getLog().info("Starting export to \"%s\"...", qPrintable(filepath));

		res = m_vray->exportScene(qPrintable(filepath), settings);

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
	if (!m_vray)
		return 1;

	Log::getLog().info("Starting render for frame %i [%.3f]...",
					   m_vray->getCurrentFrame(), m_vray->getCurrentTime());

	m_vray->startSync();

	if (locked) {
		m_vray->waitForRenderEnd();
	}

	return 0;
}


void VRayPluginRenderer::stopRender()
{
	if (m_vray) {
		m_vray->stop();
	}
}

bool VRayPluginRenderer::isRendering() const
{
	if (!m_vray)
		return false;

	const VRay::RendererState state = m_vray->getState();

	return state >= VRay::PREPARING && state <= VRay::RENDERING_PAUSED;
}

void VRayPluginRenderer::setAnimation(bool on)
{
	if (m_vray) {
		m_vray->useAnimatedValues(on);
	}
}


void VRayPluginRenderer::setCurrentTime(fpreal frame)
{
	if (m_vray) {
		m_vray->setCurrentTime(frame);
	}
}


void VRayPluginRenderer::clearFrames(double toTime) const
{
	if (!m_vray)
		return;
	m_vray->clearAllPropertyValuesUpToTime(toTime);
}


void VRayPluginRenderer::reset()
{
	if (!m_vray)
		return;

#ifdef VFH_DEBUG
	// Error just to be more visible.
	Log::getLog().error("VRayPluginRenderer::reset()");
#endif

	unbindCallbacks(false);

	m_vray->clearScene();
}

void VRayPluginRenderer::unbindCallbacks(int unbindUiButtons) const
{
	if (!m_vray)
		return;

	if (unbindUiButtons) {
		m_vray->setOnVFBClosed(nullptr);
		m_vray->setOnVFBRenderLast(nullptr);
	}

	m_vray->setOnStateChanged(nullptr);
	m_vray->setOnProgress(nullptr);
	m_vray->setOnLogMessage(nullptr);
	m_vray->setOnRendererClose(nullptr);
}

void VFBSettings::fillVfbSettings(void *stateBuf, int stateBufSize, VFBSettings &settings)
{
	const int VFBS_CURR_VER = 16;

	int rd;

	PBufStream stream;
	stream.SetBuf(stateBuf, stateBufSize);
	stream.Open();

	{
		uint32 histInfoSize = 0;
		stream.Read(&histInfoSize, sizeof(histInfoSize), rd);
		uint8 *histBuf = new uint8[histInfoSize];
		stream.Read(histBuf, sizeof(uint8) * histInfoSize, rd);
		FreePtr(histBuf);
	}
	{
		uint32 stateSize = 0;
		stream.Read(&stateSize, sizeof(stateSize), rd);
	}

	stream.Read(&settings.structVersion, sizeof(settings.structVersion), rd);

	stream.Read(&settings.vfbx, sizeof(settings.vfbx), rd);
	stream.Read(&settings.vfby, sizeof(settings.vfby), rd);
	stream.Read(&settings.vfbWidth, sizeof(settings.vfbWidth), rd);
	stream.Read(&settings.vfbHeight, sizeof(settings.vfbHeight), rd);
	stream.Read(&settings.ccx, sizeof(settings.ccx), rd);
	stream.Read(&settings.ccy, sizeof(settings.ccy), rd);
	stream.Read(&settings.infox, sizeof(settings.infox), rd);
	stream.Read(&settings.infoy, sizeof(settings.infoy), rd);
	uint32 flagsL = 0;
	stream.ReadBinaryData(flagsL);
	settings.flags64=flagsL;
	stream.Read(&settings.posSaved, sizeof(settings.posSaved), rd);

	if (settings.structVersion <= VFBS_CURR_VER) {
		if (settings.structVersion >= 7) {
			stream.Read(&settings.lex, sizeof(settings.lex), rd);
			stream.Read(&settings.ley, sizeof(settings.ley), rd);
		}

		if (settings.structVersion >= 4) {
			stream.Read(&settings.isRenderRegionValid, sizeof(settings.isRenderRegionValid), rd);
			stream.Read(&settings.rrLeft, sizeof(settings.rrLeft), rd);
			stream.Read(&settings.rrTop, sizeof(settings.rrTop), rd);
			stream.Read(&settings.rrWidth, sizeof(settings.rrWidth), rd);
			stream.Read(&settings.rrHeight, sizeof(settings.rrHeight), rd);
		}

		if(settings.structVersion >= 10) {
			stream.Read(&settings.flagsrollouts, sizeof(settings.flagsrollouts), rd);
		}

		if(settings.structVersion >= 14) {
			stream.Read(&settings.rrLeftNorm, sizeof(settings.rrLeftNorm), rd);
			stream.Read(&settings.rrTopNorm, sizeof(settings.rrTopNorm), rd);
			stream.Read(&settings.rrWidthNorm, sizeof(settings.rrWidthNorm), rd);
			stream.Read(&settings.rrHeightNorm, sizeof(settings.rrHeightNorm), rd);
		}
		else {
			settings.rrLeftNorm=settings.rrTopNorm=settings.rrWidthNorm=settings.rrHeightNorm=-1;
		}

		if(settings.structVersion >= 16) {
			uint32 flagsH=0;
			stream.ReadBinaryData(flagsH);
			settings.flags64 |= static_cast<uint64>(flagsH)<<32;
		}
	}
}

void VRayPluginRenderer::saveVfbState(QString &stateData) const
{
	if (!m_vray)
		return;

	size_t stateDataSize = 0;

	VRay::VFBState *vfbState = m_vray->vfb.getState(stateDataSize);
	if (vfbState && stateDataSize) {
		const QByteArray vfbStateData(reinterpret_cast<const char*>(vfbState->getData()), stateDataSize);

		stateData = vfbStateData.toBase64();

		FreePtr(vfbState);
	}
}

void VRayPluginRenderer::restoreVfbState(const QString &stateData) const
{
	if (!m_vray)
		return;

	const QByteArray vfbStateData(QByteArray::fromBase64(stateData.toLocal8Bit()));

	m_vray->vfb.setState(reinterpret_cast<const void*>(vfbStateData.constData()), vfbStateData.length());
}

void VRayPluginRenderer::getVfbSettings(VFBSettings &settings) const
{
	if (!m_vray)
		return;

	size_t vfbStateSize = 0;
	VRay::VFBState *vfbState = m_vray->vfb.getState(vfbStateSize);

	if (vfbState && vfbStateSize) {
		VFBSettings::fillVfbSettings(vfbState->getData(), vfbStateSize, settings);
		FreePtr(vfbState);
	}
}
