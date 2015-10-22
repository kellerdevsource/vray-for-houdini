//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "obj/obj_node_base.h"
#include "vfh_exporter.h"


using namespace VRayForHoudini;


void VRayExporter::RtCallbackLight(OP_Node *caller, void *callee, OP_EventType type, void* /*data*/)
{
	VRayExporter *exporter = reinterpret_cast<VRayExporter*>(callee);

	OBJ_Node *obj_node = caller->castToOBJNode();

	PRINT_INFO("RtCallbackLight: %s from \"%s\"",
			   OPeventToString(type), caller->getName().buffer());

	if (type == OP_PARM_CHANGED ||
		type == OP_INPUT_CHANGED ||
		type == OP_INPUT_REWIRED)
	{
		exporter->exportLight(obj_node);
	}
	else if (type == OP_NODE_PREDELETE) {
		exporter->delOpCallbacks(caller);
		exporter->removePlugin(obj_node);
	}
}


VRay::Plugin VRayExporter::exportLight(OBJ_Node *obj_node)
{
	const fpreal t = m_context.getTime();

	addOpCallback(obj_node, VRayExporter::RtCallbackLight);

	OBJ_Light *obj_light = obj_node->castToOBJLight();

	OP_Operator *light_op = obj_light->getOperator();

	const UT_String &lightOpName = light_op->getName();

	Attrs::PluginDesc pluginDesc;
	pluginDesc.pluginName = VRayExporter::getPluginName(obj_node);

	VRay::Transform tm = VRayExporter::getObjTransform(obj_node, m_context);
	pluginDesc.addAttribute(Attrs::PluginAttr("transform", tm));

	if (lightOpName.startsWith("VRayNode")) {
		OBJ::LightNodeBase *vrayNode = static_cast<OBJ::LightNodeBase*>(obj_light);

		OP_Node *op_node = static_cast<OP_Node*>(obj_node);

		OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(pluginDesc, this, static_cast<OP_Node*>(obj_node));
		if (res == OP::VRayNode::PluginResultError) {
			PRINT_ERROR("Error creating plugin descripion for node: \"%s\" [%s]",
						op_node->getName().buffer(),
						lightOpName.buffer());
		}
		else if (res == OP::VRayNode::PluginResultNA ||
				 res == OP::VRayNode::PluginResultContinue)
		{
			setAttrsFromOpNode(pluginDesc, op_node);
		}

	}
	else {
		const VRayLightType lightType = static_cast<VRayLightType>(obj_light->evalInt("light_type", 0, 0.0));

		PRINT_INFO("  Found light: type = %i",
				   lightType);

		// Point
		if (lightType == VRayLightOmni) {
			pluginDesc.pluginID = "LightOmniMax";
		}
		// Grid
		else if (lightType == VRayLightRectangle) {
			pluginDesc.pluginID = "LightRectangle";

			pluginDesc.addAttribute(Attrs::PluginAttr("u_size", obj_light->evalFloat("areasize", 0, t) / 2.0));
			pluginDesc.addAttribute(Attrs::PluginAttr("v_size", obj_light->evalFloat("areasize", 1, t) / 2.0));

			pluginDesc.addAttribute(Attrs::PluginAttr("invisible", NOT(obj_light->evalInt("light_contribprimary", 0, t))));
		}
		// Sphere
		else if (lightType == VRayLightSphere) {
			pluginDesc.pluginID = "LightSphere";

			pluginDesc.addAttribute(Attrs::PluginAttr("radius", obj_light->evalFloat("areasize", 0, t) / 2.0));
		}
		// Distant
		else if (lightType == VRayLightDome) {
			pluginDesc.pluginID = "LightDome";
		}
		// Sun
		else if (lightType == VRayLightSun) {
			pluginDesc.pluginID = "SunLight";
		}

		pluginDesc.addAttribute(Attrs::PluginAttr("intensity", obj_light->evalFloat("light_intensity", 0, t)));
		pluginDesc.addAttribute(Attrs::PluginAttr("enabled",   obj_light->evalInt("light_enable", 0, t)));

		if (lightType != VRayLightSun) {
			pluginDesc.addAttribute(Attrs::PluginAttr("color", Attrs::PluginAttr::AttrTypeColor,
															   obj_light->evalFloat("light_color", 0, t),
															   obj_light->evalFloat("light_color", 1, t),
															   obj_light->evalFloat("light_color", 2, t)));
		}
	}

	return exportPlugin(pluginDesc);
}
