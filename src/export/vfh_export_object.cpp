//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "sop/sop_vrayproxy.h"
#include "sop/sop_vrayscene.h"
#include "vop/vop_node_base.h"

#include "vfh_exporter.h"
#include "vfh_prm_templates.h"

#include <SHOP/SHOP_Node.h>
#include <PRM/PRM_Parm.h>

#include <boost/algorithm/string.hpp>


using namespace VRayForHoudini;


SHOP_Node *VRayExporter::getObjMaterial(OBJ_Node *obj_node, fpreal t)
{
	SHOP_Node *shop_node = nullptr;

#if UT_MAJOR_VERSION_INT >= 15
	OP_Node *op_node = obj_node->getMaterialNode(t);
	if (op_node) {
		shop_node = op_node->castToSHOPNode();
	}
#else
	shop_node = obj_node->getMaterialNode(t);
#endif

	return shop_node;
}


int isSmoothed(OBJ_Node &obj_node)
{
	bool res = false;
	bool hasDispl = Parm::isParmExist(obj_node, "vray_use_displ") && obj_node.evalInt("vray_use_displ", 0, 0.0);
	if (hasDispl) {
		const int displType = obj_node.evalInt("vray_displ_type", 0, 0.0);
		switch (displType) {
			// from shopnet
			case 0:
			{
				UT_String shopPath;
				obj_node.evalString(shopPath, "vray_displshoppath", 0, 0.0);
				SHOP_Node *shop_node = OPgetDirector()->findSHOPNode(shopPath.buffer());
				if (shop_node) {
					OP_Node *op_node = VRayExporter::FindChildNodeByType(shop_node, "vray_material_output");
					if (   op_node
						&& op_node->error() < UT_ERROR_ABORT)
					{
						const int idx = op_node->getInputFromName("Geometry");
						VOP::NodeBase *input = dynamic_cast<VOP::NodeBase*>(op_node->getInput(idx));
						if (   input
							&& input->getVRayPluginID() == "GeomStaticSmoothedMesh")
						{
							res = true;
						}
					}
				}
				break;
			}
			case 2:
			{
				res = true;
			}
			default:
				break;
		}
	}
	return res;
}


void VRayExporter::RtCallbackNode(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

	OBJ_Node *obj_node = caller->castToOBJNode();

	Log::getLog().debug("RtCallbackNode: %s from \"%s\"", OPeventToString(type), obj_node->getName().buffer());

	if (type == OP_PARM_CHANGED ||
		type == OP_INPUT_CHANGED ||
		type == OP_INPUT_REWIRED || /* parenting */
		type == OP_FLAG_CHANGED) /* visibility */
	{
		VRay::Plugin mtl;

		const PRM_Parm *param = Parm::getParm(*caller, reinterpret_cast<long>(data));
		if (param) {
			// TODO: Use obj->getMaterialParmToken() instead of using "shop_materialpath" directly
			//
			if (boost::equals(param->getToken(), "shop_materialpath")) {
				SHOP_Node *shop_node = exporter.getObjMaterial(obj_node);
				if (shop_node) {
					MtlContext ctx(obj_node);
					mtl = exporter.exportMaterial(shop_node, ctx);
				}
				if (!mtl) {
					mtl = exporter.exportDefaultMaterial();
				}
			}
		}

		exporter.exportNode(obj_node, mtl, VRay::Plugin());
	}
	else if (type == OP_NODE_PREDELETE) {
		exporter.delOpCallbacks(caller);
		exporter.removePlugin(obj_node);
	}
}


void VRayExporter::RtCallbackNodeData(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

	Log::getLog().debug("RtCallbackNodeData: %s from \"%s\"", OPeventToString(type), caller->getName().buffer());

	if (type == OP_PARM_CHANGED ||
		type == OP_INPUT_CHANGED ||
		// TODO: Improve handling by checking the exact flag if possible
		type == OP_FLAG_CHANGED ||
		type == OP_INPUT_REWIRED)
	{
		OP_Network *parent = caller->getParent();
		if (parent) {
			OBJ_Node *obj_node = parent->castToOBJNode();
			if (obj_node) {
				exporter.exportObject(obj_node);
			}
		}
	}
	else if (type == OP_NODE_PREDELETE) {
		exporter.delOpCallbacks(caller);
	}
}


VRay::Plugin VRayExporter::exportNode(OBJ_Node *obj_node, VRay::Plugin material, VRay::Plugin geometry)
{
	VRay::Plugin nodePlugin;

	if (obj_node) {
		SOP_Node *geomNode = obj_node->getRenderSopPtr();
		if (geomNode) {
			addOpCallback(obj_node, VRayExporter::RtCallbackNode);

			OP_Operator     *geomOp     = geomNode->getOperator();
			const UT_String &geomOpName = geomOp->getName();

			bool flipTm = false;
			if (geomOpName.equal("VRayNodeGeomPlane")) {
				flipTm = true;
			}

			Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(obj_node), "Node");
			if (geometry) {
				pluginDesc.addAttribute(Attrs::PluginAttr("geometry", geometry));
			}
			if (material) {
				pluginDesc.addAttribute(Attrs::PluginAttr("material", material));
			}

			pluginDesc.addAttribute(Attrs::PluginAttr("transform",
													  VRayExporter::getObjTransform(obj_node, m_context, flipTm)));

			pluginDesc.addAttribute(Attrs::PluginAttr("visible",
													  obj_node->getVisible()));

			nodePlugin = exportPlugin(pluginDesc);
		}
	}

	return nodePlugin;
}


