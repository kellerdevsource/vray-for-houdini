//
// Copyright (c) 2015-2017, Chaos Software Ltd
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

	Log::getLog().debug("RtCallbackView: %s from \"%s\"",
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
				exporter.exportView();
				exporter.exportDefaultHeadlight(true);
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
	const VRay::Transform &tm = VRayExporter::getObjTransform(camera.castToOBJNode(), context);
	
	const VRay::Vector v0(tm.matrix.v1.x, -tm.matrix.v1.z, tm.matrix.v1.y);
	const VRay::Vector v1(-tm.matrix.v2.x, tm.matrix.v2.z, -tm.matrix.v2.y);

	const float dd = v0.x * v0.x + v0.y * v0.y;
	float d = sqrtf(dd);
	if (v1.z > 0.0f)
		d = -d;
	const float shift = -d / sqrtf(1.0f - dd);

	return shift;
}

PhysicalCameraMode VRayExporter::usePhysicalCamera(const OBJ_Node &camera) const
{
	static const std::string paramUsePhysCam("CameraPhysical_use");

	PhysicalCameraMode physCamMode = PhysicalCameraMode::modeNone;

	if (Parm::isParmExist(camera, paramUsePhysCam)) {
		if (camera.evalInt(paramUsePhysCam.c_str(), 0, 0.0)) {
			physCamMode = PhysicalCameraMode::modeUser;
		}
	}

	if (physCamMode == PhysicalCameraMode::modeNone) {
		const fpreal t = getContext().getTime();

		const float winX = camera.evalFloat("win", 0, t);
		const float winY = camera.evalFloat("win", 1, t);

		if (!IsFloatEq(winX, 0.0f) || !IsFloatEq(winY, 0.0f)) {
			physCamMode = PhysicalCameraMode::modeAuto;
		}
	}

	return physCamMode;
}

