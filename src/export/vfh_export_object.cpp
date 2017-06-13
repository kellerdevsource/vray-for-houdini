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


SHOP_Node *VRayExporter::getObjMaterial(OBJ_Node *obj_node, fpreal t)
{
	SHOP_Node *shop_node = nullptr;
	OP_Node *op_node = obj_node->getMaterialNode(t);
	if (op_node) {
		shop_node = op_node->castToSHOPNode();
	}

	return shop_node;
}


void VRayExporter::RtCallbackOBJGeometry(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	Log::getLog().debug("RtCallbackOBJGeometry: %s from \"%s\"", OPeventToString(type), caller->getName().buffer());

	UT_ASSERT( caller->castToOBJNode() );
	UT_ASSERT( caller->castToOBJNode()->castToOBJGeometry() );

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
				shouldReExport =
						(   prmToken.equal(obj_geo->getMaterialParmToken())
						|| (spare && spare->getValue(OBJ_MATERIAL_SPARE_TAG))
						);
			}
		}
		case OP_FLAG_CHANGED:
		case OP_INPUT_CHANGED:
		case OP_INPUT_REWIRED:
		{

			GeometryExporter geoExporter(*obj_geo, exporter);
			geoExporter.setExportGeometry(shouldReExport);

			int nPlugins = geoExporter.exportNodes();
			for (int i = 0; i < nPlugins; ++i) {
				exporter.exportPlugin(geoExporter.getPluginDescAt(i));
			}

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
		case OP_INPUT_REWIRED:
		{
			SOP_Node *geom_node = obj_geo->getRenderSopPtr();
			if (geom_node) {
				exporter.addOpCallback(geom_node, VRayExporter::RtCallbackSOPChanged);
			}

			GeometryExporter geoExporter(*obj_geo, exporter);
			int nPlugins = geoExporter.exportNodes();
			for (int i = 0; i < nPlugins; ++i) {
				 exporter.exportPlugin(geoExporter.getPluginDescAt(i));
			}

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

VRay::Plugin VRayExporter::exportObject(OBJ_Node *obj_node)
{
	VRay::Plugin plugin;
	if (NOT(obj_node)) {
		return plugin;
	}

#if 0
	OBJ_OBJECT_TYPE objType = obj_node->getObjectType();
	dumpType(objType);

	OP_Node *renNode = obj_node->getRenderNodePtr();
	if (renNode) {
		DOP_Node *dopRenNode = CAST_DOPNODE(renNode);
		if (dopRenNode) {
			Log::getLog().debug("DOP_Node");
		}

		POPNET_Node *popNetRenNode = CAST_POPNETNODE(renNode);
		if (popNetRenNode) {
			Log::getLog().debug("POPNET_Node");
		}

		POP_Node *popRenNode = CAST_POPNODE(renNode);
		if (popRenNode) {
			POP_ContextData pdata("popRenNode");
			GEO_PrimParticle *part = pdata.getPrimPart(popRenNode);
			if (part) {
				Log::getLog().debug("part = %i", part->getNumParticles());
			}
		}

		SOP_Node *geomRenNode = CAST_SOPNODE(renNode);
		if (geomRenNode) {
			GU_DetailHandleAutoReadLock gdl(geomRenNode->getCookedGeoHandle(m_context));
			if (gdl.isValid()) {
				const GU_Detail &gdp = *gdl.getGdp();
				for (GA_AttributeDict::iterator it = gdp.getAttributeDict(GA_ATTRIB_PRIMITIVE).begin(GA_SCOPE_PUBLIC); !it.atEnd(); ++it) {
					GA_Attribute *attrib = it.attrib();
					Log::getLog().debug("attrib = %s", attrib->getName().buffer());
				}

				const GA_IntrinsicManager &iman = gdp.getIntrinsicManager();

				UT_StringArray inames;
				iman.extractNames(inames);
				for (int i = 0; i < inames.size(); ++i) {
					Log::getLog().debug("intrin = %s", inames[i].buffer());
				}

				if (gdp.containsPrimitiveType(GEO_PRIMPART)) {
					Log::getLog().debug("GEO_PRIMPART");
				}

				{
					auto & primList = gdp.getPrimitiveList();
					const int primCount = primList.offsetSize();
					for (int c = 0; c < primCount; ++c) {
						const GA_Primitive *prim = primList.get(c);
						if (prim && prim->getTypeId() == GEO_PRIMPART) {
							const GU_PrimParticle *part = UTverify_cast<const GU_PrimParticle*>(prim);
							if (part) {
								const GEO_PartRender &partRen = part->getRenderAttribs();
							}
						}
					}
				}

				{
					GA_LocalIntrinsic id = gdp.findIntrinsic("pointattributes");

					bool readonly = gdp.getIntrinsicReadOnly(id);
					exint tuplesize = gdp.getIntrinsicTupleSize(id);
					GA_StorageClass storage = gdp.getIntrinsicStorage(id);
					const char *name = gdp.getIntrinsicName(id);
					const UT_Options *options = gdp.getIntrinsicOptions(id);

					switch (storage) {
						case GA_STORECLASS_INT: {
							UT_StackBuffer<int64> values(tuplesize);
							exint   evalsize = gdp.getIntrinsic(id, values, tuplesize);
							UT_ASSERT(evalsize == tuplesize);
							dumpValues(name, options, readonly, storage, values, evalsize);
							break;
						}
						case GA_STORECLASS_FLOAT: {
							UT_StackBuffer<fpreal64> values(tuplesize);
							exint evalsize = gdp.getIntrinsic(id, values, tuplesize);
							UT_ASSERT(evalsize == tuplesize);
							dumpValues(name, options, readonly, storage, values, evalsize);
							break;
						}
						case GA_STORECLASS_STRING: {
							UT_StringArray values;
							exint evalsize = gdp.getIntrinsic(id, values);
							UT_ASSERT(evalsize == tuplesize);
							UT_ASSERT(evalsize == values.entries());
							dumpValues(name, options, readonly, storage, values, evalsize);
							break;
						}
						default:
							break;
					}
				}
			}
		}
	}
#endif

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
				int nPlugins = geoExporter.exportNodes();
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
		VRay::Plugin mtlPlugin = exportMaterial(*shopNode);
		pluginDesc.addAttribute(Attrs::PluginAttr("material", mtlPlugin));
	}

	setAttrsFromOpNodePrms(pluginDesc, &clipperNode);

	return exportPlugin(pluginDesc);
}