VRay::Plugin VRayExporter::exportNodeData(SOP_Node *geom_node, GeomExportParams &expParams)
{
	VRay::Plugin geom;
	if (geom_node && isNodeAnimated(geom_node)) {
		addOpCallback(geom_node, VRayExporter::RtCallbackNodeData);

		OP_Operator     *geomOp     = geom_node->getOperator();
		const UT_String &geomOpName = geomOp->getName();

		Attrs::PluginDesc geomPluginDesc;

		if (geomOpName.startsWith("VRayNode")) {
			SOP::NodeBase *vrayNode = static_cast<SOP::NodeBase*>(geom_node);

			OP_Node *op_node = static_cast<OP_Node*>(geom_node);

			OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(geomPluginDesc, *this, static_cast<OP_Node*>(geom_node->getParent()));
			if (res == OP::VRayNode::PluginResultError) {
				Log::getLog().error("Error creating plugin descripion for node: \"%s\" [%s]",
							op_node->getName().buffer(),
							geomOpName.buffer());
			}
			else if (res == OP::VRayNode::PluginResultNA ||
					 res == OP::VRayNode::PluginResultContinue)
			{
				setAttrsFromOpNode(geomPluginDesc, op_node);
			}

			geom = exportPlugin(geomPluginDesc);
		}
#if 0
		else if (geomOpName.equal("dopio")) {
			geom = exportParticles(obj_node);
		}
#endif
		else {
			GU_DetailHandleAutoReadLock gdl(geom_node->getCookedGeoHandle(m_context));
			const GU_Detail *gdp = gdl.getGdp();

			// NOTE: Could happen, for example, with file node when file is
			// missing
			if (NOT(gdp)) {
				Log::getLog().error("Incorrect geometry detail!");
			}
			else {
				// NOTE: Find the correct way to detect fur...
				//
				GA_ROAttributeRef ref_guardhair(gdp->findAttribute(GA_ATTRIB_PRIMITIVE, "guardhair"));
				const GA_ROHandleI hnd_guardhair(ref_guardhair.getAttribute());

				GA_ROAttributeRef ref_hairid(gdp->findAttribute(GA_ATTRIB_PRIMITIVE, "hairid"));
				const GA_ROHandleI hnd_hairid(ref_hairid.getAttribute());

				if (hnd_guardhair.isValid() && hnd_hairid .isValid()) {
					geom = exportGeomMayaHair(geom_node, gdp);
				}
				else {
					geom = exportGeomStaticMesh(*geom_node, *gdp, expParams);
				}
			}
		}
	}

	return geom;
}


VRay::Plugin VRayExporter::exportObject(OBJ_Node *obj_node)
{
	const fpreal t = m_context.getTime();

	VRay::Plugin obj_plugin = VRay::Plugin();

	SOP_Node *geom_node = obj_node->getRenderSopPtr();
	if (!geom_node) {
		Log::getLog().error("OBJ \"%s\": Render SOP is not found!",
					obj_node->getName().buffer());
	}
	else {
		OP_Operator *geom_op = geom_node->getOperator();

		const UT_String &geomOpName = geom_op->getName();

		Log::getLog().info("  Render SOP: %s:\"%s\"",
				   geom_op->getName().buffer(),
				   obj_node->getName().buffer());

		if (geomOpName.equal("VRayNodeVRayScene")) {
#ifdef CGR_HAS_VRAYSCENE
			obj_plugin = exportVRayScene(obj_node, geom_node);
#endif
		}
		else {
			GeomExportParams expParams;
			expParams.uvWeldThreshold = isSmoothed(*obj_node)? expParams.uvWeldThreshold: -1.f;

			VRay::Plugin geom = exportNodeData(geom_node, expParams);

			if (geom) {
				MtlContext   mtlCtx(obj_node);
				VRay::Plugin mtl;

				SHOP_Node *shop_node = getObjMaterial(obj_node, t);
				if (shop_node) {
					Log::getLog().info("  Found material: \"%s\" [%s]",
							   shop_node->getName().buffer(), shop_node->getOperator()->getName().buffer());

					MtlContext ctx(obj_node);
					mtl = exportMaterial(shop_node, ctx);
				}
				else if (expParams.shopToID.size()) {
					if (expParams.shopToID.size() == 1) {
						OP_Node *op_node = OPgetDirector()->findNode(expParams.shopToID.begin().key());
						if (op_node) {
							mtl = exportMaterial(op_node->castToSHOPNode(), mtlCtx);
						}
					}
					else {
						Attrs::PluginDesc mtlMultiDesc(VRayExporter::getPluginName(geom_node, "Mtl"), "MtlMulti");

						VRay::ValueList mtls_list;
						VRay::IntList   ids_list;

						Log::getLog().info("Adding MtlMulti:");

						for (SHOPToID::iterator oIt = expParams.shopToID.begin(); oIt != expParams.shopToID.end(); ++oIt) {
							const char *shop_materialpath = oIt.key();

							OP_Node *op_node = OPgetDirector()->findNode(shop_materialpath);
							if (op_node) {
								const int &material_id = oIt.data();

								Log::getLog().info(" %i: \"%s\"",
										   material_id, shop_materialpath);

								mtls_list.push_back(VRay::Value(exportMaterial(op_node->castToSHOPNode(), mtlCtx)));
								ids_list.push_back(material_id);
							}
						}

						mtlMultiDesc.addAttribute(Attrs::PluginAttr("mtls_list", mtls_list));
						mtlMultiDesc.addAttribute(Attrs::PluginAttr("ids_list",  ids_list));

						mtl = exportPlugin(mtlMultiDesc);
					}
				}

				VRay::Plugin geomDispl = exportDisplacement(obj_node, geom);
				if (geomDispl) {
					geom = geomDispl;
				}

				// Export default grey material
				if (NOT(mtl)) {
					mtl = exportDefaultMaterial();
				}

				exportNode(obj_node, mtl, geom);
			}
		}
	}

	return obj_plugin;
}