void VRayExporter::fillViewParamFromCameraNode(const OBJ_Node &camera, ViewParams &viewParams)
{
	const fpreal t = getContext().getTime();

	int imageWidth  = camera.evalInt("res", 0, t);
	int imageHeight = camera.evalInt("res", 1, t);
	if (m_rop->evalInt("override_camerares", 0, t)) {
		UT_String resfraction;
		m_rop->evalString(resfraction, "res_fraction", 0, t);
		if (resfraction.isFloat()) {
			const fpreal k = resfraction.toFloat();
			imageWidth  *= k;
			imageHeight *= k;
		}
		else {
			imageWidth  = m_rop->evalInt("res_override", 0, t);
			imageHeight = m_rop->evalInt("res_override", 1, t);
		}
	}

	viewParams.renderSize.w = imageWidth;
	viewParams.renderSize.h = imageHeight;
	viewParams.renderView.tm = getObjTransform(camera.castToOBJNode(), m_context);

	if (Parm::getParmInt(*m_rop, "SettingsCamera_override_fov")) {
		viewParams.renderView.fov = Parm::getParmFloat(*m_rop, "SettingsCamera_fov");
	}
	else {
		const float aperture = camera.evalFloat("aperture", 0, t);
		fpreal focal = camera.evalFloat("focal", 0, t);

		viewParams.cameraPhysical.focalUnits =
			static_cast<HoudiniFocalUnits>(camera.evalInt("focalunits", 0, t));

		switch (viewParams.cameraPhysical.focalUnits) {
			case HoudiniFocalUnits::millimeters: break;
			case HoudiniFocalUnits::meters:     focal *= 1000.0f;    break;
			case HoudiniFocalUnits::nanometers: focal *= 0.0000001f; break;
			case HoudiniFocalUnits::inches:     focal *= 25.4f;      break;
			case HoudiniFocalUnits::feet:       focal *= 304.8f;     break;
			default: break;
		}

		viewParams.renderView.fov = getFov(aperture, focal);
	}

	viewParams.renderView.clip_start = camera.evalFloat("near", 0, t);
	viewParams.renderView.clip_end   = camera.evalFloat("far", 0, t);

	const float cropLeft   = camera.evalFloat("cropl", 0, t);
	const float cropRight  = camera.evalFloat("cropr", 0, t);
	const float cropBottom = camera.evalFloat("cropb", 0, t);
	const float cropTop    = camera.evalFloat("cropt", 0, t);

	viewParams.cropRegion.x = imageWidth * cropLeft;
	viewParams.cropRegion.y = imageHeight * (1.0f - cropTop);
	viewParams.cropRegion.width  = imageWidth * (cropRight - cropLeft);
	viewParams.cropRegion.height = imageHeight * (cropTop - cropBottom);

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

	viewParams.useCameraPhysical = static_cast<PhysicalCameraMode>(usePhysicalCamera(camera));
	if (viewParams.useCameraPhysical != PhysicalCameraMode::modeNone) {
		if (viewParams.useCameraPhysical == PhysicalCameraMode::modeAuto) {
			viewParams.cameraPhysical.exposure = false;
			viewParams.cameraPhysical.specify_fov = true;
			viewParams.cameraPhysical.fov = viewParams.renderView.fov;
			viewParams.cameraPhysical.horizontal_offset = -camera.evalFloat("win", 0, t);
			viewParams.cameraPhysical.vertical_offset   = -camera.evalFloat("win", 1, t);
		}
		else if (viewParams.useCameraPhysical == PhysicalCameraMode::modeUser) {
			viewParams.cameraPhysical.fovMode =
				static_cast<CameraFovMode>(camera.evalInt("CameraPhysical_fov_mode", 0, 0.0));

			switch (viewParams.cameraPhysical.fovMode) {
				case CameraFovMode::useHoudini: {
					viewParams.cameraPhysical.specify_fov = true;
					viewParams.cameraPhysical.fov = viewParams.renderView.fov;
					break;
				}
				case CameraFovMode::usePhysical: {
					viewParams.cameraPhysical.specify_fov = false;
					break;
				}
				case CameraFovMode::useFovOverride: {
					viewParams.cameraPhysical.specify_fov = true;
					viewParams.cameraPhysical.fov = SYSdegToRad(camera.evalFloat("CameraPhysical_fov", 0, t));
					break;
				}
			}

			viewParams.cameraPhysical.lens_shift = camera.evalInt("CameraPhysical_auto_lens_shift", 0, 0.0)
				                                       ? getLensShift(camera, getContext())
				                                       : camera.evalFloat("CameraPhysical_lens_shift", 0, t);
			viewParams.cameraPhysical.horizontal_shift = camera.evalFloat("CameraPhysical_horizontal_shift", 0, t);

			viewParams.cameraPhysical.type = static_cast<PhysicalCameraType>(camera.evalInt("CameraPhysical_type", 0, t));
			viewParams.cameraPhysical.film_width = camera.evalFloat("CameraPhysical_film_width", 0, t);
			viewParams.cameraPhysical.focal_length = camera.evalFloat("CameraPhysical_focal_length", 0, t);
			viewParams.cameraPhysical.zoom_factor = camera.evalFloat("CameraPhysical_zoom_factor", 0, t);
			viewParams.cameraPhysical.focus_distance = camera.evalFloat("CameraPhysical_focus_distance", 0, t);

			viewParams.cameraPhysical.distortion_type = camera.evalInt("CameraPhysical_distortion_type", 0, t);

			if (!camera.evalInt("CameraPhysical_parm_distortion_enable", 0, t)) {
				viewParams.cameraPhysical.distortion = 0.0f;
				viewParams.cameraPhysical.distortion_tex.clear();
			}
			else {
				viewParams.cameraPhysical.distortion = camera.evalFloat("CameraPhysical_distortion", 0, t);
				camera.evalString(viewParams.cameraPhysical.distortion_tex, "CameraPhysical_distortion_tex", 0, t);
			}

			viewParams.cameraPhysical.f_number = camera.evalFloat("CameraPhysical_f_number", 0, t);
			viewParams.cameraPhysical.shutter_speed = camera.evalFloat("CameraPhysical_shutter_speed", 0, t);
			viewParams.cameraPhysical.shutter_angle = camera.evalFloat("CameraPhysical_shutter_angle", 0, t);
			viewParams.cameraPhysical.shutter_offset = camera.evalFloat("CameraPhysical_shutter_offset", 0, t);
			viewParams.cameraPhysical.latency = camera.evalFloat("CameraPhysical_latency", 0, t);
			viewParams.cameraPhysical.ISO = camera.evalFloat("CameraPhysical_ISO", 0, t);

			viewParams.cameraPhysical.dof_display_threshold = camera.evalFloat("CameraPhysical_dof_display_threshold", 0, t);
			viewParams.cameraPhysical.exposure = camera.evalInt("CameraPhysical_exposure", 0, t);

			viewParams.cameraPhysical.white_balance.r = camera.evalFloat("CameraPhysical_white_balance", 0, t);
			viewParams.cameraPhysical.white_balance.g = camera.evalFloat("CameraPhysical_white_balance", 1, t);
			viewParams.cameraPhysical.white_balance.b = camera.evalFloat("CameraPhysical_white_balance", 2, t);

			viewParams.cameraPhysical.vignetting = camera.evalFloat("CameraPhysical_vignetting", 0, t);
			viewParams.cameraPhysical.blades_enable = camera.evalInt("CameraPhysical_blades_enable", 0, t);
			viewParams.cameraPhysical.blades_num = camera.evalInt("CameraPhysical_blades_num", 0, t);
			viewParams.cameraPhysical.blades_rotation = SYSdegToRad(camera.evalFloat("CameraPhysical_blades_rotation", 0, t));
			viewParams.cameraPhysical.center_bias = camera.evalFloat("CameraPhysical_center_bias", 0, t);
			viewParams.cameraPhysical.anisotropy = camera.evalFloat("CameraPhysical_anisotropy", 0, t);
			viewParams.cameraPhysical.use_dof = camera.evalInt("CameraPhysical_use_dof", 0, t);
			viewParams.cameraPhysical.use_moblur = camera.evalInt("CameraPhysical_use_moblur", 0, t);
			viewParams.cameraPhysical.subdivs = camera.evalInt("CameraPhysical_subdivs", 0, t);
			viewParams.cameraPhysical.dont_affect_settings = camera.evalInt("CameraPhysical_dont_affect_settings", 0, t);

			camera.evalString(viewParams.cameraPhysical.lens_file, "CameraPhysical_lens_file", 0, t);

			viewParams.cameraPhysical.horizontal_offset = camera.evalFloat("CameraPhysical_horizontal_offset", 0, t);
			viewParams.cameraPhysical.vertical_offset = camera.evalFloat("CameraPhysical_vertical_offset", 0, t);

			viewParams.cameraPhysical.bmpaperture_enable = camera.evalInt("CameraPhysical_bmpaperture_enable", 0, t);
			viewParams.cameraPhysical.bmpaperture_resolution = camera.evalInt("CameraPhysical_bmpaperture_resolution", 0, t);

			camera.evalString(viewParams.cameraPhysical.bmpaperture_tex, "CameraPhysical_bmpaperture_tex", 0, t);

			viewParams.cameraPhysical.optical_vignetting = camera.evalFloat("CameraPhysical_optical_vignetting", 0, t);
			viewParams.cameraPhysical.bmpaperture_affects_exposure = camera.evalInt("CameraPhysical_bmpaperture_affects_exposure", 0, t);
			viewParams.cameraPhysical.enable_thin_lens_equation = camera.evalInt("CameraPhysical_enable_thin_lens_equation", 0, t);
		}
	}
}

