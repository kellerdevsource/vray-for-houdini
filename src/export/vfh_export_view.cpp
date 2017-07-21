//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_exporter.h"
#include "vfh_prm_templates.h"
#include "vfh_hou_utils.h"
#include "vfh_rop.h"

#include <boost/algorithm/string.hpp>

using namespace VRayForHoudini;

const std::string VRayForHoudini::ViewPluginsDesc::settingsCameraDofPluginName("settingsCameraDof");
const std::string VRayForHoudini::ViewPluginsDesc::settingsCameraPluginName("settingsCamera");
const std::string VRayForHoudini::ViewPluginsDesc::cameraPhysicalPluginName("cameraPhysical");
const std::string VRayForHoudini::ViewPluginsDesc::cameraDefaultPluginName("cameraDefault");
const std::string VRayForHoudini::ViewPluginsDesc::renderViewPluginName("renderView");
const std::string VRayForHoudini::ViewPluginsDesc::stereoSettingsPluginName("stereoSettings");
const std::string VRayForHoudini::ViewPluginsDesc::settingsMotionBlurPluginName("settingsMotionBlur");

float VRayForHoudini::getFov(float aperture, float focal)
{
	// From https://www.sidefx.com/docs/houdini13.0/ref/cameralenses
	return 2.0f * atanf(aperture / 2.0f / focal);
}

void VRayExporter::RtCallbackView(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	if (!csect.tryEnter())
		return;

	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

	Log::getLog().info("RtCallbackView: %s from \"%s\"",
			   OPeventToString(type), caller->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED: {
			if (Parm::isParmSwitcher(*caller, reinterpret_cast<intptr_t>(data))) {
				break;
			}
		}
		case OP_INPUT_CHANGED: {
			bool procceedEvent = false;

			if (caller->castToOBJNode()) {
				procceedEvent = true;
			}
			else if (caller->castToROPNode()) {
				const PRM_Parm *param = Parm::getParm(*caller, reinterpret_cast<uintptr_t>(data));
				if (param) {
					procceedEvent = boost::starts_with(param->getToken(), "SettingsCamera") ||
									boost::starts_with(param->getToken(), "SettingsMotionBlur") ||
									boost::starts_with(param->getToken(), "VRayStereoscopic");
				}
			}

			if (procceedEvent) {
				exporter.exportDefaultHeadlight(true);
				exporter.exportView();
			}

			break;
		}
		case OP_NODE_PREDELETE: {
			exporter.delOpCallback(caller, VRayExporter::RtCallbackView);
			break;
		}
		default:
			break;
	}

	csect.leave();
}

static float getLensShift(const OBJ_Node &camera, OP_Context &context)
{
	float shift = 0.0f;

	VRay::Transform tm = VRayExporter::getObjTransform(camera.castToOBJNode(), context);
	
	const VRay::Vector v0 = tm.matrix.v1;
	const VRay::Vector v1 = tm.matrix.v2;
	
	const float dd = v0.x * v0.x + v0.y * v0.y;
	const float d = (v1.z > 0.0f)?-sqrtf(dd) : sqrtf(dd);
	shift = -d / sqrtf(1.0f - dd);

	return shift;
}

static void aspectCorrectFovOrtho(ViewParams &viewParams)
{
	const float aspect = float(viewParams.renderSize.w) / float(viewParams.renderSize.h);
	if (aspect < 1.0f) {
		viewParams.renderView.fov = 2.0f * atanf(tanf(viewParams.renderView.fov / 2.0f) * aspect);
		viewParams.renderView.ortho_width *= aspect;
	}
}

int VRayExporter::isPhysicalView(const OBJ_Node &camera) const
{
	static const std::string paramUsePhysCam("CameraPhysical_use");

	int isPhysical = false;
	if (Parm::isParmExist(camera, paramUsePhysCam)) {
		isPhysical = camera.evalInt(paramUsePhysCam.c_str(), 0, 0.0);
	}

	return isPhysical;
}

