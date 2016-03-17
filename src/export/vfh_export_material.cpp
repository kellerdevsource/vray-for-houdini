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
#include "vfh_tex_utils.h"

#include "obj/obj_node_base.h"
#include "vop/vop_node_base.h"
#include "vop/material/vop_mtl_def.h"

#include <SHOP/SHOP_Node.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Options.h>
#include <OP/OP_PropertyLookupList.h>
#include <OP/OP_MultiparmInfo.h>
#include <GU/GU_Detail.h>


using namespace VRayForHoudini;



void VRayExporter::RtCallbackSurfaceShop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

	Log::getLog().info("RtCallbackSurfaceShop: %s from \"%s\"",
					   OPeventToString(type), caller->getName().buffer());

	if (type == OP_INPUT_REWIRED && caller->error() < UT_ERROR_ABORT) {
		UT_String inputName;
		const int idx = reinterpret_cast<long>(data);
		caller->getInputName(inputName, idx);

		if (inputName.equal("Material")) {
			SHOP_Node *shop_node = caller->getParent()->castToSHOPNode();
			if (shop_node) {
				UT_String shopPath;
				shop_node->getFullPath(shopPath);

				// XXX: Pass all referred objects
				ExportContext expContext;
				exporter.exportMaterial(*shop_node, expContext);
			}
		}
	}
	else if (type == OP_NODE_PREDELETE) {
		exporter.delOpCallback(caller, VRayExporter::RtCallbackSurfaceShop);
	}
}


VRay::Plugin VRayExporter::exportMaterial(SHOP_Node &shop_node, ExportContext &parentContext)
{
	VRay::Plugin material;
	UT_ValArray<OP_Node *> mtlOutList;
	if ( shop_node.getOpsByName("vray_material_output", mtlOutList) ) {
		// there is at least 1 "vray_material_output" node so take the first one
		VOP::MaterialOutput *mtlOut = static_cast< VOP::MaterialOutput * >( mtlOutList(0) );
		addOpCallback(mtlOut, VRayExporter::RtCallbackSurfaceShop);

		if (mtlOut->error() < UT_ERROR_ABORT ) {
			Log::getLog().info("Exporting material output \"%s\"...",
							   mtlOut->getName().buffer());

			const int idx = mtlOut->getInputFromName("Material");
			OP_Node *inpNode = mtlOut->getInput(idx);
			if (inpNode) {
				VOP_Node *vopNode = inpNode->castToVOPNode();
				if (vopNode) {
					switch (mtlOut->getInputType(idx)) {
						case VOP_SURFACE_SHADER: {
							material = exportVop(vopNode);
							break;
						}
						case VOP_TYPE_BSDF: {
							VRay::Plugin pluginBRDF = exportVop(vopNode);

							// Wrap BRDF into MtlSingleBRDF for RT GPU to work properly
							Attrs::PluginDesc mtlPluginDesc(VRayExporter::getPluginName(vopNode, "Mtl"), "MtlSingleBRDF");
							mtlPluginDesc.addAttribute(Attrs::PluginAttr("brdf", pluginBRDF));

							material = exportPlugin(mtlPluginDesc);
							break;
						}
						default:
							Log::getLog().error("Unsupported input type for node \"%s\", input %d!",
												mtlOut->getName().buffer(), idx);
					}

					if (material && isIPR()) {
						// Wrap material into MtlRenderStats to always have the same material name
						// Used when rewiring materials when running interactive RT session
						Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(&shop_node, "Mtl"), "MtlRenderStats");

						pluginDesc.addAttribute(Attrs::PluginAttr("base_mtl", material));
						material = exportPlugin(pluginDesc);
					}
				}
			}
		}
	}
	else {
		Log::getLog().error("Can't find \"V-Ray Material Output\" operator under \"%s\"!",
							shop_node.getName().buffer());
	}

	if ( NOT(material) ) {
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
	SHOP_Node *shopNode = vopNode.getParent()->castToSHOPNode();
	if (NOT(shopNode)) {
		return;
	}

	const Parm::VRayPluginInfo *pluginInfo = Parm::GetVRayPluginInfo( pluginDesc.pluginID );
	if (!pluginInfo) {
		return;
	}



	const OP_DependencyList &depList = shopNode->getOpDependents();
	for (OP_DependencyList::reverse_iterator it = depList.rbegin(); !it.atEnd(); it.advance()) {
		const OP_Dependency &dep = *it;
		if (dep.getRefOpId() != vopNode.getUniqueId()) {
			continue;
		}

		const PRM_Parm &vopPrm = vopNode.getParm(dep.getRefId().getParmRef());
		const PRM_Parm &shopPrm = shopNode->getParm(dep.getSourceRefId().getParmRef());

		const std::string attrName = vopPrm.getToken();
		if (   vopPrm.getType().isFloatType()
			&& pluginInfo->attributes.count(attrName)
			&& NOT(pluginDesc.contains(attrName)) )
		{
			const Parm::AttrDesc &attrDesc = pluginInfo->attributes.at(attrName);
			switch (attrDesc.value.type) {
				case Parm::eBool:
				case Parm::eEnum:
				case Parm::eInt:
				case Parm::eTextureInt:
				{
					Attrs::PluginDesc mtlOverrideDesc(VRayExporter::getPluginName(&vopNode, attrName), "TexUserScalar");
					mtlOverrideDesc.addAttribute(Attrs::PluginAttr("default_value", shopNode->evalInt(&shopPrm, 0, getContext().getTime())));
					mtlOverrideDesc.addAttribute(Attrs::PluginAttr("user_attribute", shopPrm.getToken()));

					VRay::Plugin overridePlg = exportPlugin(mtlOverrideDesc);
					pluginDesc.addAttribute(Attrs::PluginAttr(attrName, overridePlg, "scalar"));

					break;
				}
				case Parm::eFloat:
				case Parm::eTextureFloat:
				{
					Attrs::PluginDesc mtlOverrideDesc(VRayExporter::getPluginName(&vopNode, attrName), "TexUserScalar");
					mtlOverrideDesc.addAttribute(Attrs::PluginAttr("default_value", shopNode->evalFloat(&shopPrm, 0, getContext().getTime())));
					mtlOverrideDesc.addAttribute(Attrs::PluginAttr("user_attribute", shopPrm.getToken()));

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
					for (int i = 0; i < std::min(shopPrm.getVectorSize(), 4); ++i) {
						attr.paramValue.valVector[i] = shopNode->evalFloat(&shopPrm, i, getContext().getTime());
					}
					mtlOverrideDesc.addAttribute(attr);
					mtlOverrideDesc.addAttribute(Attrs::PluginAttr("user_attribute", shopPrm.getToken()));

					VRay::Plugin mtlOverridePlg = exportPlugin(mtlOverrideDesc);
					pluginDesc.addAttribute(Attrs::PluginAttr(attrName, mtlOverridePlg, "color"));

					break;
				}
				default:
				// ignore other types for now
					;
			}
		}
	}
}
