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
#include "vfh_export_geom.h"

#include <SHOP/SHOP_Node.h>
#include <PRM/PRM_Parm.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_BundleList.h>

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


//void VRayExporter::RtCallbackNode(OP_Node *caller, void *callee, OP_EventType type, void *data)
//{
//	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);
//	OBJ_Node *obj_node = caller->castToOBJNode();

//	Log::getLog().debug("RtCallbackNode: %s from \"%s\"", OPeventToString(type), obj_node->getName().buffer());

//	switch (type) {
//		case OP_PARM_CHANGED:
//		{
//			VRay::Plugin mtl;
//			const PRM_Parm *objPrm = Parm::getParm(*caller, reinterpret_cast<long>(data));
//			if (objPrm) {
//				SHOP_Node *shop_node = exporter.getObjMaterial(obj_node);
//				if (shop_node) {
//					if (boost::equals(objPrm->getToken(), "shop_materialpath")) {
//						ExportContext expContext(CT_OBJ, exporter, *obj_node);
//						mtl = exporter.exportMaterial(*shop_node, expContext);
//						if (!mtl) {
//							mtl = exporter.exportDefaultMaterial();
//						}
//					}
//					else {
//						PRM_Parm *shopPrm = shop_node->getParmList()->getParmPtr(objPrm->getToken());

//						if (shopPrm && (objPrm->getType() == shopPrm->getType())) {
//							ExportContext expContext(CT_OBJ, exporter, *obj_node);
//							mtl = exporter.exportMaterial(*shop_node, expContext);
//						}
//					}

//				}
//				exporter.exportNode(obj_node, mtl, VRay::Plugin());
//			}
//		}
//		case OP_INPUT_CHANGED:
//		case OP_INPUT_REWIRED:
//		case OP_FLAG_CHANGED:
//		{
//			exporter.exportNode(obj_node, VRay::Plugin(), VRay::Plugin());
//			break;
//		}
//		case OP_NODE_PREDELETE:
//		{
//			exporter.delOpCallbacks(caller);
//			exporter.removePlugin(obj_node);
//			break;
//		}
//		default:
//			break;
//	}
//}


void VRayExporter::RtCallbackOBJGeometry(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);
	OBJ_Node *obj_node = caller->castToOBJNode();

	Log::getLog().debug("RtCallbackOBJGeometry: %s from \"%s\"", OPeventToString(type), caller->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED:
		// TODO: Improve handling by checking the exact flag if possible
		case OP_FLAG_CHANGED:
		case OP_INPUT_CHANGED:
		case OP_INPUT_REWIRED:
		{
			exporter.exportObject(obj_node);
			break;
		}
		case OP_NODE_PREDELETE:
		{
			exporter.delOpCallbacks(caller);
			break;
		}
		default:
			break;
	}
}


void VRayExporter::RtCallbackSOPChanged(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);
	OBJ_Node *obj_node = caller->getParent()->castToOBJNode();

	Log::getLog().debug("RtCallbackSOPChanged: %s from \"%s\"", OPeventToString(type), caller->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED:
		case OP_FLAG_CHANGED:
		case OP_INPUT_CHANGED:
		case OP_INPUT_REWIRED:
		{
			exporter.exportObject(obj_node);
			break;
		}
		case OP_NODE_PREDELETE:
		{
			exporter.delOpCallbacks(caller);
			break;
		}
		default:
			break;
	}
}


void VRayExporter::RtCallbackVRayClipper(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);
	OBJ_Node *clipperNode = caller->castToOBJNode();

	Log::getLog().debug("RtCallbackVRayClipper: %s from \"%s\"", OPeventToString(type), clipperNode->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED:
		case OP_FLAG_CHANGED:
		case OP_INPUT_CHANGED:
		case OP_INPUT_REWIRED:
		{
			exporter.exportVRayClipper(*clipperNode);
			break;
		}
		case OP_NODE_PREDELETE:
		{
			exporter.delOpCallbacks(caller);
			exporter.removePlugin(clipperNode);
			break;
		}
		default:
			break;
	}
}


//VRay::Plugin VRayExporter::exportNode(OBJ_Node *obj_node, VRay::Plugin material, VRay::Plugin geometry)
//{
//	VRay::Plugin nodePlugin;

//	if (obj_node) {
//		SOP_Node *geomNode = obj_node->getRenderSopPtr();
//		if (geomNode) {
//			addOpCallback(obj_node, VRayExporter::RtCallbackNode);

//			OP_Operator     *geomOp     = geomNode->getOperator();
//			const UT_String &geomOpName = geomOp->getName();

//			bool flipTm = false;
//			if (geomOpName.equal("VRayNodeGeomPlane")) {
//				flipTm = true;
//			}

//			Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(obj_node), "Node");
//			if (geometry) {
//				pluginDesc.addAttribute(Attrs::PluginAttr("geometry", geometry));
//			}
//			if (material) {
//				pluginDesc.addAttribute(Attrs::PluginAttr("material", material));
//			}

//			pluginDesc.addAttribute(Attrs::PluginAttr("transform",
//													  VRayExporter::getObjTransform(obj_node, m_context, flipTm)));

