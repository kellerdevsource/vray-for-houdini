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

#if 0
static void aspectCorrectFovOrtho(ViewParams &viewParams)
{
	const float aspect = float(viewParams.renderSize.w) / float(viewParams.renderSize.h);
	if (aspect < 1.0f) {
		viewParams.renderView.fov = 2.0f * atanf(tanf(viewParams.renderView.fov / 2.0f) * aspect);
		viewParams.renderView.ortho_width *= aspect;
	}
}
#endif

int VRayExporter::isPhysicalCamera(const OBJ_Node &camera)
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
			const fpreal k = resfraction.toFloat();
			imageWidth  *= k;
			imageHeight *= k;
		}
		else {
			imageWidth  = m_rop->evalInt("res_override", 0, t);
			imageHeight = m_rop->evalInt("res_override", 1, t);
		}
	}

	float fov;

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
	viewParams.renderView.tm = getObjTransform(camera.castToOBJNode(), m_context);

	// TODO: Correct FOV with "winres"

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

	viewParams.usePhysicalCamera = isPhysicalCamera(camera);
	if (viewParams.usePhysicalCamera) {
		viewParams.physCam.type = static_cast<PhysicalCameraType>(camera.evalInt("CameraPhysical_type", 0, t));
		viewParams.physCam.useDof = camera.evalInt("CameraPhysical_use_dof", 0, t);
		viewParams.physCam.useMoBlur = camera.evalInt("CameraPhysical_use_moblur", 0, t);
		viewParams.physCam.selectedItem = static_cast<MenuItemSelected>(camera.evalInt("CameraPhysical_mode_select", 0, t));

		if (viewParams.physCam.selectedItem == physicalCameraUseHoudiniCameraSettings) {
			viewParams.physCam.focalUnits = static_cast<HoudiniFocalUnits>(camera.evalInt("focalunits", 0, t));
			viewParams.physCam.houdiniFNumber = camera.evalFloat("fstop", 0, t);
			viewParams.physCam.houdiniFocalLength = camera.evalFloat("focal", 0, t);
			viewParams.physCam.houdiniFocusDistance = camera.evalFloat("focus", 0, t);
		}
		else if (viewParams.physCam.selectedItem == physicalCameraUsePhysicalCameraSettings) {
			viewParams.physCam.exposure = camera.evalInt("CameraPhysical_exposure", 0, t);
			viewParams.physCam.focalLength = camera.evalFloat("CameraPhysical_focal_length", 0, t);
			viewParams.physCam.fNumber = camera.evalFloat("CameraPhysical_f_number", 0, t);
			viewParams.physCam.shutterSpeed = camera.evalFloat("CameraPhysical_shutter_speed", 0, t);
		}

		viewParams.physCam.filmWidth = camera.evalFloat("CameraPhysical_film_width", 0, t);
		viewParams.physCam.fov = camera.evalFloat("CameraPhysical_fov", 0, t);
		viewParams.physCam.shutterAngle = camera.evalFloat("CameraPhysical_shutter_angle", 0, t);
		viewParams.physCam.shutterOffset = camera.evalFloat("CameraPhysical_shutter_offset", 0, t);
		viewParams.physCam.latency = camera.evalFloat("CameraPhysical_latency", 0, t);
		viewParams.physCam.ISO = camera.evalFloat("CameraPhysical_ISO", 0, t);
		viewParams.physCam.zoomFactor = camera.evalFloat("CameraPhysical_zoom_factor", 0, t);
		viewParams.physCam.specifyFocus = camera.evalInt("CameraPhysical_specify_focus", 0, t);
		viewParams.physCam.focusDistance = camera.evalFloat("CameraPhysical_focus_distance", 0, t);
		viewParams.physCam.targeted = camera.evalInt("CameraPhysical_targeted", 0, t);
		viewParams.physCam.targetDistance = camera.evalFloat("CameraPhysical_target_distance", 0, t);
		viewParams.physCam.balance.r = camera.evalFloat("CameraPhysical_white_balance", 0, t);
		viewParams.physCam.balance.g = camera.evalFloat("CameraPhysical_white_balance", 1, t);
		viewParams.physCam.balance.b = camera.evalFloat("CameraPhysical_white_balance", 2, t);
		viewParams.physCam.vignetting = camera.evalFloat("CameraPhysical_vignetting", 0, t);
		viewParams.physCam.opticalVignetting = camera.evalFloat("CameraPhysical_optical_vignetting", 0, t);
		viewParams.physCam.subdivisions = camera.evalFloat("CameraPhysical_subdivs", 0, t);
		viewParams.physCam.dontAffectSettings = camera.evalInt("CameraPhysical_dont_affect_settings", 0, t);
	}
}

