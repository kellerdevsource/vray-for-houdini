//
// Copyright (c) 2015-2016, Chaos Software Ltd
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

#include <GA/GA_IntrinsicMacros.h>
#include <GU/GU_PrimPart.h>
#include <POP/POP_ContextData.h>

using namespace VRayForHoudini;


OP_Node* VRayExporter::getObjMaterial(OBJ_Node *objNode, fpreal t)
{
	if (!objNode)
		return nullptr;
	return objNode ? objNode->getMaterialNode(t) : nullptr;
}


void VRayExporter::RtCallbackOBJGeometry(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	Log::getLog().debug("RtCallbackOBJGeometry: %s from \"%s\"", OPeventToString(type), caller->getName().buffer());

	UT_ASSERT(caller->castToOBJNode());
	UT_ASSERT(caller->castToOBJNode()->castToOBJGeometry());

	VRayExporter &exporter = *reinterpret_cast< VRayExporter* >(callee);
	OBJ_Geometry *obj_geo = caller->castToOBJNode()->castToOBJGeometry();

	int shouldReExport = false;
	switch (type) {
		case OP_PARM_CHANGED: {
			if (Parm::isParmSwitcher(*caller, reinterpret_cast<intptr_t>(data))) {
				break;
			}

			// If the parameter is for material override it has OBJ_MATERIAL_SPARE_TAG tag
			const PRM_Parm *prm = Parm::getParm(*caller, reinterpret_cast<intptr_t>(data));
			if (prm) {
				UT_StringRef prmToken = prm->getToken();
				const PRM_SpareData	*spare = prm->getSparePtr();
				shouldReExport = (prmToken.equal(obj_geo->getMaterialParmToken()) ||
				                 (spare && spare->getValue(OBJ_MATERIAL_SPARE_TAG)));
			}
		}
		case OP_FLAG_CHANGED:
		case OP_INPUT_CHANGED:
		case OP_INPUT_REWIRED: {
			ObjectExporter objExporter(exporter, *obj_geo);
			objExporter.setExportGeometry(shouldReExport);
			objExporter.exportNode();
			break;
		}
		case OP_NODE_PREDELETE: {
			exporter.delOpCallbacks(caller);
			break;
		}
		default:
			break;
	}
}


void VRayExporter::RtCallbackSOPChanged(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	Log::getLog().debug("RtCallbackSOPChanged: %s from \"%s\"", OPeventToString(type), caller->getName().buffer());

	UT_ASSERT( caller->getCreator()->castToOBJNode() );
	UT_ASSERT( caller->getCreator()->castToOBJNode()->castToOBJGeometry() );

	VRayExporter &exporter = *reinterpret_cast< VRayExporter* >(callee);
	OBJ_Geometry *obj_geo = caller->getCreator()->castToOBJNode()->castToOBJGeometry();

	switch (type) {
		case OP_PARM_CHANGED: {
			if (Parm::isParmSwitcher(*caller, reinterpret_cast<intptr_t>(data))) {
				break;
			}
		}
		case OP_FLAG_CHANGED:
		case OP_INPUT_CHANGED:
		case OP_INPUT_REWIRED: {
			SOP_Node *geom_node = obj_geo->getRenderSopPtr();
			if (geom_node) {
				exporter.addOpCallback(geom_node, VRayExporter::RtCallbackSOPChanged);
			}

			ObjectExporter objExporter(exporter, *obj_geo);
			GeometryExporter geoExporter(objExporter, *obj_geo, exporter);
			geoExporter.exportGeometry();
			break;
		}
		case OP_NODE_PREDELETE: {
			exporter.delOpCallbacks(caller);
			break;
		}
		default:
			break;
	}
}


