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
#include "obj/obj_node_base.h"
#include "vop/vop_node_base.h"
#include "vop/material/vop_mtl_def.h"

#include <SHOP/SHOP_Node.h>
#include <SOP/SOP_Node.h>
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
#if 0
			OBJ_Node *obj = ctx.getObject();

			Log::getLog().msg("obj->getMaterialParmToken() \"%s\"",
							  obj->getMaterialParmToken());
#if 1
			UT_StringArray outVars;
			obj->getLocalVarNames(outVars);
			for (const auto &outVar : outVars) {
				Log::getLog().msg("Local var: \"%s\"",
								  outVar.buffer());
			}
#endif
			PRM_Template *tmpl = obj->getOperator()->getParmTemplates();
			while(tmpl->getType() != PRM_LIST_TERMINATOR) {
				tmpl->ta
				tmpl++;
			}

			for (int i = 0; i < obj->getNumParms(); ++i) {
				const PRM_Parm &parm = obj->getParm(i);
				const PRM_SpareData *spareData = parm.getSparePtr();
				if (spareData) {
					// spareData->get
				}
#if 0
				Log::getLog().msg("Parm: \"%s\" override %s",
								  parm.getOverride(0));

				PRM_ParmOwner *parmOwner = parm.getParmOwner();
				if (parmOwner) {
					Log::getLog().msg("Parm: \"%s\" owned by \"%s\" is pending override %i",
									  parm.getToken(),
									  parmOwner->getFullPath().c_str(),
									  parmOwner->isParmPendingOverride(nullptr, 0));
				}
#endif
			}
#endif
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