ReturnValue VRayExporter::fillSettingsMotionBlur(ViewParams &viewParams, Attrs::PluginDesc &settingsMotionBlur)
{
	setAttrsFromOpNodePrms(settingsMotionBlur, m_rop, "SettingsMotionBlur_");

	return ReturnValue::Success;
}

void VRayExporter::fillPhysicalCamera(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc)
{
	if (!viewParams.cameraObject)
		return;

	OBJ_Node &camera = *viewParams.cameraObject;
	const fpreal t = getContext().getTime();

	const float aspect = float(viewParams.renderSize.w) / float(viewParams.renderSize.h);

	float horizontalOffset = camera.evalFloat("CameraPhysical_horizontal_offset", 0, t);
	float verticalOffset = camera.evalFloat("CameraPhysical_vertical_offset", 0, t);
	if (aspect < 1.0f) {
		const float offsetFix = 1.0 / aspect;
		horizontalOffset *= offsetFix;
		verticalOffset *= offsetFix;
	}

	const float lens_shift = camera.evalInt("CameraPhysical_auto_lens_shift", 0, 0.0)
		? getLensShift(camera, getContext())
		: camera.evalFloat("CameraPhysical_lens_shift", 0, t);

	const MenuItemSelected itemSelected =
		static_cast<MenuItemSelected>(camera.evalInt("CameraPhysical_mode_select", 0, 0.0));

	if (itemSelected != physicalCameraUseFieldOfView) {
		pluginDesc.add(Attrs::PluginAttr("fov", viewParams.renderView.fov));
	}

	pluginDesc.add(Attrs::PluginAttr("horizontal_offset", horizontalOffset));
	pluginDesc.add(Attrs::PluginAttr("vertical_offset",   verticalOffset));
	pluginDesc.add(Attrs::PluginAttr("lens_shift",        lens_shift));

	setAttrsFromOpNodePrms(pluginDesc, &camera, "CameraPhysical_");

	int specifyFovValue = 0;

	switch (itemSelected) {
		case physicalCameraUseHoudiniCameraSettings: {
			pluginDesc.remove("shutter_speed");

			UT_String focalUnits;
			camera.evalString(focalUnits, "focalunits", 0, t);

			const double focalLength = camera.evalFloat("focal", 0, t);

			double resultValue;
			if (focalUnits == "mm") {
				resultValue = focalLength;
			}
			else if (focalUnits == "m") {
				// Convert from meters to milimeters
				resultValue = focalLength * 1000.0f;
			}
			else if (focalUnits == "nm") {
				// Convert from nanometers to milimeters
				resultValue = focalLength * 0.0000001f;
			}
			else if (focalUnits == "in") {
				// Convert from inches to milimeters
				resultValue = focalLength * 25.4f;
			}
			else if (focalUnits == "ft") {
				// Convert from feet to milimeters
				resultValue = focalLength * 304.8f;
			}

			pluginDesc.add(Attrs::PluginAttr("focal_length", resultValue));
			pluginDesc.add(Attrs::PluginAttr("f_number", camera.evalFloat("fstop", 0, t)));
			pluginDesc.add(Attrs::PluginAttr("focus_distance", camera.evalFloat("focus", 0, t)));

			break;
		}
		case physicalCameraUseFieldOfView: {
			pluginDesc.remove("film_width");
			pluginDesc.remove("focal_length");
			specifyFovValue = 1;
			break;
		}
		case physicalCameraUsePhysicalCameraSettings: {
			pluginDesc.remove("fov");
			specifyFovValue = 0;
			break;
		}
	}

	// Use fov value or not based on menu option selected
	pluginDesc.add(Attrs::PluginAttr("specify_fov", specifyFovValue));
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
		pluginDesc.add(Attrs::PluginAttr("type", 0));
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
	pluginDesc.addAttribute(Attrs::PluginAttr("aperture", (m_rop->evalFloat("SettingsCameraDof_aperture", 0, t)) / 100.0f));
	setAttrsFromOpNodePrms(pluginDesc, m_rop, "SettingsCameraDof_");
}

