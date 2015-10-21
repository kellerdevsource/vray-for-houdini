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


using namespace VRayForHoudini;


const std::string VRayForHoudini::ViewParams::renderViewPluginName("renderView");
const std::string VRayForHoudini::ViewParams::physicalCameraPluginName("cameraPhysical");
const std::string VRayForHoudini::ViewParams::defaultCameraPluginName("cameraDefault");
const std::string VRayForHoudini::ViewParams::settingsCameraDofPluginName("settingsCameraDof");


static void aspectCorrectFovOrtho(ViewParams &viewParams)
{
	const float aspect = float(viewParams.renderSize.w) / float(viewParams.renderSize.h);
	if (aspect < 1.0f) {
		viewParams.renderView.fov = 2.0f * atanf(tanf(viewParams.renderView.fov / 2.0f) * aspect);
		viewParams.renderView.ortho_width *= aspect;
	}
}


void VRayExporter::fillCameraData(OP_Node &camera, ViewParams &viewParams)
{
	const fpreal t = m_context.getTime();

	const int imageWidth  = camera.evalFloat("res", 0, t);
	const int imageHeight = camera.evalFloat("res", 1, t);

	// From https://www.sidefx.com/docs/houdini13.0/ref/cameralenses
	const float apx   = camera.evalFloat("aperture", 0, t);
	const float focal = camera.evalFloat("focal", 0, t);
	const float fovx  = 2.0f * atanf((apx / 2.0f) / focal);

	viewParams.renderSize.w = imageWidth;
	viewParams.renderSize.h = imageHeight;
	viewParams.renderView.fov = fovx;
	viewParams.renderView.tm = VRayExporter::GetOBJTransform(camera.castToOBJNode(), m_context);

	aspectCorrectFovOrtho(viewParams);
}


void VRayExporter::fillPhysicalCamera(ViewParams &viewParams, Attrs::PluginDesc &physCamDesc)
{
#if 0
	BL::Camera cameraData(viewParams.cameraObject.data());
	if (cameraData) {
		PointerRNA vrayCamera = RNA_pointer_get(&cameraData.ptr, "vray");
		PointerRNA physicalCamera = RNA_pointer_get(&vrayCamera, "CameraPhysical");

		const float aspect = float(viewParams.renderSize.w) / float(viewParams.renderSize.h);

		float horizontal_offset = -cameraData.shift_x();
		float vertical_offset   = -cameraData.shift_y();
		if (aspect < 1.0f) {
			const float offset_fix = 1.0 / aspect;
			horizontal_offset *= offset_fix;
			vertical_offset   *= offset_fix;
		}

		const float lens_shift = RNA_boolean_get(&physicalCamera, "auto_lens_shift")
		                         ? GetLensShift(viewParams.cameraObject)
		                         : RNA_float_get(&physicalCamera, "lens_shift");

		float focus_distance = Blender::GetCameraDofDistance(viewParams.cameraObject);
		if (focus_distance < 0.001f) {
			focus_distance = 5.0f;
		}

		physCamDesc.add("fov", viewParams.renderView.fov);
		physCamDesc.add("horizontal_offset", horizontal_offset);
		physCamDesc.add("vertical_offset",   vertical_offset);
		physCamDesc.add("lens_shift",        lens_shift);
		physCamDesc.add("specify_focus",     true);
		physCamDesc.add("focus_distance",    focus_distance);

		setAttrsFromPropGroupAuto(physCamDesc, &physicalCamera, "CameraPhysical");
	}
#endif
}


VRay::Plugin VRayExporter::exportCameraPhysical(ViewParams &viewParams)
{
	Attrs::PluginDesc physCamDesc(ViewParams::physicalCameraPluginName, "CameraPhysical");
	fillPhysicalCamera(viewParams, physCamDesc);

	return exportPlugin(physCamDesc);
}


VRay::Plugin VRayExporter::exportRenderView(const ViewParams &viewParams)
{
	Attrs::PluginDesc viewDesc(ViewParams::renderViewPluginName, "RenderView");
	viewDesc.add(Attrs::PluginAttr("transform", viewParams.renderView.tm));
	viewDesc.add(Attrs::PluginAttr("fov", viewParams.renderView.fov));
	viewDesc.add(Attrs::PluginAttr("clipping", (viewParams.renderView.use_clip_start || viewParams.renderView.use_clip_end)));
	viewDesc.add(Attrs::PluginAttr("clipping_near", viewParams.renderView.clip_start));
	viewDesc.add(Attrs::PluginAttr("clipping_far", viewParams.renderView.clip_end));
	viewDesc.add(Attrs::PluginAttr("orthographic", viewParams.renderView.ortho));
	viewDesc.add(Attrs::PluginAttr("orthographicWidth", viewParams.renderView.ortho_width));

	// TODO: Set this only for viewport rendering
	viewDesc.add(Attrs::PluginAttr("use_scene_offset", false));

	return exportPlugin(viewDesc);
}


VRay::Plugin VRayExporter::exportCameraDefault(ViewParams &viewParams)
{
	Attrs::PluginDesc defCamDesc(ViewParams::defaultCameraPluginName, "CameraDefault");
	defCamDesc.add(Attrs::PluginAttr("orthographic", viewParams.renderView.ortho));

	return exportPlugin(defCamDesc);
}


VRay::Plugin VRayExporter::exportSettingsCameraDof(ViewParams &viewParams)
{
	Attrs::PluginDesc camDofDesc(ViewParams::settingsCameraDofPluginName, "SettingsCameraDof");

	if (viewParams.cameraObject && viewParams.cameraObject) {
#if 0
		BL::Camera cameraData(viewParams.cameraObject.data());
		if (cameraData) {
			PointerRNA vrayCamera = RNA_pointer_get(&cameraData.ptr, "vray");
			PointerRNA cameraDof = RNA_pointer_get(&vrayCamera, "SettingsCameraDof");
			setAttrsFromPropGroupAuto(camDofDesc, &cameraDof, "SettingsCameraDof");
		}
#endif
	}

	return exportPlugin(camDofDesc);
}


int VRayExporter::exportView()
{
	OP_Node *camera = VRayExporter::GetCamera(m_rop);
	if (!camera) {
		PRINT_ERROR("Camera is not set!");
		return 1;
	}

	ViewParams viewParams;
	fillCameraData(*camera, viewParams);

	bool needReset = m_viewParams.needReset(viewParams);
	if (!needReset && viewParams.usePhysicalCamera) {
		// needReset |= is_physical_updated(viewParams);
	}

	if (needReset) {
		removePlugin(ViewParams::settingsCameraDofPluginName);
		removePlugin(ViewParams::renderViewPluginName);
		removePlugin(ViewParams::defaultCameraPluginName);
		removePlugin(ViewParams::physicalCameraPluginName);
	}

	VRay::Plugin renView;
	VRay::Plugin physCam;
	VRay::Plugin defCam;

	if (needReset &&
		!viewParams.renderView.ortho &&
		!viewParams.usePhysicalCamera) {
		exportSettingsCameraDof(viewParams);
	}

	if (viewParams.usePhysicalCamera) {
		physCam = exportCameraPhysical(viewParams);
	}
	else {
		defCam = exportCameraDefault(viewParams);
	}

	const bool paramsChanged = m_viewParams.changedParams(viewParams);
	if (needReset || paramsChanged) {
		renView = exportRenderView(viewParams);
	}

	if (needReset) {
		// Commit camera plugin removal / creation
		getRenderer().commit();

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

	// Or use "autocommit"
	if (paramsChanged) {
		getRenderer().commit();
	}

	return 0;
}
