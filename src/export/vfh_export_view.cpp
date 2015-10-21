//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_exporter.h"
#include "vfh_prm_templates.h"

#include <boost/algorithm/string.hpp>


using namespace VRayForHoudini;


const std::string VRayForHoudini::ViewPluginsDesc::settingsCameraDofPluginName("settingsCameraDof");
const std::string VRayForHoudini::ViewPluginsDesc::settingsCameraPluginName("settingsCamera");
const std::string VRayForHoudini::ViewPluginsDesc::cameraPhysicalPluginName("cameraPhysical");
const std::string VRayForHoudini::ViewPluginsDesc::cameraDefaultPluginName("cameraDefault");
const std::string VRayForHoudini::ViewPluginsDesc::renderViewPluginName("renderView");


void VRayExporter::RtCallbackView(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter *exporter = reinterpret_cast<VRayExporter*>(callee);

	PRINT_INFO("RtCallbackView: %s from \"%s\"",
			   OPeventToString(type), caller->getName().buffer());

	if (type == OP_INPUT_CHANGED) {
		exporter->exportView();
	}
	else if (type == OP_PARM_CHANGED) {
		if (!Parm::isParmSwitcher(*caller, long(data))) {
			bool procceedEvent = false;

			if (caller->castToOBJNode()) {
				procceedEvent = true;
			}
			else if (caller->castToROPNode()) {
				const PRM_Parm *param = Parm::getParm(*caller, reinterpret_cast<long>(data));
				if (param) {
					procceedEvent = boost::starts_with(param->getToken(), "SettingsCamera") ||
									boost::starts_with(param->getToken(), "SettingsMotionBlur");
				}
			}

			if (procceedEvent) {
				exporter->exportView();
			}
		}
	}
	else if (type == OP_NODE_PREDELETE) {
		exporter->delOpCallback(caller, VRayExporter::RtCallbackView);
	}
}


static float getLensShift(const OBJ_Node &camera)
{
	// TODO: getLensShift
	return 0.0f;
}


static void aspectCorrectFovOrtho(ViewParams &viewParams)
{
	const float aspect = float(viewParams.renderSize.w) / float(viewParams.renderSize.h);
	if (aspect < 1.0f) {
		viewParams.renderView.fov = 2.0f * atanf(tanf(viewParams.renderView.fov / 2.0f) * aspect);
		viewParams.renderView.ortho_width *= aspect;
	}
}


int VRayExporter::isPhysicalView(const OBJ_Node &camera)
{
	static const std::string paramUsePhysCam("CameraPhysical_use");

	int isPhysical = false;
	if (Parm::isParmExist(camera, "CameraPhysical_use")) {
		isPhysical = camera.evalInt(paramUsePhysCam.c_str(), 0, 0.0);
	}

	return isPhysical;
}


void VRayExporter::fillCameraData(const OBJ_Node &camera, const OP_Node &rop, ViewParams &viewParams)
{
	const fpreal t = m_context.getTime();

	const int imageWidth  = camera.evalFloat("res", 0, t);
	const int imageHeight = camera.evalFloat("res", 1, t);

	float fov = 0.785398f;

	viewParams.renderView.fovOverride = Parm::getParmInt(rop, "SettingsCamera.override_fov");
	if (viewParams.renderView.fovOverride) {
		fov = Parm::getParmFloat(rop, "SettingsCamera.fov");
	}
	else {
		// From https://www.sidefx.com/docs/houdini13.0/ref/cameralenses
		const float apx   = camera.evalFloat("aperture", 0, t);
		const float focal = camera.evalFloat("focal", 0, t);
		fov = 2.0f * atanf((apx / 2.0f) / focal);
	}

	viewParams.renderSize.w = imageWidth;
	viewParams.renderSize.h = imageHeight;
	viewParams.renderView.fov = fov;
	viewParams.renderView.tm = VRayExporter::GetOBJTransform(camera.castToOBJNode(), m_context);

	if (!viewParams.renderView.fovOverride) {
		aspectCorrectFovOrtho(viewParams);
	}
}


