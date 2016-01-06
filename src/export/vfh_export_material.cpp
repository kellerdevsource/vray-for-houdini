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

		SHOPExportContext mtlContext(*this, shop_node, parentContext);
		ECFnSHOPOverrides fnSHOPOverrides(&mtlContext);
		fnSHOPOverrides.initOverrides();

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
							material = exportVop(vopNode, &mtlContext);
							break;
						}
						case VOP_TYPE_BSDF: {
							VRay::Plugin pluginBRDF = exportVop(vopNode, &mtlContext);

							// Wrap BRDF into MtlSingleBRDF for RT GPU to work properly
							Attrs::PluginDesc mtlPluginDesc(VRayExporter::getPluginName(vopNode, "Mtl@"), "MtlSingleBRDF");
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
						Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(mtlOut->getParent(), "Mtl@"), "MtlRenderStats");
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


void VRayExporter::setAttrsFromSHOPOverrides(Attrs::PluginDesc &pluginDesc, VOP_Node &vopNode, ECFnSHOPOverrides &mtlContext)
{
	vassert( mtlContext.isValid() );
	vassert( mtlContext.hasOverrides() );
	vassert( mtlContext.hasOverrides(vopNode) );

	const Parm::VRayPluginInfo *pluginInfo = Parm::GetVRayPluginInfo( pluginDesc.pluginID );
	if (!pluginInfo) {
		return;
	}

	MTLOverrideType overrideType = mtlContext.getOverrideType();
	// we have override
	for (const auto &aIt : pluginInfo->attributes) {
		const std::string    &attrName = aIt.first;
		const Parm::AttrDesc &attrDesc = aIt.second;

		int inpIdx = vopNode.getInputFromName(attrName.c_str());
		OP_Node *inpNode = vopNode.getInput(inpIdx);
		if (inpNode) {
			// if we have connected input for this attribute skip override
			// inputs take priority over everything else
			continue;
		}

		switch (overrideType) {
			case MTLO_OBJ:
			// MTLO_OBJ = override value is taken from a param on the corresponding object node
			{
				std::string overridingPrmName;
				if (mtlContext.getOverrideName(vopNode, attrName, overridingPrmName)) {
					setAttrValueFromOpNode(pluginDesc, attrDesc, *mtlContext.getObjectNode(), overridingPrmName);
				}
				break;
			}
			case MTLO_GEO:
			// MTLO_GEO = override value is taken from a map channel on the geometry
			{
				std::string overridingChName;
				if (mtlContext.getOverrideName(vopNode, attrName, overridingChName))
				{
					OBJ_Node *objNode = mtlContext.getObjectNode();
					switch (attrDesc.value.type) {
						case Parm::eTextureInt:
						{
							Attrs::PluginDesc mtlOverrideDesc;
							mtlOverrideDesc.pluginName = VRayExporter::getPluginName(&vopNode, "VOPOverride@", objNode->getName().toStdString());
							mtlOverrideDesc.pluginID = "TexUserScalar";
							mtlOverrideDesc.addAttribute(Attrs::PluginAttr("user_attribute", overridingChName));
							VRay::Plugin mtlOverridePlg = exportPlugin(mtlOverrideDesc);
							pluginDesc.addAttribute(Attrs::PluginAttr(attrName, mtlOverridePlg, "scalar"));

							break;
						}
						case Parm::eTextureFloat:
						{
							Attrs::PluginDesc mtlOverrideDesc;
							mtlOverrideDesc.pluginName = VRayExporter::getPluginName(&vopNode, "VOPOverride@", objNode->getName().toStdString());
							mtlOverrideDesc.pluginID = "TexUserScalar";
							mtlOverrideDesc.addAttribute(Attrs::PluginAttr("user_attribute", overridingChName));
							VRay::Plugin mtlOverridePlg = exportPlugin(mtlOverrideDesc);
							pluginDesc.addAttribute(Attrs::PluginAttr(attrName, mtlOverridePlg, "scalar"));

							break;
						}
						case Parm::eTextureColor:
						{
							Attrs::PluginDesc mtlOverrideDesc;
							mtlOverrideDesc.pluginName = VRayExporter::getPluginName(&vopNode, "VOPOverride@", objNode->getName().toStdString());
							mtlOverrideDesc.pluginID = "TexUserColor";
							mtlOverrideDesc.addAttribute(Attrs::PluginAttr("user_attribute", overridingChName));
							VRay::Plugin mtlOverridePlg = exportPlugin(mtlOverrideDesc);
							pluginDesc.addAttribute(Attrs::PluginAttr(attrName, mtlOverridePlg, "color"));

							break;
						}
						default:
						// ignore other types for now
							;
					}

				}
				break;
			}
			default:
				;
		}
	}
}