ReturnValue VRayExporter::fillSettingsMotionBlur(ViewParams &viewParams, Attrs::PluginDesc &settingsMotionBlur)
{
	setAttrsFromOpNodePrms(settingsMotionBlur, m_rop, "SettingsMotionBlur_");

	return ReturnValue::Success;
}

void VRayExporter::fillPhysicalCamera(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc)
{
	const PhysicalCameraParams &cameraPhysical = viewParams.cameraPhysical;

	pluginDesc.add(Attrs::PluginAttr("type", cameraPhysical.type));
	pluginDesc.add(Attrs::PluginAttr("film_width", cameraPhysical.film_width));
	pluginDesc.add(Attrs::PluginAttr("focal_length", cameraPhysical.focal_length));
	pluginDesc.add(Attrs::PluginAttr("zoom_factor", cameraPhysical.zoom_factor));
	pluginDesc.add(Attrs::PluginAttr("f_number", cameraPhysical.f_number));
	pluginDesc.add(Attrs::PluginAttr("lens_shift", cameraPhysical.lens_shift));
	pluginDesc.add(Attrs::PluginAttr("shutter_speed", cameraPhysical.shutter_speed));
	pluginDesc.add(Attrs::PluginAttr("shutter_angle", cameraPhysical.shutter_angle));
	pluginDesc.add(Attrs::PluginAttr("shutter_offset", cameraPhysical.shutter_offset));
	pluginDesc.add(Attrs::PluginAttr("latency", cameraPhysical.latency));
	pluginDesc.add(Attrs::PluginAttr("ISO", cameraPhysical.ISO));
	pluginDesc.add(Attrs::PluginAttr("specify_focus", cameraPhysical.specify_focus));
	pluginDesc.add(Attrs::PluginAttr("focus_distance", cameraPhysical.focus_distance));
	pluginDesc.add(Attrs::PluginAttr("targeted", cameraPhysical.targeted));
	pluginDesc.add(Attrs::PluginAttr("dof_display_threshold", cameraPhysical.dof_display_threshold));
	pluginDesc.add(Attrs::PluginAttr("exposure", cameraPhysical.exposure));

	pluginDesc.add(Attrs::PluginAttr("white_balance",
				   cameraPhysical.white_balance.r,
				   cameraPhysical.white_balance.g,
				   cameraPhysical.white_balance.b));

	pluginDesc.add(Attrs::PluginAttr("vignetting", cameraPhysical.vignetting));
	pluginDesc.add(Attrs::PluginAttr("blades_enable", cameraPhysical.blades_enable));
	pluginDesc.add(Attrs::PluginAttr("blades_num", cameraPhysical.blades_num));
	pluginDesc.add(Attrs::PluginAttr("blades_rotation", cameraPhysical.blades_rotation));
	pluginDesc.add(Attrs::PluginAttr("center_bias", cameraPhysical.center_bias));
	pluginDesc.add(Attrs::PluginAttr("anisotropy", cameraPhysical.anisotropy));
	pluginDesc.add(Attrs::PluginAttr("use_dof", cameraPhysical.use_dof));
	pluginDesc.add(Attrs::PluginAttr("use_moblur", cameraPhysical.use_moblur));
	pluginDesc.add(Attrs::PluginAttr("subdivs", cameraPhysical.subdivs));
	pluginDesc.add(Attrs::PluginAttr("dont_affect_settings", cameraPhysical.dont_affect_settings));
	pluginDesc.add(Attrs::PluginAttr("lens_file", cameraPhysical.lens_file.buffer()));
	pluginDesc.add(Attrs::PluginAttr("specify_fov", cameraPhysical.specify_fov));
	pluginDesc.add(Attrs::PluginAttr("fov", cameraPhysical.fov));
	pluginDesc.add(Attrs::PluginAttr("horizontal_shift", cameraPhysical.horizontal_shift));

	const float aspect = float(viewParams.renderSize.w) / float(viewParams.renderSize.h);
	const float aspectOffsetFix = 1.0f / aspect;

	float verticalOffset = cameraPhysical.vertical_offset * aspectOffsetFix;
	if (viewParams.renderView.stereoParams.use) {
		verticalOffset /= 2.0f;
	}

	pluginDesc.add(Attrs::PluginAttr("horizontal_offset", cameraPhysical.horizontal_offset));
	pluginDesc.add(Attrs::PluginAttr("vertical_offset", verticalOffset));

	pluginDesc.add(Attrs::PluginAttr("distortion_type", cameraPhysical.distortion_type));
	pluginDesc.add(Attrs::PluginAttr("distortion", cameraPhysical.distortion));
	pluginDesc.add(Attrs::PluginAttr("distortion_tex",
				   exportNodeFromPathWithDefaultMapping(cameraPhysical.distortion_tex, defaultMappingChannel)));

	pluginDesc.add(Attrs::PluginAttr("bmpaperture_enable", cameraPhysical.bmpaperture_enable));
	pluginDesc.add(Attrs::PluginAttr("bmpaperture_resolution", cameraPhysical.bmpaperture_resolution));

	pluginDesc.add(Attrs::PluginAttr("bmpaperture_tex",
				   exportNodeFromPathWithDefaultMapping(cameraPhysical.bmpaperture_tex, defaultMappingChannel)));

	pluginDesc.add(Attrs::PluginAttr("optical_vignetting", cameraPhysical.optical_vignetting));
	pluginDesc.add(Attrs::PluginAttr("bmpaperture_affects_exposure", cameraPhysical.bmpaperture_affects_exposure));
	pluginDesc.add(Attrs::PluginAttr("enable_thin_lens_equation", cameraPhysical.enable_thin_lens_equation));
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

	if (viewParams.useCameraPhysical != PhysicalCameraMode::modeNone) {
		pluginDesc.add(Attrs::PluginAttr("type", 0));
	}

	setAttrsFromOpNodePrms(pluginDesc, m_rop, "SettingsCamera_");
}

