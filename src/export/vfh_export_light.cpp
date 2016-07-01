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
#include "obj/obj_node_base.h"
#include "rop/vfh_rop.h"

#include <OP/OP_Bundle.h>


using namespace VRayForHoudini;


void VRayExporter::RtCallbackLight(OP_Node *caller, void *callee, OP_EventType type, void* data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

	OBJ_Node *obj_node = caller->castToOBJNode();

	Log::getLog().info("RtCallbackLight: %s from \"%s\"",
			   OPeventToString(type), caller->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED: {
			if (Parm::isParmSwitcher(*caller, long(data))) {
				break;
			}
		}
		case OP_INPUT_CHANGED:
		case OP_INPUT_REWIRED: {
			exporter.exportLight(obj_node);
			break;
		}
		case OP_NODE_PREDELETE: {
			exporter.removePlugin(obj_node);
			exporter.delOpCallbacks(obj_node);
			break;
		}
		default:
			break;
	}
}


int VRayExporter::isLightEnabled(OP_Node *op_node)
{
	const fpreal t = getContext().getTime();

	fpreal dimmer = 0;
	op_node->evalParameterOrProperty("dimmer", 0, t, dimmer);

	OP_Bundle *bundle = m_rop->getForcedLightsBundle();
	return (   (bundle && bundle->contains(op_node, false))
			|| (dimmer > 0));
}


VRay::Plugin VRayExporter::exportDefaultHeadlight(bool update)
{
	static const UT_StringRef theHeadlightNameToken = "DefaultHeadlight";

	int headlight = 0;
	m_rop->evalParameterOrProperty("soho_autoheadlight",
								   0,
								   getContext().getTime(),
								   headlight
								   );
	if (!headlight) {
		return VRay::Plugin();
	}

	// create direcional light from camera
	OBJ_Node *cam = getCamera(m_rop);
	if (!cam) {
		return VRay::Plugin();
	}

	if (update) {
		VRay::Plugin light = m_renderer.getPlugin(theHeadlightNameToken);
		if (!light) {
			return VRay::Plugin();
		}
	}

	Attrs::PluginDesc pluginDesc(theHeadlightNameToken.buffer(), "LightDirect");
	pluginDesc.addAttribute(Attrs::PluginAttr("transform",
											  VRayExporter::getObjTransform(cam, getContext())
											  )
							);
	return exportPlugin(pluginDesc);
}


VRay::Plugin VRayExporter::exportLight(OBJ_Node *obj_node)
{
	const fpreal t = getContext().getTime();

	addOpCallback(obj_node, VRayExporter::RtCallbackLight);

	OBJ_Light *obj_light = obj_node->castToOBJLight();

	Attrs::PluginDesc pluginDesc;
	pluginDesc.pluginName = VRayExporter::getPluginName(obj_node);

	OP::VRayNode *vrayNode = dynamic_cast<OP::VRayNode*>(obj_light);
	if (vrayNode) {
		pluginDesc.addAttribute(Attrs::PluginAttr("enabled", isLightEnabled(obj_node) ));

		ExportContext expContext(CT_OBJ, *this, *obj_node);
		OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(pluginDesc, *this, &expContext);

		if (res == OP::VRayNode::PluginResultError) {
			Log::getLog().error("Error creating plugin descripion for node: \"%s\" [%s]",
						obj_light->getName().buffer(),
						obj_light->getOperator()->getName().buffer());
		}
		else if (res == OP::VRayNode::PluginResultNA ||
				 res == OP::VRayNode::PluginResultContinue)
		{
			setAttrsFromOpNodePrms(pluginDesc, obj_light);
		}

		bool fliptm = vrayNode->getVRayPluginID() == OBJ::getVRayPluginIDName(OBJ::VRayPluginID::LightDome);
		VRay::Transform tm = VRayExporter::getObjTransform(obj_node, m_context, fliptm);
		pluginDesc.addAttribute(Attrs::PluginAttr("transform", tm));

	}
	else {
		const VRayLightType lightType = static_cast<VRayLightType>(obj_light->evalInt("light_type", 0, 0.0));

		Log::getLog().info("  Found light: type = %i",
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

		VRay::Transform tm = VRayExporter::getObjTransform(obj_node, m_context);
		pluginDesc.addAttribute(Attrs::PluginAttr("transform", tm));
	}

	return exportPlugin(pluginDesc);
}


void VRayExporter::exportLights()
{
	const fpreal t = getContext().getTime();

	OP_Bundle *activeLights = m_rop->getActiveLightsBundle();
	if (   !activeLights
		||  activeLights->entries() <= 0 )
	{
		exportDefaultHeadlight();
		return;
	}

	for (int i = 0; i < activeLights->entries(); ++i) {
		OP_Node *node = activeLights->getNode(i);
		if (!node) {
			continue;
		}

		OBJ_Node *objNode = node->castToOBJNode();
		if (!objNode) {
			continue;
		}

		OBJ_Light *light = objNode->castToOBJLight();
		if (!light) {
			continue;
		}

		exportLight(light);
	}
}
