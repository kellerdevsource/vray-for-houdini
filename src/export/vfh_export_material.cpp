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
#include "vfh_op_utils.h"

#include "vop/material/vop_mtl_def.h"

#include <SHOP/SHOP_Node.h>
#include <VOP/VOP_ParmGenerator.h>
#include <OP/OP_Options.h>

using namespace VRayForHoudini;

void VRayExporter::RtCallbackSurfaceShop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	if (!csect.tryEnter())
		return;

	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

	Log::getLog().info("RtCallbackSurfaceShop: %s from \"%s\"",
					   OPeventToString(type), caller->getName().buffer());

	if (type == OP_INPUT_REWIRED) {
		exporter.exportMaterial(caller);
	}
	else if (type == OP_NODE_PREDELETE) {
		exporter.delOpCallback(caller, RtCallbackSurfaceShop);
	}

	csect.leave();
}

VRay::Plugin VRayExporter::exportMaterial(VOP_Node *vopNode)
{
	if (!vopNode) {
		return VRay::Plugin();
	}

	VRay::Plugin material = exportVop(vopNode);

	const VOP_Type vopType = vopNode->getShaderType();
	if (vopType == VOP_TYPE_BSDF) {
		// Wrap BRDF into MtlSingleBRDF for RT GPU to work properly.
		Attrs::PluginDesc mtlPluginDesc(getPluginName(vopNode, "MtlSingle"), "MtlSingleBRDF");
		mtlPluginDesc.addAttribute(Attrs::PluginAttr("brdf", material));
		material = exportPlugin(mtlPluginDesc);
	}

	if (material && isIPR()) {
		// Wrap material into MtlRenderStats to always have the same material name.
		// Used when rewiring materials when running interactive RT session.
		Attrs::PluginDesc pluginDesc(getPluginName(vopNode, "MtlStats"), "MtlRenderStats");
		pluginDesc.addAttribute(Attrs::PluginAttr("base_mtl", material));
		material = exportPlugin(pluginDesc);
	}

	return material;
}

VRay::Plugin VRayExporter::exportMaterial(OP_Node *matNode)
{
	VRay::Plugin material;

	VOP_Node *vopNode = CAST_VOPNODE(matNode);
	if (vopNode) {
		addOpCallback(matNode, RtCallbackSurfaceShop);

		material = exportMaterial(vopNode);
	}
	else {
		OP_Node *materialNode = getVRayNodeFromOp(*matNode, "Material");
		if (materialNode) {
			addOpCallback(matNode, RtCallbackSurfaceShop);

			material = exportMaterial(CAST_VOPNODE(materialNode));
		}
	}

	if (!material) {
		material = exportDefaultMaterial();
	}

	return material;
}


VRay::Plugin VRayExporter::exportDefaultMaterial()
{
	Attrs::PluginDesc brdfDesc("BRDFDiffuse@Clay", "BRDFDiffuse");
	brdfDesc.addAttribute(Attrs::PluginAttr("color", 0.5f, 0.5f, 0.5f));

	Attrs::PluginDesc mtlDesc("Mtl@Clay", "MtlSingleBRDF");
	mtlDesc.addAttribute(Attrs::PluginAttr("brdf", exportPlugin(brdfDesc)));

	return exportPlugin(mtlDesc);
}