void VRayExporter::fillSettingsCameraDof(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc)
{
	const fpreal t = m_context.getTime();
	OBJ_Node *camera = VRayExporter::getCamera(m_rop);

	fpreal focalDist = m_rop->evalFloat("SettingsCameraDof_focal_dist", 0, t);
	if (m_rop->evalInt("SettingsCameraDof_focus_from_camera", 0, t)) {
		focalDist = camera->evalFloat("focus", 0, t);
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("focal_dist", focalDist));
	pluginDesc.addAttribute(Attrs::PluginAttr("aperture", (m_rop->evalFloat("SettingsCameraDof_aperture", 0, t)) / 100.0f));
	setAttrsFromOpNodePrms(pluginDesc, m_rop, "SettingsCameraDof_");
}

void VRayExporter::exportRenderView(const ViewParams &viewParams)
{
	Attrs::PluginDesc renderView("renderView", "RenderView");
	fillRenderView(viewParams, renderView);
	exportPlugin(renderView);
}

VRay::Plugin VRayExporter::exportPhysicalCamera(const ViewParams &viewParams, int needRemoval)
{
	if (needRemoval) {
		removePlugin("cameraPhysical", false);
	}

	Attrs::PluginDesc cameraPhysical("cameraPhysical", "CameraPhysical");
	fillPhysicalCamera(viewParams, cameraPhysical);

	return exportPlugin(cameraPhysical);
}