void VRayExporter::fillViewParamFromCameraNode(const OBJ_Node &camera, ViewParams &viewParams)
{
	const fpreal t = m_context.getTime();

	int imageWidth  = camera.evalInt("res", 0, t);
	int imageHeight = camera.evalInt("res", 1, t);
	if (m_rop->evalInt("override_camerares", 0, t)) {
		UT_String resfraction;
		m_rop->evalString(resfraction, "res_fraction", 0, t);
		if (resfraction.isFloat()) {
			fpreal k = resfraction.toFloat();
			imageWidth  *= k;
			imageHeight *= k;
		}
		else {
			imageWidth  = m_rop->evalInt("res_override", 0, t);
			imageHeight = m_rop->evalInt("res_override", 1, t);
		}
	}

	float fov = 0.785398f;

	viewParams.renderView.fovOverride = Parm::getParmInt(*m_rop, "SettingsCamera_override_fov");
	if (viewParams.renderView.fovOverride) {
		fov = Parm::getParmFloat(*m_rop, "SettingsCamera_fov");
	}
	else {
		const float aperture = camera.evalFloat("aperture", 0, t);
		const float focal = camera.evalFloat("focal", 0, t);
		fov = getFov(aperture, focal);
	}

	viewParams.renderSize.w = imageWidth;
	viewParams.renderSize.h = imageHeight;
	viewParams.renderView.fov = fov;
	viewParams.renderView.tm = VRayExporter::getObjTransform(camera.castToOBJNode(), m_context);

	if (!viewParams.renderView.fovOverride) {
		aspectCorrectFovOrtho(viewParams);
	}

	viewParams.renderView.stereoParams.use = Parm::getParmInt(*m_rop, "VRayStereoscopicSettings_use");
	viewParams.renderView.stereoParams.stereo_eye_distance       = Parm::getParmFloat(*m_rop, "VRayStereoscopicSettings_eye_distance");
	viewParams.renderView.stereoParams.stereo_interocular_method = Parm::getParmInt(*m_rop,   "VRayStereoscopicSettings_interocular_method");
	viewParams.renderView.stereoParams.stereo_specify_focus      = Parm::getParmInt(*m_rop,   "VRayStereoscopicSettings_specify_focus");
	viewParams.renderView.stereoParams.stereo_focus_distance     = Parm::getParmFloat(*m_rop, "VRayStereoscopicSettings_focus_distance");
	viewParams.renderView.stereoParams.stereo_focus_method       = Parm::getParmInt(*m_rop,   "VRayStereoscopicSettings_focus_method");
	viewParams.renderView.stereoParams.stereo_view               = Parm::getParmInt(*m_rop,   "VRayStereoscopicSettings_view");
	viewParams.renderView.stereoParams.adjust_resolution         = Parm::getParmInt(*m_rop,   "VRayStereoscopicSettings_adjust_resolution");

	if (viewParams.renderView.stereoParams.use &&
		viewParams.renderView.stereoParams.adjust_resolution)
	{
		viewParams.renderSize.w *= 2;
	}
}

void VRayExporter::fillSettingsMotionBlur(ViewParams &viewParams)
{
	const fpreal t = m_context.getTime();

	int useCameraSettings = m_rop->evalInt("SettingsMotionBlur_camera_motion_blur", 0, t);
	if (useCameraSettings) {
		OBJ_Node *camera = VRayExporter::getCamera(m_rop);
		const fpreal shutter = camera->evalFloat("shutter", 0, t);
		viewPlugins.settingsMotionBlur.addAttribute(Attrs::PluginAttr("duration", shutter));
	}

	setAttrsFromOpNodePrms(viewPlugins.settingsMotionBlur, m_rop, "SettingsMotionBlur_");
}