void VRayExporter::RtCallbackVRayClipper(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast< VRayExporter* >(callee);
	OBJ_Node *clipperNode = caller->castToOBJNode();

	Log::getLog().debug("RtCallbackVRayClipper: %s from \"%s\"", OPeventToString(type), clipperNode->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED: {
			if (Parm::isParmSwitcher(*caller, reinterpret_cast<intptr_t>(data))) {
				break;
			}
		}
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

template <typename ValueType>
static void dumpValues(const char *name, const UT_Options *options, bool readonly, GA_StorageClass storage, ValueType values, exint evalsize) {
	Log::getLog().debug("name = %s", name);
}

static void dumpType(OBJ_OBJECT_TYPE objType) {
	std::string objTypeStr;
	if (objType & OBJ_WORLD) { objTypeStr += "OBJ_WORLD | "; }
	if (objType & OBJ_GEOMETRY) {  objTypeStr += "OBJ_GEOMETRY | ";  }
	if (objType & OBJ_CAMERA) { objTypeStr += "OBJ_CAMERA | "; }
	if (objType & OBJ_LIGHT)    { objTypeStr += "OBJ_LIGHT | "; }
	if (objType & OBJ_RENDERER) {  objTypeStr += "OBJ_RENDERER | ";  }
	if (objType & OBJ_FOG) {  objTypeStr += "OBJ_FOG | ";  }
	if (objType & OBJ_BONE) {  objTypeStr += "OBJ_BONE | ";  }
	if (objType & OBJ_HANDLE) { objTypeStr += "OBJ_HANDLE | "; }
	if (objType & OBJ_BLEND) { objTypeStr += "OBJ_BLEND | "; }
	if (objType & OBJ_FORCE) { objTypeStr += "OBJ_FORCE | "; }
	if (objType & OBJ_CAMSWITCH) { objTypeStr += "OBJ_CAMSWITCH | "; }
	if (objType & OBJ_SOUND) { objTypeStr += "OBJ_SOUND | "; }
	if (objType & OBJ_MICROPHONE) { objTypeStr += "OBJ_MICROPHONE | "; }
	if (objType & OBJ_SUBNET) { objTypeStr += "OBJ_SUBNET | "; }
	if (objType & OBJ_FETCH) { objTypeStr += "OBJ_FETCH | "; }
	if (objType & OBJ_NULL) {  objTypeStr += "OBJ_NULL | ";  }
	if (objType & OBJ_STICKY) { objTypeStr += "OBJ_STICKY | "; }
	if (objType & OBJ_DOPNET) { objTypeStr += "OBJ_DOPNET | "; }
	if (objType & OBJ_RIVET) { objTypeStr += "OBJ_RIVET | "; }
	if (objType & OBJ_MUSCLE) { objTypeStr += "OBJ_MUSCLE | "; }
	if (objType & OBJ_STD_LIGHT) { objTypeStr += "OBJ_STD_LIGHT | "; }
	if (objType & OBJ_STD_BONE) {  objTypeStr += "OBJ_STD_BONE | ";  }
	if (objType & OBJ_STD_HANDLE) { objTypeStr += "OBJ_STD_HANDLE | "; }
	if (objType & OBJ_STD_BLEND) { objTypeStr += "OBJ_STD_BLEND | "; }
	if (objType & OBJ_STD_FETCH) { objTypeStr += "OBJ_STD_FETCH | "; }
	if (objType & OBJ_STD_STICKY) { objTypeStr += "OBJ_STD_STICKY | "; }
	if (objType & OBJ_STD_RIVET) { objTypeStr += "OBJ_STD_RIVET | "; }
	if (objType & OBJ_STD_NULL) {  objTypeStr += "OBJ_STD_NULL | ";  }
	if (objType & OBJ_STD_MUSCLE) { objTypeStr += "OBJ_STD_MUSCLE | "; }
	if (objType & OBJ_STD_CAMSWITCH) { objTypeStr += "OBJ_STD_CAMSWITCH | "; }
	if (objType & OBJ_ALL) {  objTypeStr += "OBJ_ALL"; }
	Log::getLog().debug("OBJ_OBJECT_TYPE = %s", objTypeStr.c_str());
}

VRay::Plugin VRayExporter::exportObject(OBJ_Node *objNode)
{
	if (!objNode) {
		return VRay::Plugin();
	}

	VRay::Plugin plugin;

	OP_Node *renderOp = objNode->getRenderNodePtr();
	if (!renderOp) {
		Log::getLog().error("OBJ \"%s\": Render OP is not found!",
							objNode->getName().buffer());
	}
	else {
		const UT_String &objOpType = objNode->getOperator()->getName();

		if (objOpType.equal("VRayNodeVRayClipper")) {
			plugin = exportVRayClipper(*objNode);
		}
#ifdef CGR_HAS_VRAYSCENE
		else if (objOpType.equal("VRayNodeVRayScene")) {
			plugin = exportVRayScene(objNode, CAST_SOPNODE(renderOp));
		}
#endif
		else {
			addOpCallback(objNode, RtCallbackOBJGeometry);
			addOpCallback(renderOp, RtCallbackSOPChanged);

			ObjectExporter objExporter(*this, *objNode);
			plugin = objExporter.exportNode();

			if (!plugin) {
				Log::getLog().info("Error exporting OBJ: %s [%s]",
								   objNode->getName().buffer(),
								   objNode->getOperator()->getName().buffer());
				Log::getLog().info("  Render OP: %s:\"%s\"",
								   renderOp->getName().buffer(),
								   renderOp->getOperator()->getName().buffer());
			}
		}
	}

	return plugin;
}


VRay::Plugin VRayExporter::exportVRayClipper(OBJ_Node &clipperNode)
{
	addOpCallback(&clipperNode, VRayExporter::RtCallbackVRayClipper);
	fpreal t = getContext().getTime();

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
	VRay::Plugin mtlPlugin = exportMaterial(clipperNode.getMaterialNode(t));
	if (mtlPlugin) {
		pluginDesc.addAttribute(Attrs::PluginAttr("material", mtlPlugin));
	}

	setAttrsFromOpNodePrms(pluginDesc, &clipperNode);

	return exportPlugin(pluginDesc);
}