void VRayExporter::setAttrsFromSHOPOverrides(Attrs::PluginDesc &pluginDesc, VOP_Node &vopNode)
{
	OP_Network *creator = vopNode.getCreator();
	if (NOT(creator)) {
		return;
	}

	const Parm::VRayPluginInfo *pluginInfo = Parm::GetVRayPluginInfo( pluginDesc.pluginID );
	if (!pluginInfo) {
		return;
	}

	const fpreal t = m_context.getTime();

	VOP_ParmGeneratorList prmVOPs;
	vopNode.getParmInputs(prmVOPs);
	for (VOP_ParmGenerator *prmVOP : prmVOPs) {
		int inpidx = vopNode.whichInputIs(prmVOP);
		if (inpidx < 0) {
			continue;
		}

		UT_String inpName;
		vopNode.getInputName(inpName, inpidx);
		const std::string attrName = inpName.toStdString();
		// plugin doesn't have such attribute or
		// it has already been exported
		if (   NOT(pluginInfo->attributes.count(attrName))
			|| pluginDesc.contains(attrName) )
		{
			continue;
		}

		UT_String prmToken = prmVOP->getParmNameCache();
		const PRM_Parm *prm = creator->getParmList()->getParmPtr(prmToken);
		// no such parameter on the parent SHOP node or
		// parameter is not floating point
		if (   NOT(prm)
			|| NOT(prm->getType().isFloatType()) )
		{
			continue;
		}

		const Parm::AttrDesc &attrDesc = pluginInfo->attributes.at(attrName);
		switch (attrDesc.value.type) {
			case Parm::eBool:
			case Parm::eEnum:
			case Parm::eInt:
			case Parm::eTextureInt:
			{
				Attrs::PluginDesc mtlOverrideDesc(VRayExporter::getPluginName(&vopNode, attrName), "TexUserScalar");
				mtlOverrideDesc.addAttribute(Attrs::PluginAttr("default_value", creator->evalInt(prm, 0, t)));
				mtlOverrideDesc.addAttribute(Attrs::PluginAttr("user_attribute", prm->getToken()));

				VRay::Plugin overridePlg = exportPlugin(mtlOverrideDesc);
				pluginDesc.addAttribute(Attrs::PluginAttr(attrName, overridePlg, "scalar"));

				break;
			}
			case Parm::eFloat:
			case Parm::eTextureFloat:
			{
				Attrs::PluginDesc mtlOverrideDesc(VRayExporter::getPluginName(&vopNode, attrName), "TexUserScalar");
				mtlOverrideDesc.addAttribute(Attrs::PluginAttr("default_value", creator->evalFloat(prm, 0, t)));
				mtlOverrideDesc.addAttribute(Attrs::PluginAttr("user_attribute", prm->getToken()));

				VRay::Plugin overridePlg = exportPlugin(mtlOverrideDesc);
				pluginDesc.addAttribute(Attrs::PluginAttr(attrName, overridePlg, "scalar"));

				break;
			}
			case Parm::eColor:
			case Parm::eAColor:
			case Parm::eTextureColor:
			{
				Attrs::PluginDesc mtlOverrideDesc(VRayExporter::getPluginName(&vopNode, attrName), "TexUserColor");

				Attrs::PluginAttr attr("default_color", Attrs::PluginAttr::AttrTypeAColor);
				for (int i = 0; i < std::min(prm->getVectorSize(), 4); ++i) {
					attr.paramValue.valVector[i] = creator->evalFloat(prm, i, t);
				}
				mtlOverrideDesc.addAttribute(attr);
				mtlOverrideDesc.addAttribute(Attrs::PluginAttr("user_attribute", prm->getToken()));

				VRay::Plugin mtlOverridePlg = exportPlugin(mtlOverrideDesc);
				pluginDesc.addAttribute(Attrs::PluginAttr(attrName, mtlOverridePlg, "color"));

				break;
			}
			case Parm::eVector:
			{
				VRay::Vector v;
				for (int i = 0; i < std::min(3, prm->getVectorSize()); ++i) {
					v[i] = creator->evalFloat(prm->getToken(), i, t);
				}

				pluginDesc.addAttribute(Attrs::PluginAttr(attrName, v));
				break;
			}
			case Parm::eMatrix:
			{
				VRay::Matrix m(1);

				for (int k = 0; k < std::min(9, prm->getVectorSize()); ++k) {
					const int i = k / 3;
					const int j = k % 3;
					m[i][j] = creator->evalFloat(prm->getToken(), k, t);
				}

				pluginDesc.addAttribute(Attrs::PluginAttr(attrName, m));
				break;
			}
			case Parm::eTransform:
			{
				VRay::Transform tm(1);

				for (int k = 0; k < std::min(16, prm->getVectorSize()); ++k) {
					const int i = k / 4;
					const int j = k % 4;
					if (i < 3) {
						if (j < 3) {
							tm.matrix[i][j] = creator->evalFloat(prm->getToken(), k, t);
						}
					}
					else {
						if (j < 3) {
							tm.offset[j] = creator->evalFloat(prm->getToken(), k, t);
						}
					}
				}

				pluginDesc.addAttribute(Attrs::PluginAttr(attrName, tm));
				break;
			}
			default:
			// ignore other types for now
				;
		}
	}
}
