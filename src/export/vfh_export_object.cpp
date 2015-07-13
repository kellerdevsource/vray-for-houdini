/*
 * Copyright (c) 2015, Chaos Software Ltd
 *
 * V-Ray For Houdini
 *
 * Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sop/sop_vrayproxy.h"
#include "sop/sop_vrayscene.h"
#include "vfh_exporter.h"

#include <SHOP/SHOP_Node.h>


using namespace VRayForHoudini;


void VRayExporter::RtCallbackNode(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter *exporter = (VRayExporter*)callee;

	PRINT_INFO("RtCallbackNode: %s from \"%s\"",
			   OPeventToString(type), caller->getName().buffer());

	if (   type == OP_PARM_CHANGED
		|| type == OP_INPUT_CHANGED
		|| type == OP_INPUT_REWIRED /* parenting  */
		|| type == OP_FLAG_CHANGED  /* visibility */
		)
	{
		exporter->exportNode(caller->castToOBJNode(), VRay::Plugin(), VRay::Plugin());
	}
	else if (type == OP_NODE_PREDELETE) {
		// TODO: Remove object for RT
		caller->removeOpInterest(exporter, VRayExporter::RtCallbackNode);
	}
}


void VRayExporter::RtCallbackNodeData(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter *exporter = (VRayExporter*)callee;

	PRINT_INFO("RtCallbackNodeData: %s from \"%s\"",
			   OPeventToString(type), caller->getName().buffer());

	if (   type == OP_PARM_CHANGED
		|| type == OP_INPUT_CHANGED
		|| type == OP_INPUT_REWIRED
		)
	{
		OP_Network *parent = caller->getParent();
		if (parent) {
			OBJ_Node *obj_node = parent->castToOBJNode();
			if (obj_node) {
				exporter->exportObject(obj_node);
			}
		}
	}
	else if (type == OP_NODE_PREDELETE) {
		caller->removeOpInterest(exporter, VRayExporter::RtCallbackNodeData);
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

			Attrs::PluginDesc pluginDesc(obj_node, "Node", "Node@");
			if (geometry) {
				pluginDesc.addAttribute(Attrs::PluginAttr("geometry", geometry));
			}
			if (material) {
				pluginDesc.addAttribute(Attrs::PluginAttr("material", material));
			}

			pluginDesc.addAttribute(Attrs::PluginAttr("transform",
													  VRayExporter::GetOBJTransform(obj_node, m_context, flipTm)));

			nodePlugin = exportPlugin(pluginDesc);
		}
	}

	return nodePlugin;
}