ReturnValue VRayExporter::exportView(const ViewParams &newViewParams)
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

			Log::getLog().warning("Maximum resolution for animations in Houdini Indie is 1920 x 1080");
			Log::getLog().warning("Clamping resolution to %i x %i",
									viewParams.renderSize.w, viewParams.renderSize.h);
		}
	}

	const int prevAutoCommit = getRenderer().getVRay().getAutoCommit();
	getRenderer().setAutoCommit(false);

	// NOTE: For animation we need to export keyframes everytime
	// or data will be wiped with "clearKeyFrames()".
	const bool needReset = isAnimation() || m_viewParams.needReset(viewParams);
	if (needReset) {
		Log::getLog().debug("VRayExporter::exportView: Resetting view...");

		if (!isAnimation()) {
			removePlugin("renderView", false);
			removePlugin("settingsCamera", false);
			removePlugin("settingsCameraDof", false);
			removePlugin("settingsMotionBlur", false);
			removePlugin("stereoSettings", false);
			removePlugin("cameraPhysical", false);
			removePlugin("cameraDefault", false);
		}

		Attrs::PluginDesc settingsCamera("settingsCamera", "SettingsCamera");
		fillSettingsCamera(viewParams, settingsCamera);
		exportPlugin(settingsCamera);

		if (viewParams.useCameraPhysical != PhysicalCameraMode::modeNone) {
			getRenderer().setCamera(exportPhysicalCamera(viewParams, false));
		}
		else {
			Attrs::PluginDesc settingsMotionBlur("settingsMotionBlur", "SettingsMotionBlur");
			fillSettingsMotionBlur(viewParams, settingsMotionBlur);
			exportPlugin(settingsMotionBlur);

			if (!viewParams.renderView.ortho) {
				Attrs::PluginDesc settingsCameraDof("settingsCameraDof", "SettingsCameraDof");
				fillSettingsCameraDof(viewParams, settingsCameraDof);
				exportPlugin(settingsCameraDof);
			}

			Attrs::PluginDesc cameraDefault("cameraDefault", "CameraDefault");
			fillCameraDefault(viewParams, cameraDefault);
			getRenderer().setCamera(exportPlugin(cameraDefault));
		}

		if (viewParams.renderView.stereoParams.use && isIPR() != iprModeSOHO && !isGPU()) {
			Attrs::PluginDesc stereoSettings("stereoSettings", "VRayStereoscopicSettings");
			fillStereoSettings(viewParams, stereoSettings);
			exportPlugin(stereoSettings);
		}

		exportRenderView(viewParams);
	}
	else {
		if (m_viewParams.changedParams(viewParams)) {
			exportRenderView(viewParams);
		}
		if (m_viewParams.changedPhysCam(viewParams)) {
			getRenderer().setCamera(exportPhysicalCamera(viewParams, true));
		}
	}

	getRenderer().commit();
	getRenderer().setAutoCommit(prevAutoCommit);

	if (m_viewParams.changedSize(viewParams)) {
		setRenderSize(viewParams.renderSize.w, viewParams.renderSize.h);
	}

	if (m_viewParams.changedCropRegion(viewParams)) {
		getRenderer().getVRay().setRenderRegion(
			viewParams.cropRegion.x,
			viewParams.cropRegion.y,
			viewParams.cropRegion.width,
			viewParams.cropRegion.height);
	}

	// Store new params
	m_viewParams = viewParams;

	return ReturnValue::Success;
}