void VRayExporter::fillPhysicalCamera(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc)
{
	if (!viewParams.cameraObject)
		return;

	OBJ_Node &camera = *viewParams.cameraObject;
	const fpreal t = getContext().getTime();

	const float aspect = float(viewParams.renderSize.w) / float(viewParams.renderSize.h);

	float horizontal_offset = camera.evalFloat("CameraPhysical_horizontal_offset", 0, t);
	float vertical_offset   = camera.evalFloat("CameraPhysical_vertical_offset", 0, t);
	if (aspect < 1.0f) {
		const float offset_fix = 1.0 / aspect;
		horizontal_offset *= offset_fix;
		vertical_offset   *= offset_fix;
	}

	const float lens_shift = camera.evalInt("CameraPhysical_auto_lens_shift", 0, 0.0)
							 ? getLensShift(camera, getContext())
							 : camera.evalFloat("CameraPhysical_lens_shift", 0, t);

	pluginDesc.add(Attrs::PluginAttr("fov", viewParams.renderView.fov));
	pluginDesc.add(Attrs::PluginAttr("horizontal_offset", horizontal_offset));
	pluginDesc.add(Attrs::PluginAttr("vertical_offset",   vertical_offset));
	pluginDesc.add(Attrs::PluginAttr("lens_shift",        lens_shift));
	// pluginDesc.add(Attrs::PluginAttr("specify_focus",     true));

	setAttrsFromOpNodePrms(pluginDesc, &camera, "CameraPhysical_");
}

void VRayExporter::fillRenderView(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc)
{
	pluginDesc.add(Attrs::PluginAttr("transform", viewParams.renderView.tm));
	pluginDesc.add(Attrs::PluginAttr("fov", viewParams.renderView.fov));
	pluginDesc.add(Attrs::PluginAttr("clipping", (viewParams.renderView.use_clip_start || viewParams.renderView.use_clip_end)));
	pluginDesc.add(Attrs::PluginAttr("clipping_near", viewParams.renderView.clip_start));
	pluginDesc.add(Attrs::PluginAttr("clipping_far", viewParams.renderView.clip_end));
	pluginDesc.add(Attrs::PluginAttr("orthographic", viewParams.renderView.ortho));
	pluginDesc.add(Attrs::PluginAttr("orthographicWidth", viewParams.renderView.ortho_width));

	if (isIPR()) {
		pluginDesc.add(Attrs::PluginAttr("use_scene_offset", false));
	}

	if (isGPU() && viewParams.renderView.stereoParams.use) {
		pluginDesc.add(Attrs::PluginAttr("stereo_on",                 viewParams.renderView.stereoParams.use));
		pluginDesc.add(Attrs::PluginAttr("stereo_eye_distance",       viewParams.renderView.stereoParams.stereo_eye_distance));
		pluginDesc.add(Attrs::PluginAttr("stereo_interocular_method", viewParams.renderView.stereoParams.stereo_interocular_method));
		pluginDesc.add(Attrs::PluginAttr("stereo_specify_focus",      viewParams.renderView.stereoParams.stereo_specify_focus));
		pluginDesc.add(Attrs::PluginAttr("stereo_focus_distance",     viewParams.renderView.stereoParams.stereo_focus_distance));
		pluginDesc.add(Attrs::PluginAttr("stereo_focus_method",       viewParams.renderView.stereoParams.stereo_focus_method));
		pluginDesc.add(Attrs::PluginAttr("stereo_view",               viewParams.renderView.stereoParams.stereo_view));
	}
}

void VRayExporter::fillStereoSettings(const ViewParams& /*viewParams*/, Attrs::PluginDesc &pluginDesc)
{
	setAttrsFromOpNodePrms(pluginDesc, m_rop, "VRayStereoscopicSettings_");
}

void VRayExporter::fillCameraDefault(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc)
{
	pluginDesc.add(Attrs::PluginAttr("orthographic", viewParams.renderView.ortho));
}

