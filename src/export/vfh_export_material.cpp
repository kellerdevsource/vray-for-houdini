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

#include <PRM/PRM_ParmMicroNode.h>


using namespace VRayForHoudini;


struct MaterialOverrideLink {
	MaterialOverrideLink(OP_Node &fromObject, const std::string &fromAttr, OP_Node &targetVOP)
		: fromObject(fromObject)
		, fromAttr(fromAttr)
		, targetVOP(targetVOP)
	{}

	OP_Node     &fromObject; // Object that overrides the attribute value
	std::string  fromAttr;   // Object's attrubute name (could be different from target VOP name)
	OP_Node     &targetVOP;  // Target VOP to override property on
};

// Key: Target VOP attribute name
typedef std::map<std::string, MaterialOverrideLink>  MaterialOverrideLinks;


int MtlContext::hasMaterialOverrides()
{
	int hasMtlOverrides = false;

	if (m_object) {
		SOP_Node *sopNode = m_object->getRenderSopPtr();
		if (sopNode) {
			OP_Context ctx; // NOTE: May be use context from exporter?
			GU_DetailHandleAutoReadLock gdl(sopNode->getCookedGeoHandle(ctx));

			const GU_Detail *gdp = gdl.getGdp();
			if (gdp) {
				GA_ROAttributeRef materialOverridesHandle = gdp->findStringTuple(GA_ATTRIB_PRIMITIVE, "material_overrides");
				if (materialOverridesHandle.isValid()) {
					hasMtlOverrides = true;
				}
			}
		}
	}

	return hasMtlOverrides;
}


int MtlContext::hasMaterialPromotes()
{
	int hasMtlPromotes = false;

	if (m_object) {
		// ...
	}

	return hasMtlPromotes;
}


VRay::Plugin VRayExporter::exportMaterial(SHOP_Node *shop_node, MtlContext ctx)
{
	VRay::Plugin material;

	OP_Node *op_node = VRayExporter::FindChildNodeByType(shop_node, "vray_material_output");
	if (!op_node) {
		Log::getLog().error("Can't find \"V-Ray Material Output\" operator under \"%s\"!",
							shop_node->getName().buffer());
	}
	else {
		VOP::MaterialOutput *mtl_out = static_cast<VOP::MaterialOutput *>(op_node);
		addOpCallback(mtl_out, VRayExporter::RtCallbackSurfaceShop);

		if (ctx.getObject()) {
			const fpreal now = m_context.getTime();

			OBJ_Node &obj = *ctx.getObject();

			SHOP_Node *shopNode = getObjMaterial(&obj, now);
			if (shopNode) {
				// Collect material override parameters
				MaterialOverrideLinks mtlOverLinks;

				const PRM_ParmList *objParmList  = obj.getParmList();
				PRM_ParmList       *shopParmList = shopNode->getParmList();

				if (objParmList && shopParmList) {
					for(int pi = 0; pi < objParmList->getEntries(); pi++) {
						const PRM_Parm *parm = objParmList->getParmPtr(pi);
						if (parm && !parm->getBypassFlag()) {
							const PRM_SpareData	*spare = parm->getSparePtr();
							if (spare) {
								// If the parameter is for material override it has OBJ_MATERIAL_SPARE_TAG tag
								if (spare->getValue(OBJ_MATERIAL_SPARE_TAG)) {
									std::string propOverName(parm->getToken());

									int componentIndex = 0;

									// We are interested in a property name (like "diffuse") not component name (like "diffuser"),
									// so if the property is a compound value (like color) ||| append component suffix to get
									// the correct reference (because references are per-component).
									if (parm->getVectorSize() > 1) {
										propOverName += "r";
										// We don't care about the actual index
										componentIndex = 0;
									}

									// Property index on a SHOP
									const int propOverParmIdx = shopParmList->getParmIndex(propOverName.c_str());

									printf("propOverParmIdx %i\n",
										   propOverParmIdx);
									if (propOverParmIdx >= 0) {
										DEP_MicroNode &src = shopParmList->parmMicroNode(propOverParmIdx, componentIndex);

										DEP_MicroNodeList outputs;
										src.getOutputs(outputs);
										for (int i = 0; i < outputs.entries(); ++i) {
											PRM_ParmMicroNode *micronode = dynamic_cast<PRM_ParmMicroNode*>(outputs(i));
											if (micronode) {
												const PRM_Parm &referringParm = micronode->ownerParm();
												PRM_ParmOwner  *referringParmOwner = referringParm.getParmOwner();
												if (referringParmOwner) {
													OP_Node *referringNode = referringParmOwner->castToOPNode();
													if (referringNode) {
														// Adding material override link
														mtlOverLinks.emplace(std::piecewise_construct,
																			 std::forward_as_tuple(referringParm.getToken()),
																			 std::forward_as_tuple(static_cast<OP_Node&>(obj), propOverName, *referringNode));
														break;
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}

				for (const auto &mtlOverLink : mtlOverLinks) {
					Log::getLog().msg("  Object's \'%s\' prop \'%s\' overrides \'%s\' from \'%s\'",
									  mtlOverLink.second.fromObject.getName().buffer(),
									  mtlOverLink.second.fromAttr.c_str(),
									  mtlOverLink.first.c_str(),
									  mtlOverLink.second.targetVOP.getName().buffer());
				}
			}
		}

		if (mtl_out->error() < UT_ERROR_ABORT ) {
			Log::getLog().info("Exporting material output \"%s\"...",
							   mtl_out->getName().buffer());

			const int idx = mtl_out->getInputFromName("Material");
			VOP::NodeBase *input = dynamic_cast<VOP::NodeBase*>(mtl_out->getInput(idx));
			if (input) {
				switch (mtl_out->getInputType(idx)) {
					case VOP_SURFACE_SHADER: {
						material = exportVop(input);
						break;
					}
					case VOP_TYPE_BSDF: {
						VRay::Plugin pluginBRDF = exportVop(input);

						// Wrap BRDF into MtlSingleBRDF for RT GPU to work properly
						Attrs::PluginDesc mtlPluginDesc(VRayExporter::getPluginName(input, "Mtl@"), "MtlSingleBRDF");
						mtlPluginDesc.addAttribute(Attrs::PluginAttr("brdf", pluginBRDF));

						material = exportPlugin(mtlPluginDesc);
						break;
					}
					default:
						Log::getLog().error("Unsupported input type for node \"%s\", input %d!",
											mtl_out->getName().buffer(), idx);
				}

				if (material) {
					// Wrap material into MtlRenderStats to always have the same material name
					// Used when rewiring materials when running interactive RT session
					// TODO: Do not use for non-interactive export
					Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(mtl_out->getParent(), "Mtl@"), "MtlRenderStats");
					pluginDesc.addAttribute(Attrs::PluginAttr("base_mtl", material));
					material = exportPlugin(pluginDesc);
				}
			}
		}
	}

	return material;
}