int VRayExporter::exportView()
{
	// We should not use this for IPR.
	vassert(m_isIPR != iprModeSOHO);

	Log::getLog().debug("VRayExporter::exportView()");

	static VUtils::FastCriticalSection viewCsect;
	if (!viewCsect.tryEnter())
		return 1;

	OBJ_Node *camera = getCamera(m_rop);
	if (!camera)
		return 1;

	addOpCallback(camera, RtCallbackView);

	ViewParams viewParams;
	fillViewParamFromCameraNode(*camera, viewParams);

	exportView(viewParams);

	viewCsect.leave();

	return 0;
}

int ViewParams::changedParams(const ViewParams &other) const
{
	return MemberNotEq(renderView);
}

int ViewParams::changedSize(const ViewParams &other) const
{
	return
		MemberNotEq(renderSize.w) ||
		MemberNotEq(renderSize.h);
}

int ViewParams::needReset(const ViewParams &other) const
{
	return
		MemberNotEq(useCameraPhysical) ||
		renderView.needReset(other.renderView);
}

int ViewParams::changedCropRegion(const ViewParams & other) const
{
	return
		MemberNotEq(cropRegion.x) ||
		MemberNotEq(cropRegion.y) ||
		MemberNotEq(cropRegion.width) ||
		MemberNotEq(cropRegion.height);
}

