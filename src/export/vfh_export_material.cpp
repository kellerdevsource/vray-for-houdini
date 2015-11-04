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
				OP_PropertyLookupList props;

				const PRM_ParmList *plist = obj.getParmList();
				const int numObjParams = plist->getEntries();

				for(int pi = 0; pi < numObjParams; pi++) {
					const PRM_Parm      *parm = plist->getParmPtr(pi);
					const PRM_SpareData	*spare = parm->getSparePtr();
					if (parm && spare && !parm->getBypassFlag()) {
						if (spare->getValue(OBJ_MATERIAL_SPARE_TAG)) {
							Log::getLog().msg("Material override parameter: \"%s\"",
											  parm->getToken());

							props.addParm(parm->getToken(), &obj, nullptr);

							OP_Node *fromNode = nullptr;
							PRM_Parm *fromParm = nullptr;
							static_cast<OP_Node&>(obj).getParameterOrProperty(parm->getToken(), now, fromNode, fromParm, false);
							if (fromNode) {
								Log::getLog().msg("  fromNode: %s", fromNode->getName().buffer());
							}
							if (fromParm) {
								Log::getLog().msg("  fromParm: %s", fromParm->getToken());
							}
#if 0
							const PRM_Parm *shopParm = Parm::getParm(*shopNode, parm->getToken());
							if (shopParm) {
								// const PRM_SpareData	*shopParmSpare = shopParm->getSparePtr();
								// if (shopParmSpare) {
								// }
							}
#endif
						}
					}
				}

				shopNode->findParametersOrProperties(now, props);
				for (int i = 0; i < props.entries(); ++i) {
					auto node = props.getNode(i);
					auto parm = props.getParmPtr(i);
					if (parm) {
						Log::getLog().msg("Prop[%d] %s := %s %s",
										  i,
										  props.getName(i),
										  node ? node->getName().buffer() : "<missing>",
										  parm ? parm->getToken() : "<missing>");

						const PRM_SpareData	*spare = parm->getSparePtr();
						if (spare && !parm->getBypassFlag()) {
							OP_Node *fromNode = nullptr;
							PRM_Parm *fromParm = nullptr;
							shopNode->getParameterOrProperty(parm->getToken(), now, fromNode, fromParm, false);
							if (fromNode) {
								Log::getLog().msg("  fromNode: %s", fromNode->getName().buffer());
							}
							if (fromParm) {
								Log::getLog().msg("  fromParm: %s", fromParm->getToken());
							}
						}
					}
				}

				Log::getLog().msg(" SHOP hasOpDependents: %i", !shopNode->getOpDependents().isEmpty());
				Log::getLog().msg(" OBJ  hasOpDependents: %i", !obj.getOpDependents().isEmpty());

#define in :

#if 0
				OP_NodeList deps;
				shopNode->getExistingOpDependents(deps, false);
				for (const auto &dep in deps) {
					Log::getLog().msg("  Dependents: %s", dep->getName().buffer());
				}
#endif
#if 1
				const OP_DependencyList &deps = shopNode->getOpDependents();
				for (const auto &dep in deps) {
					const PRM_RefId &getRefId = dep.getRefId();
					const PRM_RefId &getSourceRefId = dep.getSourceRefId();

					Log::getLog().msg("  dep.getRefId().getParmRef() = %i | dep.getSourceRefId().getParmRef() = %i",
									  getRefId.getParmRef(), getSourceRefId.getParmRef());

//					const int getParmRef = getRefId.getParmRef();
//					const PRM_Parm &prm = shopNode->getParm(getParmRef);
//					Log::getLog().msg("  prm ref: %s",
//									  prm.getToken());
				}
#endif

				if (shopNode->hasMultiparmInfo()) {
					OP_MultiparmInfo &mpInfo = shopNode->getMultiparmInfo();
					Log::getLog().msg(" mpInfo: %i",
									  mpInfo.entries());
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