VRay::Plugin VRayExporter::recreatePhysicalCamera(const ViewParams &viewParams) {
	removePlugin("cameraPhysical");
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

	const bool needReset = m_viewParams.needReset(viewParams);
	if (needReset) {
		Log::getLog().debug("VRayExporter::exportView: Reseting view plugins...");

		removePlugin("settingsCamera", false);
		removePlugin("settingsCameraDof", false);
		removePlugin("stereoSettings", false);
		removePlugin("cameraPhysical", false);
		removePlugin("cameraDefault", false);

		Attrs::PluginDesc renderView("renderView", "RenderView");
		Attrs::PluginDesc settingsMotionBlur("settingsMotionBlur", "SettingsMotionBlur");
		fillRenderView(viewParams, renderView);
		if (fillSettingsMotionBlur(viewParams, settingsMotionBlur) == ReturnValue::Error) {
			return ReturnValue::Error;
		}
		Attrs::PluginDesc settingsCamera("settingsCamera", "SettingsCamera");
		fillSettingsCamera(viewParams, settingsCamera);
		Attrs::PluginDesc stereoSettings("stereoSettings", "VRayStereoscopicSettings");
		if (!isIPR() && !isGPU()) {
			fillStereoSettings(viewParams, stereoSettings);
		}
		Attrs::PluginDesc cameraPhysical("cameraPhysical", "CameraPhysical");
		Attrs::PluginDesc cameraDefault("cameraDefault", "CameraDefault");
		Attrs::PluginDesc settingsCameraDof("settingsCameraDof", "SettingsCameraDof");

		exportPlugin(settingsCamera);
		exportPlugin(settingsMotionBlur);

		if (viewParams.usePhysicalCamera) {
			getRenderer().setCamera(recreatePhysicalCamera(viewParams));
		}
		else {
			fillCameraDefault(viewParams, cameraDefault);
			fillSettingsCameraDof(viewParams, settingsCameraDof);
			if (!viewParams.renderView.ortho) {
				exportPlugin(settingsCameraDof);
			}
			getRenderer().setCamera(exportPlugin(cameraDefault));
		}

		if (viewParams.renderView.stereoParams.use && !isIPR() && !isGPU()) {
			exportPlugin(stereoSettings);
		}

		exportPlugin(renderView);
	}
	else if (m_viewParams.changedParams(viewParams)) {
		Attrs::PluginDesc renderView("renderView", "RenderView");
		fillRenderView(viewParams, renderView);
		exportPlugin(renderView);
	}
	else if (m_viewParams.changedPhysCam(viewParams)) {
		recreatePhysicalCamera(viewParams);
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

	ViewParams viewParams(camera);
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

int ViewParams::changedPhysCam(const ViewParams &other) const
{
	return (MemberNotEq(physCam));
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

bool PhysicalCameraParams::operator ==(const PhysicalCameraParams &other) const 
{
	return (MemberEq(type) &&
			MemberEq(useDof) &&
			MemberEq(useMoBlur) && 
			MemberEq(selectedItem) &&
			MemberEq(exposure) &&
			MemberFloatEq(filmWidth) &&
			MemberFloatEq(focalLength) &&
			MemberFloatEq(fov) &&
			MemberFloatEq(fNumber) &&
			MemberFloatEq(shutterSpeed) &&
			MemberFloatEq(shutterAngle) &&
			MemberFloatEq(shutterOffset) &&
			MemberFloatEq(latency) &&
			MemberFloatEq(ISO) &&
			MemberFloatEq(zoomFactor) &&
			MemberFloatEq(focusDistance) &&
			MemberFloatEq(targetDistance) &&
			MemberFloatEq(balance.r) &&
			MemberFloatEq(balance.g) &&
			MemberFloatEq(balance.b) &&
			MemberFloatEq(vignetting) &&
			MemberFloatEq(opticalVignetting) &&
			MemberEq(subdivisions) &&
			MemberEq(dontAffectSettings) &&
			MemberEq(focalUnits) &&
			MemberFloatEq(houdiniFocalLength) &&
			MemberFloatEq(houdiniFocusDistance) &&
			MemberFloatEq(houdiniFNumber));
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