//			pluginDesc.addAttribute(Attrs::PluginAttr("visible",
//													  obj_node->getVisible()));

//			nodePlugin = exportPlugin(pluginDesc);
//		}
//	}

//	return nodePlugin;
//}



VRay::Plugin VRayExporter::exportObject(OBJ_Node *obj_node)
{
	VRay::Plugin plugin;
	if (NOT(obj_node)) {
		return plugin;
	}

	SOP_Node *geom_node = obj_node->getRenderSopPtr();
	if (!geom_node) {
		Log::getLog().error("OBJ \"%s\": Render SOP is not found!",
					obj_node->getName().buffer());
	}
	else {
		Log::getLog().info("  Render SOP: %s:\"%s\"",
				   geom_node->getOperator()->getName().buffer(),
				   obj_node->getName().buffer());

		if (obj_node->getOperator()->getName().equal("VRayNodeVRayClipper")) {
			plugin = exportVRayClipper(*obj_node);
		}
		else if (obj_node->getOperator()->getName().equal("VRayNodeVRayScene")) {
#ifdef CGR_HAS_VRAYSCENE
			plugin = exportVRayScene(obj_node, geom_node);
#endif
		}
		else {
			OBJ_Geometry *obj_geo = obj_node->castToOBJGeometry();
			if (obj_geo) {
				addOpCallback(obj_geo, VRayExporter::RtCallbackOBJGeometry);
				addOpCallback(geom_node, VRayExporter::RtCallbackSOPChanged);

				GeometryExporter geoExporter(*obj_geo, *this);
				int nPlugins = geoExporter.exportGeometry();
				for (int i = 0; i < nPlugins; ++i) {
					VRay::Plugin nodePlugin = exportPlugin(geoExporter.getPluginDescAt(i));
					if (NOT(plugin)) {
						plugin = nodePlugin;
					}
				}
			}
		}
	}

	return plugin;
}


VRay::Plugin VRayExporter::exportVRayClipper(OBJ_Node &clipperNode)
{
	addOpCallback(&clipperNode, VRayExporter::RtCallbackVRayClipper);
	fpreal t = getContext().getTime();

	// TODO: get node list from prim attribute
	Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(&clipperNode, ""), "VRayClipper");

	// find and export clipping geometry plugins
	UT_String nodePath;
	clipperNode.evalString(nodePath, "clip_mesh", 0, 0, t);
	OP_Node *opNode = OPgetDirector()->findNode(nodePath.buffer());
	VRay::Plugin clipNodePlugin;
	if (   opNode
		&& opNode->getOpTypeID() == OBJ_OPTYPE_ID
		&& opNode->getUniqueId() != clipperNode.getUniqueId())
	{
		OBJ_Node *objNode = opNode->castToOBJNode();
		if (objNode->getObjectType() == OBJ_GEOMETRY) {
			clipNodePlugin = exportObject(objNode);
		}
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("clip_mesh", clipNodePlugin));

	// find and export excussion node plugins
	UT_String nodeMask;
	clipperNode.evalString(nodeMask, "exclusion_nodes", 0, 0, t);

	// get a manager that contains objects
	OP_Network *objMan = OPgetDirector()->getManager("obj");

	UT_String bundle_name;
	OP_Bundle *bundle = OPgetDirector()->getBundles()->getPattern(bundle_name, objMan, objMan, nodeMask, "!!OBJ!!");
	// get the node list for processing
	OP_NodeList nodeList;
	bundle->getMembers(nodeList);
	// release the internal bundle created by getPattern()
	OPgetDirector()->getBundles()->deReferenceBundle(bundle_name);

	VRay::ValueList nodePluginList;
	nodePluginList.reserve(nodeList.size());
	for (OP_Node *node : nodeList) {
		OBJ_Node *objNode = node->castToOBJNode();
		if (   NOT(objNode)
			|| NOT(node->getVisible())
			|| objNode->getObjectType() != OBJ_GEOMETRY)
		{
			continue;
		}

		Attrs::PluginDesc nodePluginDesc(VRayExporter::getPluginName(objNode), "Node");
		VRay::Plugin nodePlugin = exportPlugin(nodePluginDesc);
		if (NOT(nodePlugin)) {
			continue;
		}

		nodePluginList.emplace_back(nodePlugin);
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("exclusion_nodes", nodePluginList));

	// transform
	pluginDesc.addAttribute(Attrs::PluginAttr("transform", VRayExporter::getObjTransform(&clipperNode, m_context, NOT(clipNodePlugin))));

	// material
	SHOP_Node *shopNode = getObjMaterial(&clipperNode, t);
	if (shopNode) {
		ExportContext objContext(CT_OBJ, *this, clipperNode);
		VRay::Plugin mtlPlugin = exportMaterial(*shopNode, objContext);
		pluginDesc.addAttribute(Attrs::PluginAttr("material", mtlPlugin));
	}

	setAttrsFromOpNodePrms(pluginDesc, &clipperNode);

	return exportPlugin(pluginDesc);
}