void VRayExporter::fillSettingsCamera(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc)
{
	pluginDesc.add(Attrs::PluginAttr("fov", -1.0f));

	if (viewParams.usePhysicalCamera) {
		pluginDesc.add(Attrs::PluginAttr("type", 8));
	}

	setAttrsFromOpNodePrms(pluginDesc, m_rop, "SettingsCamera_");
}

void VRayExporter::fillSettingsCameraDof(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc)
{
	const fpreal t = m_context.getTime();
	OBJ_Node *camera = VRayExporter::getCamera(m_rop);

	fpreal focalDist = m_rop->evalFloat( "SettingsCameraDof_focal_dist", 0, t);
	if (m_rop->evalInt("SettingsCameraDof_focus_from_camera", 0, t)) {
		focalDist = camera->evalFloat("focus", 0, t);
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("focal_dist", focalDist));

	setAttrsFromOpNodePrms(pluginDesc, m_rop, "SettingsCameraDof_");
}

void VRayExporter::exportView(const ViewParams &newViewParams)
{
	ViewParams viewParams(newViewParams);

	if (isAnimation() && HOU::isIndie()) {
		const int maxIndieW = 1920;
		const int maxIndieH = 1080;

		const float aspect = float(maxIndieW) / float(maxIndieH);

		if (viewParams.renderSize.w > maxIndieW ||
			viewParams.renderSize.h > maxIndieH)
		{
			if (viewParams.renderSize.w > maxIndieW) {
				viewParams.renderSize.w = maxIndieW;
				viewParams.renderSize.h = viewParams.renderSize.w / aspect;
			}

			if (viewParams.renderSize.h > maxIndieH) {
				viewParams.renderSize.h = maxIndieH;
				viewParams.renderSize.w = viewParams.renderSize.h * aspect;
			}

			Log::getLog().warning("Houdini Indie render resolution for animations is 1920 x 1080 maximum");
			Log::getLog().warning("Clamping resolution to %i x %i",
									viewParams.renderSize.w, viewParams.renderSize.h);
		}
	}

	const bool needReset = m_viewParams.needReset(viewParams);
	if (needReset) {
		Log::getLog().warning("VRayExporter::exportView: Reseting view plugins...");

		const int prevAutoCommit = getRenderer().getVRay().getAutoCommit();

		getRenderer().setAutoCommit(false);

		removePlugin(ViewPluginsDesc::settingsCameraPluginName);
		removePlugin(ViewPluginsDesc::settingsCameraDofPluginName);
		removePlugin(ViewPluginsDesc::stereoSettingsPluginName);
		removePlugin(ViewPluginsDesc::cameraPhysicalPluginName);
		removePlugin(ViewPluginsDesc::cameraDefaultPluginName);

		fillRenderView(viewParams, viewPlugins.renderView);
		fillSettingsMotionBlur(viewParams);
		fillSettingsCamera(viewParams, viewPlugins.settingsCamera);

		if (!isIPR() && !isGPU()) {
			fillStereoSettings(viewParams, viewPlugins.stereoSettings);
		}

		if (viewParams.usePhysicalCamera) {
			fillPhysicalCamera(viewParams, viewPlugins.cameraPhysical);
		}
		else {
			fillCameraDefault(viewParams, viewPlugins.cameraDefault);
			fillSettingsCameraDof(viewParams, viewPlugins.settingsCameraDof);
		}

		exportPlugin(viewPlugins.settingsCamera);
		exportPlugin(viewPlugins.settingsMotionBlur);

		if (!viewParams.renderView.ortho && !viewParams.usePhysicalCamera) {
			exportPlugin(viewPlugins.settingsCameraDof);
		}

		if (viewParams.usePhysicalCamera) {
			getRenderer().setCamera(exportPlugin(viewPlugins.cameraPhysical));
		}
		else {
			getRenderer().setCamera(exportPlugin(viewPlugins.cameraDefault));
		}

		if (!isIPR() && !isGPU() && viewParams.renderView.stereoParams.use) {
			exportPlugin(viewPlugins.stereoSettings);
		}

		exportPlugin(viewPlugins.renderView);

		getRenderer().commit();
		getRenderer().setAutoCommit(prevAutoCommit);
	}
	else if (m_viewParams.changedParams(viewParams)) {
		fillRenderView(viewParams, viewPlugins.renderView);
		exportPlugin(viewPlugins.renderView);
	}

	if (m_viewParams.changedSize(viewParams) ||
		m_viewParams.changedCropRegion(viewParams))
	{
		getRenderer().getVRay().setRenderRegion(
			viewParams.cropRegion.x,
			viewParams.cropRegion.y,
			viewParams.cropRegion.width,
			viewParams.cropRegion.height);
		setRenderSize(viewParams.renderSize.w, viewParams.renderSize.h);
	}

	// Store new params
	m_viewParams = viewParams;
}

