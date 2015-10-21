//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "sop_GeomStaticSmoothedMesh.h"


using namespace VRayForHoudini;


static PRM_Name  parm_tex_network("tex_network", "Displace Texture");


void SOP::GeomStaticSmoothedMesh::addPrmTemplate(Parm::PRMTmplList &prmTemplate)
{
	prmTemplate.push_back(PRM_Template(PRM_STRING_E, PRM_TYPE_DYNAMIC_PATH, 1, &parm_tex_network, &Parm::PRMemptyStringDefault));
}


void SOP::GeomStaticSmoothedMesh::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "GeomStaticSmoothedMesh";
}


OP_NodeFlags& SOP::GeomStaticSmoothedMesh::flags()
{
	OP_NodeFlags &flags = SOP_Node::flags();

	// This is a fake node for settings only
	flags.setBypass(true);

	return flags;
}


OP_ERROR SOP::GeomStaticSmoothedMesh::cookMySop(OP_Context &context)
{
	return error();
}


OP::VRayNode::PluginResult SOP::GeomStaticSmoothedMesh::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	PRINT_WARN("OP::GeomStaticSmoothedMesh::asPluginDesc()");

	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this, "Smooth@");

	// Displacement texture
	//
	UT_String tex_network;
	evalString(tex_network, parm_tex_network.getToken(), 0, 0.0f);
	if (NOT(tex_network.equal(""))) {
		OP_Node *tex_node = OPgetDirector()->findNode(tex_network.buffer());
		if (NOT(tex_node)) {
			PRINT_ERROR("Texture node not found!");
		}
		else {
			VRay::Plugin texture = exporter->exportVop(tex_node);
			if (NOT(texture)) {
				PRINT_ERROR("Texture node export failed!");
			}
			else {
				pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("displacement_tex_color", texture));

				// Check if plugin has "out_intensity" output
				bool hasOutIntensity = false;
				Parm::VRayPluginInfo *texPluginInfo = Parm::GetVRayPluginInfo(texture.getType());
				if (texPluginInfo->outputs.size()) {
					for (const auto &sock : texPluginInfo->outputs) {
						if (StrEq(sock.name.getToken(), "out_intensity")) {
							hasOutIntensity = true;
							break;
						}
					}
				}

				// Wrap texture with TexOutput
				if (NOT(hasOutIntensity)) {
					Attrs::PluginDesc texOutputDesc(VRayExporter::getPluginName(tex_node, "Out@"), "TexOutput");
					texOutputDesc.pluginAttrs.push_back(Attrs::PluginAttr("texmap", texture));

					texture = exporter->exportPlugin(texOutputDesc);
				}

				pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("displacement_tex_float", texture, "out_intensity"));
			}
		}
	}

	return OP::VRayNode::PluginResultContinue;
}
