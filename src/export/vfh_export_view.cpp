/*
 * Copyright (c) 2015, Chaos Software Ltd
 *
 * V-Ray For Houdini
 *
 * Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
 *
 * Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
 *
 */

#include "vfh_exporter.h"


using namespace VRayForHoudini;


void VRayExporter::exportCamera(OP_Node *camera)
{
	addOpCallback(camera, VRayExporter::RtCallbackView);

	OBJ_Node *obj_camera = camera->castToOBJNode();

	// https://www.sidefx.com/docs/houdini13.0/ref/cameralenses
	//   fovx = 2 * atn( (apx/2) / focal )
	//
	const float apx   = camera->evalFloat("aperture", 0, m_context.getTime());
	const float focal = camera->evalFloat("focal", 0, m_context.getTime());

	const float fovx  = 2.0f * atanf((apx / 2.0f) / focal);

	Attrs::PluginDesc renderView("cameraView", "RenderView");
	renderView.addAttribute(Attrs::PluginAttr("transform", VRayExporter::GetOBJTransform(obj_camera, m_context)));
	renderView.addAttribute(Attrs::PluginAttr("fov", fovx));

	m_renderer.exportPlugin(renderView);
}


int VRayExporter::exportView(OP_Node *rop)
{
	OP_Node *camera = VRayExporter::GetCamera(rop);
	if (!camera) {
		PRINT_ERROR("Camera is not set!");
	}
	else {
		exportCamera(camera);
	}

	return 0;
}