void VRayExporter::fillPhysicalCamera(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc)
{
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
							 ? getLensShift(camera)
							 : camera.evalFloat("CameraPhysical_lens_shift", 0, t);

	pluginDesc.add(Attrs::PluginAttr("fov", viewParams.renderView.fov));
	pluginDesc.add(Attrs::PluginAttr("horizontal_offset", horizontal_offset));
	pluginDesc.add(Attrs::PluginAttr("vertical_offset",   vertical_offset));
	pluginDesc.add(Attrs::PluginAttr("lens_shift",        lens_shift));
	// pluginDesc.add(Attrs::PluginAttr("specify_focus",     true));

	// Can't use auto prefix here, because Python doesn't allow to add
	// template with "." in name
	setAttrsFromOpNode(pluginDesc, &camera, "CameraPhysical_");
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

	// TODO: Set this only for viewport rendering
	pluginDesc.add(Attrs::PluginAttr("use_scene_offset", false));
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

	setAttrsFromOpNode(pluginDesc, getRop(), "SettingsCamera.");
}


void VRayExporter::fillSettingsCameraDof(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc)
{
#if 0
	BL::Camera cameraData(viewParams.cameraObject.data());
	if (cameraData) {
		PointerRNA vrayCamera = RNA_pointer_get(&cameraData.ptr, "vray");
		PointerRNA cameraDof = RNA_pointer_get(&vrayCamera, "SettingsCameraDof");
		setAttrsFromPropGroupAuto(camDofDesc, &cameraDof, "SettingsCameraDof");
	}
#endif
}


int VRayExporter::exportView()
{
	static VUtils::FastCriticalSection csect;
	if (csect.tryEnter()) {
		OBJ_Node *camera = VRayExporter::GetCamera(m_rop);
		if (!camera) {
			PRINT_ERROR("Camera is not set!");
			return 1;
		}

		addOpCallback(camera, VRayExporter::RtCallbackView);
		addOpCallback(getRop(), VRayExporter::RtCallbackView);

		ViewParams viewParams(camera);
		viewParams.usePhysicalCamera = isPhysicalView(*camera);

		fillCameraData(*camera, *m_rop, viewParams);
		fillSettingsCamera(viewParams, viewParams.viewPlugins.settingsCamera);
		fillRenderView(viewParams, viewParams.viewPlugins.renderView);

		if (viewParams.usePhysicalCamera) {
			fillPhysicalCamera(viewParams, viewParams.viewPlugins.cameraPhysical);
		}
		else {
			fillCameraDefault(viewParams, viewParams.viewPlugins.cameraDefault);
			fillSettingsCameraDof(viewParams, viewParams.viewPlugins.settingsCameraDof);
		}

		const bool needReset = m_viewParams.needReset(viewParams);
		if (needReset) {
			getRenderer().setAutoCommit(false);

			removePlugin(ViewPluginsDesc::renderViewPluginName);
			removePlugin(ViewPluginsDesc::cameraDefaultPluginName);
			removePlugin(ViewPluginsDesc::cameraPhysicalPluginName);
			removePlugin(ViewPluginsDesc::settingsCameraPluginName);
			removePlugin(ViewPluginsDesc::settingsCameraDofPluginName);

			exportPlugin(viewParams.viewPlugins.settingsCamera);

			if (!viewParams.renderView.ortho && !viewParams.usePhysicalCamera) {
				exportPlugin(viewParams.viewPlugins.settingsCameraDof);
			}
		}

		VRay::Plugin physCam;
		VRay::Plugin defCam;
		if (viewParams.usePhysicalCamera) {
			physCam = exportPlugin(viewParams.viewPlugins.cameraPhysical);
		}
		else {
			defCam = exportPlugin(viewParams.viewPlugins.cameraDefault);
		}

		VRay::Plugin renView;
		const bool paramsChanged = m_viewParams.changedParams(viewParams);
		if (needReset || paramsChanged) {
			renView = exportPlugin(viewParams.viewPlugins.renderView);
		}

		if (needReset) {
			if (physCam) {
				getRenderer().setCamera(physCam);
			}
			else if (defCam) {
				getRenderer().setCamera(defCam);
			}
		}

		if (m_viewParams.changedSize(viewParams)) {
			setRenderSize(viewParams.renderSize.w, viewParams.renderSize.h);
		}

		// Store new params
		m_viewParams = viewParams;

		if (needReset || paramsChanged) {
			getRenderer().commit();
		}

		// Restore autocommit
		if (needReset) {
			getRenderer().setAutoCommit(true);
		}

		csect.leave();
	}

	return 0;
}


int ViewPluginsDesc::needReset(const ViewPluginsDesc &other) const
{
	// NOTE: No need to reset on RenderView, we handle it differently
	//
	return (settingsCameraDof.isDifferent(other.settingsCameraDof) ||
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
			viewPlugins.needReset(other.viewPlugins));
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
