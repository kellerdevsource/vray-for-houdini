//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_GeomDisplacedMesh.h"


using namespace VRayForHoudini;


void VOP::GeomDisplacedMesh::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID   = "GeomDisplacedMesh";
}


OP::VRayNode::PluginResult VOP::GeomDisplacedMesh::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	ECFnOBJNode fnObjContext(parentContext);
	if (NOT(fnObjContext.isValid())) {
		return OP::VRayNode::PluginResultError;
	}

	pluginDesc.pluginName = VRayExporter::getPluginName(fnObjContext.getTargetNode(), boost::str(Parm::FmtPrefixManual % pluginID % "@"));
	pluginDesc.pluginID = pluginID;

	// Displacement type
	//
	const int &displace_type = evalInt("type", 0, 0.0);

	pluginDesc.add(Attrs::PluginAttr("displace_2d",         int(displace_type == 1)));
	pluginDesc.add(Attrs::PluginAttr("vector_displacement", int(displace_type == 2)));

	const int idxTexCol = getInputFromName("displacement_tex_color");
	OP_Node *texCol = getInput(idxTexCol);
	const int idxTexFloat = getInputFromName("displacement_tex_float");
	OP_Node *texFloat = getInput(idxTexFloat);

	if (texCol) {
		VRay::Plugin texture = exporter.exportVop(texCol, parentContext);
		if (texture) {
			const Parm::SocketDesc *fromSocketInfo = exporter.getConnectedOutputType(this, "displacement_tex_color");

			if (   fromSocketInfo
				&& fromSocketInfo->type >= Parm::ParmType::eOutputColor
				&& fromSocketInfo->type  < Parm::ParmType::eUnknown)
			{
				pluginDesc.addAttribute(Attrs::PluginAttr("displacement_tex_color", texture, fromSocketInfo->name.getToken()));
			}
			else {
				pluginDesc.addAttribute(Attrs::PluginAttr("displacement_tex_color", texture));
			}

			if (NOT(texFloat)) {
				// Check if plugin has "out_intensity" output
				bool hasOutIntensity = false;
				const Parm::VRayPluginInfo *texPluginInfo = Parm::GetVRayPluginInfo(texture.getType());
				if (NOT(texPluginInfo)) {
					Log::getLog().error("Node \"%s\": Plugin \"%s\" description is not found!",
										this->getName().buffer(), texture.getType());
					return OP::VRayNode::PluginResultError;
				}
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
					Attrs::PluginDesc texOutputDesc(VRayExporter::getPluginName(texCol, "Out@"), "TexOutput");
					texOutputDesc.add(Attrs::PluginAttr("texmap", texture));

					texture = exporter.exportPlugin(texOutputDesc);
					pluginDesc.add(Attrs::PluginAttr("displacement_tex_float", texture, "out_intensity"));
				}
			}
		}
	}


	if (texFloat) {
		VRay::Plugin texture = exporter.exportVop(texFloat);
		if (texture) {
			const Parm::SocketDesc *fromSocketInfo = exporter.getConnectedOutputType(this, "displacement_tex_float");

			if (   fromSocketInfo
				&& fromSocketInfo->type >= Parm::ParmType::eOutputColor
				&& fromSocketInfo->type  < Parm::ParmType::eUnknown)
			{
				pluginDesc.addAttribute(Attrs::PluginAttr("displacement_tex_float", texture, fromSocketInfo->name.getToken()));
			}
			else {
				pluginDesc.addAttribute(Attrs::PluginAttr("displacement_tex_float", texture));
			}

			pluginDesc.add(Attrs::PluginAttr("displacement_tex_color", texture));
		}
	}

	return OP::VRayNode::PluginResultContinue;
}


void VOP::GeomDisplacedMesh::getCode(UT_String &codestr, const VOP_CodeGenContext &)
{
}