int VRayExporter::exportView()
{
	Log::getLog().debug("VRayExporter::exportView()");

	static VUtils::FastCriticalSection csect;
	if (!csect.tryEnter())
		return 1;

	OBJ_Node *camera = getCamera(m_rop);
	if (!camera)
		return 1;

	addOpCallback(camera, RtCallbackView);
	addOpCallback(m_rop, RtCallbackView);

	ViewParams viewParams(camera);
	viewParams.usePhysicalCamera = isPhysicalView(*camera);

	fillViewParamFromCameraNode(*camera, viewParams);

	exportView(viewParams);

	csect.leave();

	return 0;
}

int ViewPluginsDesc::needReset(const ViewPluginsDesc &other) const
{
	// NOTE: No need to reset on RenderView, we handle it differently
	//
	return (settingsCameraDof.isDifferent(other.settingsCameraDof) ||
			settingsMotionBlur.isDifferent(other.settingsMotionBlur) ||
			settingsCamera.isDifferent(other.settingsCamera) ||
			cameraPhysical.isDifferent(other.cameraPhysical) ||
			cameraDefault.isDifferent(other.cameraDefault));
}

int ViewParams::changedParams(const ViewParams &other) const
{
	return MemberNotEq(renderView);
}

int ViewParams::changedSize(const ViewParams &other) const
{
	return (MemberNotEq(renderSize.w) ||
			MemberNotEq(renderSize.h));
}

int ViewParams::needReset(const ViewParams &other) const
{
	return (MemberNotEq(usePhysicalCamera) ||
			MemberNotEq(cameraObject) ||
			renderView.needReset(other.renderView));
}

int ViewParams::changedCropRegion(const ViewParams & other) const
{
	return (MemberNotEq(cropRegion.x) ||
			MemberNotEq(cropRegion.y) ||
			MemberNotEq(cropRegion.width) ||
			MemberNotEq(cropRegion.height));
}

bool StereoViewParams::operator ==(const StereoViewParams &other) const
{
	return (MemberEq(use) &&
			MemberFloatEq(stereo_eye_distance) &&
			MemberEq(stereo_interocular_method) &&
			MemberEq(stereo_specify_focus) &&
			MemberFloatEq(stereo_focus_distance) &&
			MemberEq(stereo_focus_method) &&
			MemberEq(stereo_view));
}

bool StereoViewParams::operator !=(const StereoViewParams &other) const
{
	return !(*this == other);
}

bool RenderViewParams::operator ==(const RenderViewParams &other) const
{
	return (MemberFloatEq(fov) &&
			MemberEq(ortho) &&
			MemberFloatEq(ortho_width) &&
			MemberEq(use_clip_start) &&
			MemberFloatEq(clip_start) &&
			MemberEq(use_clip_end) &&
			MemberFloatEq(clip_end) &&
			MemberEq(tm));
}

bool RenderViewParams::operator !=(const RenderViewParams &other) const
{
	return !(*this == other);
}

int RenderViewParams::needReset(const RenderViewParams &other) const
{
	return ((stereoParams.use != other.stereoParams.use) ||
			(other.stereoParams.use && (stereoParams != other.stereoParams)));
}
