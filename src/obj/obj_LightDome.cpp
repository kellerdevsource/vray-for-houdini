//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "obj_LightDome.h"


using namespace VRayForHoudini;


static PRM_Name  prm_heading("heading_vray_light_settings","V-Ray Light Settings");
static PRM_Name  prm_dome_tex("dome_tex_op", "Dome Texture");


void OBJ::LightDome::addPrmTemplate(Parm::PRMTmplList &prmTemplate)
{
	PRM_Template *defTmpl = OBJ_Light::getTemplateList(OBJ_PARMS_PLAIN);
	while (defTmpl->getType() != PRM_LIST_TERMINATOR) {
		prmTemplate.push_back(*defTmpl);
		defTmpl++;
	}

	prmTemplate.push_back(PRM_Template(PRM_HEADING, 1, &prm_heading));
	prmTemplate.push_back(PRM_Template(PRM_STRING_E, PRM_TYPE_DYNAMIC_PATH, 1, &prm_dome_tex, &Parm::PRMemptyStringDefault));
}


void OBJ::LightDome::setPluginType()
{
	pluginType = "LIGHT";
	pluginID   = "LightDome";
}


OP::VRayNode::PluginResult OBJ::LightDome::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, OP_Node *parent)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);

	// Need to flip tm
	VRay::Transform tm = VRayExporter::getObjTransform(parent->castToOBJNode(), exporter.getContext(), true);
	pluginDesc.addAttribute(Attrs::PluginAttr("transform", tm));

	// Dome texture
	//
	UT_String dome_tex;
	evalString(dome_tex, prm_dome_tex.getToken(), 0, 0.0f);
	if (NOT(dome_tex.equal(""))) {
		OP_Node *tex_node = OPgetDirector()->findNode(dome_tex.buffer());
		if (NOT(tex_node)) {
			Log::getLog().error("Texture node not found!");
		}
		else {
			VRay::Plugin texture = exporter.exportVop(tex_node);
			if (NOT(texture)) {
				Log::getLog().error("Texture node export failed!");
			}
			else {
				pluginDesc.addAttribute(Attrs::PluginAttr("dome_tex", texture));
			}
		}
	}

	return OP::VRayNode::PluginResultContinue;
}