VRay::Plugin VRayExporter::exportNodeData(SOP_Node *geom_node, SHOPToID &shopToID)
{
	VRay::Plugin geom;
	if (geom_node && processAnimatedNode(geom_node)) {
		addOpCallback(geom_node, VRayExporter::RtCallbackNodeData);

		OP_Operator     *geomOp     = geom_node->getOperator();
		const UT_String &geomOpName = geomOp->getName();

		Attrs::PluginDesc geomPluginDesc;

		if (geomOpName.startsWith("VRayNode")) {
			if (geomOpName.equal("VRayNodeGeomDisplacedMesh") ||
				geomOpName.equal("VRayNodeGeomStaticSmoothedMesh"))
			{
				OP_Node *mesh_op_node = geom_node->getInput(0);
				if (NOT(mesh_op_node)) {
					PRINT_ERROR("Geometry node is not connected!");
				}
				else {
					SOP_Node *mesh_node = mesh_op_node->castToSOPNode();

					// Export base mesh
					GU_DetailHandleAutoReadLock gdl(mesh_node->getCookedGeoHandle(m_context));
					const GU_Detail *gdp = gdl.getGdp();

					VRay::Plugin mesh = exportGeomStaticMesh(mesh_node, gdp, shopToID);
					// NOTE: Could be actually empty if mesh is not animated and we're exporting
					// animation. Real errors are checked inside.
					if (mesh) {
						geomPluginDesc.addAttribute(Attrs::PluginAttr("mesh", mesh));
					}
				}
			}

			SOP::NodeBase *vrayNode = static_cast<SOP::NodeBase*>(geom_node);

			OP_Node *op_node = static_cast<OP_Node*>(geom_node);

			// OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(geomPluginDesc, this, static_cast<OP_Node*>(obj_node));
			OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(geomPluginDesc, this, static_cast<OP_Node*>(geom_node->getParent()));
			if (res == OP::VRayNode::PluginResultError) {
				PRINT_ERROR("Error creating plugin descripion for node: \"%s\" [%s]",
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
				PRINT_ERROR("Incorrect geometry detail!");
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
					geom = exportGeomStaticMesh(geom_node, gdp, shopToID);
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
	if (geom_node) {
		OP_Operator *geom_op = geom_node->getOperator();

		const UT_String &geomOpName = geom_op->getName();

		PRINT_INFO("  Render SOP: %s:\"%s\"",
				   geom_op->getName().buffer(),
				   obj_node->getName().buffer());

		if (geomOpName.equal("VRayNodeVRayScene")) {
#ifdef CGR_HAS_VRAYSCENE
			obj_plugin = exportVRayScene(obj_node, geom_node);
#endif
		}
		else {
			VRayForHoudini::SHOPToID shopToID;
			VRay::Plugin geom = exportNodeData(geom_node, shopToID);
			if (geom) {
				VRay::Plugin mtl;

				SHOP_Node *shop_node = obj_node->getMaterialNode(t);
				if (shop_node) {
					PRINT_INFO("  Found material: \"%s\" [%s]",
							   shop_node->getName().buffer(), shop_node->getOperator()->getName().buffer());

					const UT_String &shopOpName = shop_node->getOperator()->getName();

					if (shopOpName.equal("material")) {
						Attrs::PluginDesc mtlMultiDesc(shop_node, "MtlMulti");

						const int numMaterials = shop_node->evalInt("num_materials", 0, 0.0);

						VRay::ValueList mtls_list;
						VRay::ValueList ids_list;

						for (int i = 1; i <= numMaterials; ++i) {
							UT_String mtl_name;
							shop_node->evalStringInst("shop_materialpath#", &i, mtl_name, 0, 0.0);

							if (mtl_name.buffer()) {
								OP_Node *op_node = OPgetDirector()->findNode(mtl_name.buffer());

								mtls_list.push_back(VRay::Value(exportMaterial(op_node->castToSHOPNode())));
							}
						}

						mtlMultiDesc.addAttribute(Attrs::PluginAttr("mtls_list", mtls_list));
						mtlMultiDesc.addAttribute(Attrs::PluginAttr("ids_list",  ids_list));

						mtl = exportPlugin(mtlMultiDesc);
					}
					else {
						mtl = exportMaterial(shop_node);
					}
				}
				else if (shopToID.size()) {
					if (shopToID.size() == 1) {
						OP_Node *op_node = OPgetDirector()->findNode(shopToID.begin().key());
						if (op_node) {
							mtl = exportMaterial(op_node->castToSHOPNode());
						}
					}
					else {
						Attrs::PluginDesc mtlMultiDesc(geom_node, "MtlMulti", "Mtl@");

						VRay::ValueList mtls_list;
						VRay::IntList   ids_list;

						PRINT_INFO("Adding MtlMulti:");

						for (SHOPToID::iterator oIt = shopToID.begin(); oIt != shopToID.end(); ++oIt) {
							const char *shop_materialpath = oIt.key();

							OP_Node *op_node = OPgetDirector()->findNode(shop_materialpath);
							if (op_node) {
								const int &material_id = oIt.data();

								PRINT_INFO(" %i: \"%s\"",
										   material_id, shop_materialpath);

								mtls_list.push_back(VRay::Value(exportMaterial(op_node->castToSHOPNode())));
								ids_list.push_back(material_id);
							}
						}

						mtlMultiDesc.addAttribute(Attrs::PluginAttr("mtls_list", mtls_list));
						mtlMultiDesc.addAttribute(Attrs::PluginAttr("ids_list",  ids_list));

						mtl = exportPlugin(mtlMultiDesc);
					}
				}

				// Export default grey material
				if (NOT(mtl)) {
					Attrs::PluginDesc pluginDesc("BRDFDiffuse@Clay", "BRDFDiffuse");
					pluginDesc.addAttribute(Attrs::PluginAttr("color", 0.5f, 0.5f, 0.5f));
					mtl = exportPlugin(pluginDesc);
				}

				exportNode(obj_node, mtl, geom);
			}
		}
	}

	return obj_plugin;
}