int ViewParams::changedPhysCam(const ViewParams &other) const
{
	return MemberNotEq(cameraPhysical);
}

bool StereoViewParams::operator == (const StereoViewParams &other) const
{
	return
		MemberEq(use) &&
		MemberFloatEq(stereo_eye_distance) &&
		MemberEq(stereo_interocular_method) &&
		MemberEq(stereo_specify_focus) &&
		MemberFloatEq(stereo_focus_distance) &&
		MemberEq(stereo_focus_method) &&
		MemberEq(stereo_view);
}

bool StereoViewParams::operator != (const StereoViewParams &other) const
{
	return !(*this == other);
}

bool PhysicalCameraParams::operator == (const PhysicalCameraParams &other) const 
{
	return
		MemberEq(fovMode) &&
		MemberEq(focalUnits) &&
		MemberEq(type) &&
		MemberFloatEq(film_width) &&
		MemberFloatEq(focal_length) &&
		MemberFloatEq(zoom_factor) &&
		MemberFloatEq(distortion) &&
		MemberEq(distortion_type) &&
		MemberFloatEq(f_number) &&
		MemberFloatEq(lens_shift) &&
		MemberFloatEq(shutter_speed) &&
		MemberFloatEq(shutter_angle) &&
		MemberFloatEq(shutter_offset) &&
		MemberFloatEq(latency) &&
		MemberFloatEq(ISO) &&
		MemberEq(specify_focus) &&
		MemberFloatEq(focus_distance) &&
		MemberFloatEq(dof_display_threshold) &&
		MemberEq(exposure) &&
		MemberFloatEq(white_balance.r) &&
		MemberFloatEq(white_balance.g) &&
		MemberFloatEq(white_balance.b) &&
		MemberFloatEq(vignetting) &&
		MemberEq(blades_enable) &&
		MemberEq(blades_num) &&
		MemberFloatEq(blades_rotation) &&
		MemberFloatEq(center_bias) &&
		MemberFloatEq(anisotropy) &&
		MemberEq(use_dof) &&
		MemberEq(use_moblur) &&
		MemberEq(subdivs) &&
		MemberEq(dont_affect_settings) &&
		MemberEq(lens_file) &&
		MemberEq(specify_fov) &&
		MemberFloatEq(fov) &&
		MemberFloatEq(horizontal_shift) &&
		MemberFloatEq(horizontal_offset) &&
		MemberFloatEq(vertical_offset) &&
		MemberEq(distortion_tex) &&
		MemberEq(bmpaperture_enable) &&
		MemberEq(bmpaperture_resolution) &&
		MemberEq(bmpaperture_tex) &&
		MemberFloatEq(optical_vignetting) &&
		MemberEq(bmpaperture_affects_exposure) &&
		MemberEq(enable_thin_lens_equation);
}

bool RenderViewParams::operator == (const RenderViewParams &other) const
{
	return
		MemberFloatEq(fov) &&
		MemberEq(ortho) &&
		MemberFloatEq(ortho_width) &&
		MemberEq(use_clip_start) &&
		MemberFloatEq(clip_start) &&
		MemberEq(use_clip_end) &&
		MemberFloatEq(clip_end) &&
		MemberEq(tm);
}

bool RenderViewParams::operator != (const RenderViewParams &other) const
{
	return !(*this == other);
}

int RenderViewParams::needReset(const RenderViewParams &other) const
{
	return
		!MemberEq(stereoParams.use) ||
		other.stereoParams.use && !MemberEq(stereoParams);
}
