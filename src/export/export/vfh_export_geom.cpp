//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_geom.h"
#include "vfh_export_mesh.h"
#include "vfh_export_hair.h"
#include "gu_vrayproxyref.h"
#include "gu_volumegridref.h"
#include "rop/vfh_rop.h"
#include "sop/sop_node_base.h"
#include "vop/vop_node_base.h"

#include <GEO/GEO_Primitive.h>
#include <GU/GU_Detail.h>
#include <OP/OP_Bundle.h>
#include <GA/GA_Types.h>

#include <QStringBuilder>

using namespace VRayForHoudini;

/// XXX: Move to ObjectExporter
typedef VUtils::HashMapKey<OP_Node*, VRay::Plugin> OpPluginCache;

static OpPluginCache opPluginCache;

void VRayForHoudini::clearOpPluginCache() {
	opPluginCache.clear();
}

static const GA_PrimitiveTypeId typeIdAlembicRef = GU_PrimPacked::lookupTypeId("AlembicRef");
static const GA_PrimitiveTypeId typeIdPackedDisk = GU_PrimPacked::lookupTypeId("PackedDisk");
static const GA_PrimitiveTypeId typeIdPackedGeometry = GU_PrimPacked::lookupTypeId("PackedGeometry");
static const GA_PrimitiveTypeId typeIdVRayProxyRef = GU_PrimPacked::lookupTypeId("VRayProxyRef");

static const char intrAlembicFilename[] = "abcfilename";
static const char intrAlembicObjectPath[] = "abcobjectpath";
static const char intrPackedPrimName[] = "packedprimname";
static const char intrPackedPrimitiveName[] = "packedprimitivename";
static const char intrPackedLocalTransform[] = "packedlocaltransform";
static const char intrGeometryID[] = "geometryid";
static const char intrFilename[] = "filename";

static const char VFH_ATTR_MATERIAL_ID[] = "switchmtl";

/// A bit-set for detecting unconnected points.
typedef std::vector<bool> DynamicBitset;

/// Check all points inside the detail and clear the bitset's indices of those
/// points which belong to some primitive. Additionally return the number of
/// unconnected points.
static GA_Size fillFreePointMap(const GU_Detail &detail, DynamicBitset &map)
{
	const GA_Size verticesCount = detail.getNumVertices();
	for (GA_Size c = 0; c < verticesCount; ++c) {
		const GA_Offset vertOffset = detail.vertexOffset(c);
		const GA_Offset pointOffset = detail.vertexPoint(vertOffset);
		const GA_Index pointIndex = detail.pointIndex(pointOffset);
		if (pointIndex < map.size()) {
			map[pointIndex] = false;
		}
	}
	GA_Size numPoints = 0;
	for (GA_Size c = 0; c < map.size(); ++c) {
		numPoints += map[c];
	}
	return numPoints;
}

bool GeometryExporter::hasSubdivApplied() const
{
	// here we check if subdivision has been assigned to this node
	// at render time. V-Ray subdivision is implemented in 2 ways:
	// 1. as a custom VOP available in V-Ray material context
	// 2. as spare parameters added to the object node

	bool res = false;

	fpreal t = ctx.getTime();
	bool hasDispl = objNode.hasParm("vray_use_displ") && objNode.evalInt("vray_use_displ", 0, t);
	if (NOT(hasDispl)) {
		return res;
	}

	const int displType = objNode.evalInt("vray_displ_type", 0, t);
	switch (displType) {
		// from shopnet
	case 0:
	{
		UT_String shopPath;
		objNode.evalString(shopPath, "vray_displshoppath", 0, t);
		SHOP_Node *shop = OPgetDirector()->findSHOPNode(shopPath.buffer());
		if (shop) {
			UT_ValArray<OP_Node *> outputNodes;
			if (shop->getOpsByName("vray_material_output", outputNodes)) {
				// there is at least 1 "vray_material_output" node so take the first one
				OP_Node *node = outputNodes(0);
				if (node->error() < UT_ERROR_ABORT) {
					const int idx = node->getInputFromName("Geometry");
					VOP::NodeBase *input = dynamic_cast< VOP::NodeBase * >(node->getInput(idx));
					if (input && input->getVRayPluginID() == "GeomStaticSmoothedMesh") {
						res = true;
					}
				}
			}
		}
		break;
	}
	// type is "GeomStaticSmoothedMesh"
	case 2:
	{
		res = true;
	}
	default:
		break;
	}

	return res;
}

int GeometryExporter::isNodeVisible(VRayRendererNode &rop, OBJ_Node &node)
{
	OP_Bundle *bundle = rop.getForcedGeometryBundle();
	if (!bundle) {
		return node.getVisible();
	}
	return bundle->contains(&node, false) || node.getVisible();
}

int GeometryExporter::isNodeVisible() const
{
	return isNodeVisible(pluginExporter.getRop(), objNode);
}

int GeometryExporter::isNodeMatte() const
{
	VRayRendererNode &rop = pluginExporter.getRop();
	OP_Bundle *bundle = rop.getMatteGeometryBundle();
	if (!bundle) {
		return false;
	}
	return bundle->contains(&objNode, false);
}

int GeometryExporter::isNodePhantom() const
{
	VRayRendererNode &rop = pluginExporter.getRop();
	OP_Bundle *bundle = rop.getPhantomGeometryBundle();
	if (!bundle) {
		return false;
	}
	return bundle->contains(&objNode, false);
}

#if 0
int GeometryExporter::_exportNodes() {
	SOP_Node *renderSOP = m_objNode.getRenderSopPtr();
	if (NOT(renderSOP)) {
		// we don't have a valid render SOP
		// nothing else to do
		return 0;
	}

	GU_DetailHandleAutoReadLock gdl(renderSOP->getCookedGeoHandle(m_context));
	if (NOT(gdl)) {
		// we don't have a valid render geometry gdp
		// nothing else to do
		return 0;
	}

	// geometry is exported in 2 steps:
	// 1. we traverse the render gdp, export the geometry that
	//    we can handle and wrap it in a Node  plugin description
	//    which is accumulated into m_detailToPluginDesc map
	// 2. we get the list of Nodes generated for the render gdp
	//    and adjust properties like transform, material,
	//    user_attributes, etc.

	m_myDetailID = gdl.handle().hash();
	const GU_Detail &gdp = *gdl.getGdp();

	if (renderSOP->getOperator()->getName().startsWith("VRayNode")
		&& !renderSOP->getOperator()->getName().startsWith("VRayNodePhxShaderCache")
		&& !renderSOP->getOperator()->getName().startsWith("VRayNodeVRayProxy")) {
		// V-Ray plane SOP and V-Ray scene SOP are still implemented such as
		// they expect to be final SOP in the SOP network
		// TODO: need to fix this in future
		exportVRaySOP(*renderSOP, m_detailToPluginDesc[m_myDetailID]);
	} else {
		// handle geometry export from the render gdp
		exportDetail(*renderSOP, gdl, m_detailToPluginDesc[m_myDetailID]);
	}

	// get the OBJ transform
	VRay::Transform tm = VRayExporter::getObjTransform(&m_objNode, m_context);

	// format material overrides specified on the object as user_attributes
	UT_String userAttrs;
	getSHOPOverridesAsUserAttributes(userAttrs);

	// handle export of material for the object node
	VRay::Plugin mtl;
	if (m_exportGeometry) {
		mtl = exportMaterial();
	}

	// adjust Node parameters
	int i = 0;
	PluginDescList &pluginList = m_detailToPluginDesc.at(m_myDetailID);
	for (Attrs::PluginDesc &nodeDesc : pluginList) {
		// TODO: need to figure out how to generate names for Node plugins
		//       comming from different primitives
		if (nodeDesc.pluginName != "") {
			continue;
		}
		nodeDesc.pluginName = VRayExporter::getPluginName(&m_objNode, boost::str(Parm::FmtPrefixManual % "Node" % std::to_string(i++)));

		Attrs::PluginAttr *attr = nullptr;

		attr = nodeDesc.get("transform");
		if (NOT(attr)) {
			nodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));
		} else {
			attr->paramValue.valTransform = tm * attr->paramValue.valTransform;
		}

		nodeDesc.addAttribute(Attrs::PluginAttr("visible", isNodeVisible()));

		attr = nodeDesc.get("geometry");
		if (attr) {
			VRay::Plugin geomDispl = m_pluginExporter.exportDisplacement(&m_objNode, attr->paramValue.valPlugin);
			if (geomDispl) {
				attr->paramValue.valPlugin = geomDispl;
			}
		}

		attr = nodeDesc.get("material");
		if (NOT(attr)) {
			// add the material only if it hasn't been specified yet
			if (mtl) {
				nodeDesc.addAttribute(Attrs::PluginAttr("material", mtl));
			}

			attr = nodeDesc.get(VFH_ATTR_MATERIAL_ID);
			if (NOT(attr)) {
				// pass material overrides with "user_attributes"
				if (userAttrs.isstring()) {
					nodeDesc.addAttribute(Attrs::PluginAttr("user_attributes", userAttrs.toStdString()));
				}
			}
		}

		// TODO: adjust other Node attrs
	}

	return pluginList.size();
}
#endif


VRay::Plugin GeometryExporter::exportMaterial()
{
#if 1
	return pluginExporter.exportDefaultMaterial();
#else
	VRay::ValueList mtls_list;
	VRay::IntList   ids_list;
	mtls_list.reserve(m_shopList.size() + 1);
	ids_list.reserve(m_shopList.size() + 1);

	// object material is always exported with id 0
	OP_Node *matNode = pluginExporter.getObjMaterial(&objNode, ctx.getTime());
	if (matNode) {
		mtls_list.emplace_back(pluginExporter.exportMaterial(matNode));
		ids_list.emplace_back(0);
	}
	else {
		mtls_list.emplace_back(pluginExporter.exportDefaultMaterial());
		ids_list.emplace_back(0);
	}

	// generate id for each SHOP and add it to the material list
	SHOPHasher hasher;
	for (const UT_String &matPath : m_shopList) {
		OP_Node *opNode = getOpNodeFromPath(matPath);
		UT_ASSERT(opNode);
		mtls_list.emplace_back(pluginExporter.exportMaterial(opNode));
		ids_list.emplace_back(hasher(opNode));
	}

	// export single MtlMulti material
	Attrs::PluginDesc mtlDesc;
	mtlDesc.pluginID = "MtlMulti";
	mtlDesc.pluginName = VRayExporter::getPluginName(&objNode, "Mtl");

	mtlDesc.addAttribute(Attrs::PluginAttr("mtls_list", mtls_list));
	mtlDesc.addAttribute(Attrs::PluginAttr("ids_list", ids_list));

	Attrs::PluginDesc myMtlIDDesc;
	myMtlIDDesc.pluginID = "TexUserScalar";
	myMtlIDDesc.pluginName = VRayExporter::getPluginName(&objNode, "MtlID");

	myMtlIDDesc.addAttribute(Attrs::PluginAttr("default_value", 0));
	myMtlIDDesc.addAttribute(Attrs::PluginAttr("user_attribute", VFH_ATTR_MATERIAL_ID));

	VRay::Plugin myMtlID = pluginExporter.exportPlugin(myMtlIDDesc);

	mtlDesc.addAttribute(Attrs::PluginAttr("mtlid_gen_float", myMtlID, "scalar"));

	VRay::Plugin mtl = pluginExporter.exportPlugin(mtlDesc);

	// handle if object is forced as matte
	if (isNodeMatte()) {
		Attrs::PluginDesc mtlWrapperDesc;
		mtlWrapperDesc.pluginID = "MtlWrapper";
		mtlWrapperDesc.pluginName = VRayExporter::getPluginName(&objNode, "MtlWrapper");

		mtlWrapperDesc.addAttribute(Attrs::PluginAttr("base_material", mtl));
		mtlWrapperDesc.addAttribute(Attrs::PluginAttr("matte_surface", 1));
		mtlWrapperDesc.addAttribute(Attrs::PluginAttr("alpha_contribution", -1));
		mtlWrapperDesc.addAttribute(Attrs::PluginAttr("affect_alpha", 1));
		mtlWrapperDesc.addAttribute(Attrs::PluginAttr("reflection_amount", 0));
		mtlWrapperDesc.addAttribute(Attrs::PluginAttr("refraction_amount", 0));

		mtl = pluginExporter.exportPlugin(mtlWrapperDesc);
	}

	// handle if object is forced as phantom
	if (isNodePhantom()) {
		Attrs::PluginDesc mtlStatsDesc;
		mtlStatsDesc.pluginID = "MtlRenderStats";
		mtlStatsDesc.pluginName = VRayExporter::getPluginName(&objNode, "MtlRenderStats");

		mtlStatsDesc.addAttribute(Attrs::PluginAttr("base_mtl", mtl));
		mtlStatsDesc.addAttribute(Attrs::PluginAttr("camera_visibility", 0));

		mtl = pluginExporter.exportPlugin(mtlStatsDesc);
	}

	return mtl;
#endif
}

int GeometryExporter::getSHOPOverridesAsUserAttributes(UT_String &userAttrs) const {
	int nOverrides = 0;

	OP_Node *shopNode = pluginExporter.getObjMaterial(&objNode, ctx.getTime());
	if (!shopNode) {
		return nOverrides;
	}

	// specify the id of the material to use
	userAttrs += VFH_ATTR_MATERIAL_ID;
	userAttrs += "=0;";

	// handle shop overrides specified on the object node
	const PRM_ParmList *shopParmList = shopNode->getParmList();
	const PRM_ParmList *objParmList = objNode.getParmList();

	for (int i = 0; i < shopParmList->getEntries(); ++i) {
		const PRM_Parm *shopPrm = shopParmList->getParmPtr(i);
		const PRM_Parm *objPrm = objParmList->getParmPtr(shopPrm->getToken());

		if (objPrm
			&& shopPrm->getType() == objPrm->getType()
			&& objPrm->getType().isFloatType()
			&& NOT(objPrm->getBypassFlag())) {
			// we have parameter with matching name on the OBJ_Node
			// => treat as override
			UT_StringArray prmValTokens;
			for (int i = 0; i < objPrm->getVectorSize(); ++i) {
				fpreal chval = objNode.evalFloat(objPrm, i, ctx.getTime());
				prmValTokens.append(std::to_string(chval));
			}

			UT_String prmValToken;
			prmValTokens.join(",", prmValToken);

			userAttrs += shopPrm->getToken();
			userAttrs += "=";
			userAttrs += prmValToken;
			userAttrs += ";";

			++nOverrides;
		}
	}

	return nOverrides;
}


VRay::Plugin GeometryExporter::exportVRaySOP(SOP_Node &sop)
{
	if (!m_exportGeometry) {
		return VRay::Plugin();
	}

	SOP::NodeBase *vrayNode = UTverify_cast<SOP::NodeBase*>(&sop);

	ExportContext ctx(CT_OBJ, pluginExporter, *vrayNode->getParent());

	Attrs::PluginDesc pluginDesc;
	switch (vrayNode->asPluginDesc(pluginDesc, pluginExporter, &ctx)) {
		case OP::VRayNode::PluginResultNA:
		case OP::VRayNode::PluginResultContinue: {
			pluginExporter.setAttrsFromOpNodePrms(pluginDesc, vrayNode);
			break;
		}
		case OP::VRayNode::PluginResultError:
		default: {
			Log::getLog().error("Error creating plugin descripion for \"%s\" [%s]",
								sop.getName().buffer(),
								sop.getOperator()->getName().buffer());
		}
	}

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin GeometryExporter::exportPackedPrimitives(SOP_Node &sop, const GU_Detail &gdp)
{
	if (GU_PrimPacked::hasPackedPrimitives(gdp)) {
		UT_Array<const GA_Primitive *> prims;
		GU_PrimPacked::getPackedPrimitives(gdp, prims);
		for (const GA_Primitive *prim : prims) {
			auto *primPacked = UTverify_cast< const GU_PrimPacked * >(prim);
			exportPacked(sop, *primPacked);
		}
	}
	return VRay::Plugin();
}

VRay::Plugin GeometryExporter::exportDetail(SOP_Node &sop, const GU_Detail &gdp)
{
	PluginsTable plugins;

	const VMRenderPoints renderPoints = getParticlesMode();
	if (renderPoints != vmRenderPointsNone) {
		VRay::Plugin fromPart = exportPointParticles(gdp, renderPoints);
		if (fromPart) {
			plugins += fromPart;
		}
	}

	if (renderPoints != vmRenderPointsAll) {
		PluginDescList __descList;
#ifdef CGR_HAS_AUR
		VolumeExporter volExp(objNode, ctx, pluginExporter);
		HoudiniVolumeExporter hVoldExp(objNode, ctx, pluginExporter);
		volExp.exportPrimitives(gdp, __descList);
		hVoldExp.exportPrimitives(gdp, __descList);
#endif

		HairPrimitiveExporter hairExp(objNode, ctx, pluginExporter);
		hairExp.exportPrimitives(gdp, __descList);

		VRay::Plugin fromPacked = exportPackedPrimitives(sop, gdp);
		if (fromPacked) {
			plugins += fromPacked;
		}

		VRay::Plugin fromPoly = exportPolyMesh(sop, gdp);
		if (fromPoly) {
			plugins += fromPoly;
		}
	}

	if (!plugins.count()) {
		return VRay::Plugin();
	}
	if (plugins.count() == 1) {
		return plugins[0];
	}

	// Combine point particles and geometry under Instancer2.
	int instancesIdx = 0;
	// +1 because first value is time.
	VRay::VUtils::ValueRefList instances(plugins.count()+1);
	instances[instancesIdx++].setDouble(0.0);

	for (int i = 0; i < plugins.count(); ++i) {
		static VRay::Transform tm;
		tm.makeIdentity();

		// Instancer works with Node plugins.
		Attrs::PluginDesc nodeDesc("", "Node");
		nodeDesc.addAttribute(Attrs::PluginAttr("geometry", plugins[i]));
		nodeDesc.addAttribute(Attrs::PluginAttr("material", pluginExporter.exportDefaultMaterial()));
		nodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));
		nodeDesc.addAttribute(Attrs::PluginAttr("visible", false));

		VRay::Plugin node = pluginExporter.exportPlugin(nodeDesc);
		UT_ASSERT(node);

		// Index + TM + VEL_TM + Node
		VRay::VUtils::ValueRefList item(4);
		item[0].setDouble(i);
		item[1].setTransform(tm);
		item[2].setTransform(tm);
		item[3].setPlugin(node);

		instances[instancesIdx++].setList(item);
	}

	Attrs::PluginDesc instancer2(VRayExporter::getPluginName(objNode),
								 "Instancer2");
	instancer2.addAttribute(Attrs::PluginAttr("instances", instances));
	instancer2.addAttribute(Attrs::PluginAttr("use_time_instancing", false));

	return pluginExporter.exportPlugin(instancer2);
}

VRay::Plugin GeometryExporter::exportPolyMesh(SOP_Node &sop, const GU_Detail &gdp)
{
	MeshExporter polyMeshExporter(objNode, ctx, pluginExporter);
	polyMeshExporter.init(gdp);
	polyMeshExporter.setSubdivApplied(hasSubdivApplied());
	if (polyMeshExporter.hasPolyGeometry()) {
		if (m_exportGeometry) {
			Attrs::PluginDesc geomDesc;
			if (polyMeshExporter.asPluginDesc(gdp, geomDesc)) {
				return pluginExporter.exportPlugin(geomDesc);
			}

			PluginDescList __descList;
			polyMeshExporter.exportPrimitives(gdp, __descList);
		}
#if 0
		else {
			// we don't want to reexport the geometry so just
			// add new node to our list of nodes
			pluginList.push_back(Attrs::PluginDesc("", "Node"));
			Attrs::PluginDesc &nodeDesc = pluginList.back();

			SHOPList shopList;
			int nSHOPs = polyMeshExporter.getSHOPList(shopList);
			if (nSHOPs > 0) {
				nodeDesc.addAttribute(Attrs::PluginAttr(VFH_ATTR_MATERIAL_ID, -1));
			}
		}
#endif
	}

	return VRay::Plugin();
}


int GeometryExporter::exportPacked(SOP_Node &sop, const GU_PrimPacked &prim)
{
#if 1
	return 0;
#else
	// get primitive unique id and check if we've already
	// processed that geometry
	uint packedID = getPrimPackedID(prim);
	if (NOT(m_detailToPluginDesc.count(packedID))) {
		// we haven't, so export geometry from that primitive
		exportPrimPacked(sop, prim, m_detailToPluginDesc[packedID]);
	}

	// get the list of Node descriptions for the primitive and
	// adjust it's properties as transform, material, user_attributes, etc.
	PluginDescList primPluginList = m_detailToPluginDesc.at(packedID);

	UT_Matrix4D fullxform;
	prim.getFullTransform4(fullxform);
	VRay::Transform tm = VRayExporter::Matrix4ToTransform(fullxform);

	GA_ROHandleS mtlpath(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	GA_ROHandleS mtlo(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, "material_override"));

	SHOPHasher hasher;
	for (Attrs::PluginDesc &pluginDesc : primPluginList) {
		pluginList.push_back(pluginDesc);
		Attrs::PluginDesc &nodeDesc = pluginList.back();

		Attrs::PluginAttr *attr = nullptr;

		attr = nodeDesc.get("transform");
		if (NOT(attr)) {
			nodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));
		} else {
			attr->paramValue.valTransform = tm * attr->paramValue.valTransform;
		}

		// we assign material and user_attributes only if material
		// has not been previously assigned. Existing VFH_ATTR_MATERIAL_ID
		// property signals that the node already has material and overrides set
		attr = nodeDesc.get(VFH_ATTR_MATERIAL_ID);
		if (NOT(attr)
			&& mtlpath.isValid()) {
			const char *shoppath = mtlpath.get(prim.getMapOffset());
			SHOP_Node *shopNode = OPgetDirector()->findSHOPNode(shoppath);
			if (shopNode) {
				// add material for export
				m_shopList.insert(shoppath);

				// pass material id with "user_attributes"
				int shopID = hasher(shopNode);
				nodeDesc.addAttribute(Attrs::PluginAttr(VFH_ATTR_MATERIAL_ID, shopID));

				UT_String userAtrs;

				userAtrs += VFH_ATTR_MATERIAL_ID;
				userAtrs += "=";
				userAtrs += std::to_string(shopID);
				userAtrs += ";";

				// pass material overrides with "user_attributes"
				UT_Options mtlOverridesDict;
				if (mtlo.isValid()
					&& mtlOverridesDict.setFromPyDictionary(mtlo.get(prim.getMapOffset()))) {
					while (mtlOverridesDict.getNumOptions() > 0) {
						UT_String key(mtlOverridesDict.begin().name().c_str());

						int chIdx = -1;
						PRM_Parm *prm = shopNode->getParmList()->getParmPtrFromChannel(key, &chIdx);
						if (NOT(prm)
							|| NOT(prm->getType().isFloatType())) {
							mtlOverridesDict.removeOption(key);
							continue;
						}

						UT_StringArray prmValTokens;
						for (int i = 0; i < prm->getVectorSize(); ++i) {
							prm->getChannelToken(key, i);
							fpreal chval = (mtlOverridesDict.hasOption(key)) ? mtlOverridesDict.getOptionF(key) : shopNode->evalFloat(prm, i, ctx.getTime());
							prmValTokens.append(std::to_string(chval));
							mtlOverridesDict.removeOption(key);
						}

						UT_String prmValToken;
						prmValTokens.join(",", prmValToken);

						userAtrs += prm->getToken();
						userAtrs += "=";
						userAtrs += prmValToken;
						userAtrs += ";";
					}
				}

				if (userAtrs.isstring()) {
					nodeDesc.addAttribute(Attrs::PluginAttr("user_attributes", userAtrs.toStdString()));
				}
			}
		}
	}

	return primPluginList.size();
#endif
}

int GeometryExporter::getPrimPackedID(const GU_PrimPacked &prim)
{
	const GA_PrimitiveTypeId primTypeID = prim.getTypeId();
	if (primTypeID == typeIdAlembicRef ||
		primTypeID == typeIdPackedDisk)
	{
		UT_String primname;
		prim.getIntrinsic(prim.findIntrinsic(intrPackedPrimitiveName), primname);
		return primname.hash();
	}
	if (primTypeID == typeIdPackedGeometry) {
		int geoID = -1;
		prim.getIntrinsic(prim.findIntrinsic(intrGeometryID), geoID);
		return geoID;
	}
	if (primTypeID ==  typeIdVRayProxyRef) {
		const VRayProxyRef *vrayproxyref = UTverify_cast<const VRayProxyRef*>(prim.implementation());
		return vrayproxyref->getOptions().hash();
	}

	UT_ASSERT(false);

	return 0;
}

VRay::Plugin GeometryExporter::exportPrimPacked(SOP_Node &sop, const GU_PrimPacked &prim)
{
	const GA_PrimitiveTypeId primTypeID = prim.getTypeId();
	if (primTypeID == typeIdAlembicRef) {
		return exportAlembicRef(sop, prim);
	}
	if (primTypeID == typeIdPackedDisk) {
		return exportPackedDisk(sop, prim);
	}
	if (primTypeID == typeIdPackedGeometry) {
		return exportPackedGeometry(sop, prim);
	}
	if (primTypeID == typeIdVRayProxyRef) {
		return exportVRayProxyRef(sop, prim);
	}

	UT_ASSERT(false);

	return VRay::Plugin();
}

VRay::Plugin GeometryExporter::exportAlembicRef(SOP_Node &sop, const GU_PrimPacked &prim)
{
	if (!m_exportGeometry) {
		return VRay::Plugin();
	}

#if 0
	UT_Matrix4 xform;
	prim.getIntrinsic(prim.findIntrinsic(intrPackedLocalTransform), xform);
	xform.invert();
	VRay::Transform tm = VRayExporter::Matrix4ToTransform(UT_Matrix4D(xform));
#endif

	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic(intrPackedPrimitiveName), primname);

	UT_String filename;
	prim.getIntrinsic(prim.findIntrinsic(intrAlembicFilename), filename);

	UT_String objname;
	prim.getIntrinsic(prim.findIntrinsic(intrAlembicObjectPath), objname);

	VRay::VUtils::CharStringRefList visibilityList(1);
	visibilityList[0] = objname;

	Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(&sop, primname.buffer()),
								 "GeomMeshFile");
	pluginDesc.addAttribute(Attrs::PluginAttr("use_full_names", true));
	pluginDesc.addAttribute(Attrs::PluginAttr("visibility_lists_type", 1));
	pluginDesc.addAttribute(Attrs::PluginAttr("visibility_list_names", visibilityList));
	pluginDesc.addAttribute(Attrs::PluginAttr("file", filename.toStdString()));
	pluginDesc.addAttribute(Attrs::PluginAttr("use_alembic_offset", true));

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin GeometryExporter::exportVRayProxyRef(SOP_Node &sop, const GU_PrimPacked &prim)
{
	if (!m_exportGeometry) {
		return VRay::Plugin();
	}

#if 0
	UT_Matrix4 xform;
	prim.getIntrinsic(prim.findIntrinsic(intrPackedLocalTransform), xform);
	xform.invert();
	VRay::Transform tm = VRayExporter::Matrix4ToTransform(UT_Matrix4D(xform));
#endif

	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic(intrPackedPrimitiveName), primname);

	Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(&sop, primname.buffer()),
								 "GeomMeshFile");

	const VRayProxyRef *vrayproxyref = UTverify_cast<const VRayProxyRef*>(prim.implementation());
	UT_Options options = vrayproxyref->getOptions();
	if (options.hasOption("anim_offset")) {
		// XXX: We coudl utilize "use_time_instancing" here
		options.setOptionF("anim_offset", options.getOptionF("anim_offset") - ctx.getFrame());
	}
	pluginExporter.setAttrsFromUTOptions(pluginDesc, options);

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin GeometryExporter::exportPackedDisk(SOP_Node &sop, const GU_PrimPacked &prim)
{
	// TODO: Implement PackedDisk geometry
#if 1
	return VRay::Plugin();
#else
	if (!m_exportGeometry) {
		return VRay::Plugin();
	}

	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic(intrPackedPrimName), primname);

	UT_String filename;
	prim.getIntrinsic(prim.findIntrinsic(intrFilename), filename);

	Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(&sop, primname.buffer()),
								 "GeomMeshFile");
	pluginDesc.addAttribute(Attrs::PluginAttr("file", filename.toStdString()));

	return pluginExporter.exportPlugin(pluginDesc);
#endif
}

VRay::Plugin GeometryExporter::exportPackedGeometry(SOP_Node &sop, const GU_PrimPacked &prim)
{
	const GA_ROHandleS pathHndl(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, "path"));
	if (!pathHndl.isValid()) {
		GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
		if (gdl) {
			return exportDetail(sop, *gdl);
		}
	}

	const UT_String &path = pathHndl.get(prim.getMapOffset());
	OP_Node *opNode = getOpNodeFromPath(path, ctx.getTime());
	if (!opNode) {
		GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
		if (gdl) {
			return exportDetail(sop, *gdl);
		}
	}

	OpPluginCache::iterator pIt = opPluginCache.find(opNode);
	if (pIt != opPluginCache.end()) {
		return pIt.data();
	}

	VRay::Plugin geometry;
			
	OBJ_Node *objNode = CAST_OBJNODE(opNode);
	SOP_Node *sopNode = CAST_SOPNODE(opNode);
	if (sopNode) {
		GU_DetailHandleAutoReadLock gdl(sopNode->getCookedGeoHandle(ctx));
		if (gdl) {
			geometry = exportDetail(*sopNode, *gdl);
		}
	}
	else if (objNode) {
		ObjectExporter objExporter(pluginExporter, *objNode);
		GeometryExporter geoExporter(objExporter, *objNode, pluginExporter);				
		geometry = geoExporter.exportGeometry();
	}
	else {
		UT_ASSERT(false);
	}

	opPluginCache.insert(opNode, geometry);

	return geometry;
}

ObjectExporter::ObjectExporter(VRayExporter &pluginExporter, OBJ_Node &objNode)
	: pluginExporter(pluginExporter)
	, objNode(objNode)
	, m_exportGeometry(true)
{}

GeometryExporter::GeometryExporter(ObjectExporter &objExporter, OBJ_Node &objNode, VRayExporter &pluginExporter)
	: objExporter(objExporter)
	, objNode(objNode)
	, ctx(pluginExporter.getContext())
	, pluginExporter(pluginExporter)
	, m_exportGeometry(true)
{}

VMRenderPoints GeometryExporter::getParticlesMode() const
{
	return static_cast<VMRenderPoints>(objNode.evalInt("vm_renderpoints", 0, ctx.getTime()));
}

static GA_ROHandleS getAttribInstancePath(const GU_Detail &gdp)
{
	return GA_ROHandleS(gdp.findAttribute(GA_ATTRIB_POINT, "instancepath"));
}

int GeometryExporter::isPointInstancer(const GU_Detail &gdp) const
{
	const GA_ROHandleS instancePathHndl = getAttribInstancePath(gdp);
	return instancePathHndl.isValid();
}

VRay::Plugin GeometryExporter::exportPointParticles(const GU_Detail &gdp, VMRenderPoints pointsMode)
{
	UT_ASSERT(pointsMode != vmRenderPointsNone);

	GA_Size numPoints = gdp.getNumPoints();
	if (!numPoints) {
		return VRay::Plugin();
	}

	if (pointsMode == vmRenderPointsUnconnected && !gdp.getNumPrimitives()) {
		pointsMode = vmRenderPointsAll;
	}

	DynamicBitset freePointMap;
	if (pointsMode == vmRenderPointsUnconnected) {
		freePointMap.resize(numPoints, true);
		numPoints = fillFreePointMap(gdp, freePointMap);
	}

	if (!numPoints) {
		return VRay::Plugin();
	}

	// Parameters.
	const fpreal renderScale = objNode.evalFloat("vm_pointscale", 0, ctx.getTime());

	int renderType;
	fpreal radiusMult = renderScale;

	const VMRenderPointsAs renderPointsAs =
		static_cast<VMRenderPointsAs>(objNode.evalInt("vm_renderpointsas", 0, ctx.getTime()));
	switch (renderPointsAs) {
		case vmRenderPointsAsSphere: {
			renderType = 7;
			radiusMult *= 0.01;
			break;
		}
		case vmRenderPointsAsCirle: {
			renderType = 6;
			radiusMult *= 5.0;
			break;
		}
	}

	// Particle properties.
	VRay::VUtils::VectorRefList positions(numPoints);

	VRay::VUtils::VectorRefList velocities;
	GA_ROHandleV3 velocityHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_VELOCITY));
	if (velocityHndl.isValid()) {
		velocities = VRay::VUtils::VectorRefList(numPoints);
	}

	VRay::VUtils::ColorRefList color;
	GA_ROHandleV3 cdHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_DIFFUSE));
	if (cdHndl.isValid()) {
		color = VRay::VUtils::ColorRefList(numPoints);
	}

	VRay::VUtils::FloatRefList opacity;
	GA_ROHandleF opacityHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_ALPHA));
	if (opacityHndl.isValid()) {
		opacity = VRay::VUtils::FloatRefList(numPoints);
	}

	VRay::VUtils::FloatRefList pscale;
	GA_ROHandleF pscaleHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_PSCALE));
	if (pscaleHndl.isValid()) {
		pscale = VRay::VUtils::FloatRefList(numPoints);
	}

	int positionsIdx = 0;
	for (GA_Index i = 0; i < numPoints; ++i) {
		const GA_Offset ptOff = gdp.pointOffset(i);

		const int isValidPoint = pointsMode == vmRenderPointsAll ? true : freePointMap[i];
		if (isValidPoint) {
			UT_ASSERT_MSG(positionsIdx < positions.size(), "Incorrect calculation of points count inside detail!");

			const UT_Vector3 &point = gdp.getPos3(ptOff);

			positions[positionsIdx].set(point.x(), point.y(), point.z());

			if (velocityHndl.isValid()) {
				const UT_Vector3F &v = velocityHndl.get(ptOff);
				velocities[positionsIdx].set(v.x(), v.y(), v.z());
			}

			if (pscaleHndl.isValid()) {
				pscale[positionsIdx] = radiusMult * pscaleHndl.get(ptOff);
			}

			if (cdHndl.isValid()) {
				const UT_Vector3F &cd = cdHndl.get(ptOff);
				color[positionsIdx].r = cd.x();
				color[positionsIdx].g = cd.y();
				color[positionsIdx].g = cd.z();
			}

			if (opacityHndl.isValid()) {
				opacity[positionsIdx] = opacityHndl.get(ptOff);
			}

			++positionsIdx;
		}
	}
	
	Attrs::PluginDesc partDesc(VRayExporter::getPluginName(&objNode, "GeomParticleSystem"),
							   "GeomParticleSystem");
	partDesc.addAttribute(Attrs::PluginAttr("positions", positions));
	partDesc.addAttribute(Attrs::PluginAttr("render_type", renderType));
	if (velocities.size()) {
		partDesc.addAttribute(Attrs::PluginAttr("velocities", velocities));
	}
	if (color.size()) {
		partDesc.addAttribute(Attrs::PluginAttr("colors", color));
	}
	if (opacity.size()) {
		partDesc.addAttribute(Attrs::PluginAttr("opacity_pp", opacity));
	}
	if (pscale.size()) {
		partDesc.addAttribute(Attrs::PluginAttr("radii", pscale));
		partDesc.addAttribute(Attrs::PluginAttr("point_radii", true));
	}
	else {
		partDesc.addAttribute(Attrs::PluginAttr("radius", radiusMult));
		partDesc.addAttribute(Attrs::PluginAttr("point_size", radiusMult));
	}

	return pluginExporter.exportPlugin(partDesc);
}

VRay::Plugin GeometryExporter::exportInstancer(const GU_Detail &gdp)
{
	VRay::Plugin instancer;
	
	return instancer;
}

VRay::Plugin GeometryExporter::exportPointInstancer(const GU_Detail &gdp)
{
	const GA_Size numPoints = gdp.getNumPoints();

	GA_ROHandleV3 velocityHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_VELOCITY));

	GA_ROHandleS instanceHndl(gdp.findAttribute(GA_ATTRIB_POINT, "instance"));
	GA_ROHandleS instancePathHndl(gdp.findAttribute(GA_ATTRIB_POINT, "instancepath"));
	GA_ROHandleS materialPathHndl(gdp.findAttribute(GA_ATTRIB_POINT, "shop_materialpath"));

	GA_ROHandleV3 cdHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_DIFFUSE));
	GA_ROHandleF opacityHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_ALPHA));
	GA_ROHandleF pscaleHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_PSCALE));
	GA_ROHandleV3 scaleHndl(gdp.findAttribute(GA_ATTRIB_POINT, "scale"));

	// TODO: Move to AppSDK headers.
	enum HierarchicalParameterizedNodeParameterFlags {
		useParentTimes = (1 << 0),
		useObjectID = (1 << 1),
		usePrimaryVisibility = (1 << 2),
		useUserAttributes = (1 << 3),
		useParentTimesAtleastForGeometry = (1 << 4),
		useMaterial = (1 << 5),
		useGeometry = (1 << 6),
	};

	Attrs::PluginDesc instancer2(VRayExporter::getPluginName(objNode, "Instancer2"), "Instancer2");

	// +1 because first element is time.
	VRay::VUtils::ValueRefList instances(numPoints+1);
	instances[0].setDouble(0.0);

	int validPointIdx = 0;
	for (GA_Index i = 0; i < numPoints; ++i) {
		const GA_Offset ptOff = gdp.pointOffset(i);

		UT_String instanceObjectPath;
		if (instanceHndl.isValid()) {
			instanceObjectPath = instanceHndl.get(ptOff);
		}
		else if (instancePathHndl.isValid()) {
			instanceObjectPath = instancePathHndl.get(ptOff);
		}
		UT_ASSERT_MSG(instanceObjectPath.length(), "Instance object path is not set!");

		OP_Node *instaceOpNode = getOpNodeFromPath(instanceObjectPath, ctx.getTime());
		UT_ASSERT_MSG(instanceObjectPath, "Instance object is not found!");

		OBJ_Node *instaceObjNode = CAST_OBJNODE(instaceOpNode);
		UT_ASSERT_MSG(instaceObjNode, "Instance object is not an OBJ node!");

		uint32_t additional_params_flags = 0;

		VRay::Plugin instanceNode = pluginExporter.exportObject(instaceObjNode);

		VRay::Plugin instanceMtl;
		if (materialPathHndl.isValid()) {
			const UT_String &instanceMtlPath = materialPathHndl.get(ptOff);
			OP_Node *instaceMatNode = getOpNodeFromPath(instanceMtlPath, ctx.getTime());
			if (instaceMatNode) {
				instanceMtl = pluginExporter.exportMaterial(instaceMatNode);
				if (instanceMtl) {
					additional_params_flags |= useMaterial;
				}
			}
		}

		QString userAttributes;
		GEOAttribList attrList;
		gdp.getAttributes().matchAttributes(GEOgetV3AttribFilter(), GA_ATTRIB_POINT, attrList);
		for (const GA_Attribute *attr : attrList) {
			if (attr &&
				attr->getName() != GEO_STD_ATTRIB_POSITION &&
				attr->getName() != GEO_STD_ATTRIB_NORMAL &&
				attr->getName() != GEO_STD_ATTRIB_VELOCITY)
			{
				GA_ROHandleV3 attrHndl(attr);
				if (attrHndl.isValid()) {
					const UT_Vector3 &value = attrHndl.get(ptOff);

					userAttributes += QString::asprintf("%s=%g,%g,%g;",
														attr->getName().buffer(), value.x(), value.y(), value.z());
				}
			}
		}

		// Index + TM + VEL_TM + Flags + Node
		int itemListSize = 5;

		if (instanceMtl) {
			itemListSize++;
		}

		if (userAttributes.length()) {
			additional_params_flags |= useUserAttributes;
			itemListSize++;
		}

		int itemListOffs = 0;

		VRay::VUtils::ValueRefList item(itemListSize);

		// Particle index.
		item[itemListOffs++].setDouble(validPointIdx);

		// Particle transform:
		//   http://www.sidefx.com/docs/houdini/copy/instanceattrs
		//
		const UT_Vector3 &point = gdp.getPos3(ptOff);
		VRay::Transform tm;
		tm.matrix.makeIdentity();
		tm.offset.set(point.x(), point.y(), point.z());

		VRay::Vector scale(1.0f, 1.0f, 1.0f);
		if (scaleHndl.isValid()) {
			const UT_Vector3 &s = scaleHndl.get(ptOff);
			scale.x *= s.x();
			scale.y *= s.y();
			scale.z *= s.z();
		}
		if (pscaleHndl.isValid()) {
			scale *= pscaleHndl.get(ptOff);
		}

		tm.matrix.v0.x *= scale.x;
		tm.matrix.v1.y *= scale.y;
		tm.matrix.v2.z *= scale.z;

		// Mult with object inv. tm.
		VRay::Transform objTm = VRayExporter::getObjTransform(instaceObjNode, ctx, false);
		objTm.makeInverse();
		tm = tm * objTm;

		item[itemListOffs++].setTransform(tm);

		// Particle velocity.
		UT_Vector3 velocity(0.0, 0.0, 0.0);
		if (velocityHndl.isValid()) {
			// XXX: This value seems incorrect.
			// velocity = velocityHndl.get(ptOff);
		}

		VRay::VUtils::TraceTransform vel;
		vel.m.makeZero();
		vel.ffs.x = velocity.x();
		vel.ffs.y = velocity.y();
		vel.ffs.z = velocity.z();

		item[itemListOffs++].setTraceTransform(vel);

		item[itemListOffs++].setDouble(additional_params_flags);

		if (additional_params_flags & useUserAttributes) {
			item[itemListOffs++].setString(userAttributes.toLocal8Bit().constData());
		}
		if (additional_params_flags & useMaterial) {
			item[itemListOffs++].setPlugin(instanceMtl);
		}

		item[itemListOffs++].setPlugin(instanceNode);

		// +1 because first element is time.
		instances[validPointIdx+1].setList(item);

		++validPointIdx;
	}

	instancer2.addAttribute(Attrs::PluginAttr("instances", instances));
	instancer2.addAttribute(Attrs::PluginAttr("use_additional_params", true));
	instancer2.addAttribute(Attrs::PluginAttr("use_time_instancing", false));
	
	return pluginExporter.exportPlugin(instancer2);
}

VRay::Plugin GeometryExporter::exportGeometry()
{
	SOP_Node *renderSOP = objNode.getRenderSopPtr();
	if (!renderSOP) {
		return VRay::Plugin();
	}

	const UT_String &renderOpType = renderSOP->getOperator()->getName();
	if (renderOpType.startsWith("VRayNode") &&
		!renderOpType.equal("VRayNodePhxShaderCache") &&
		!renderOpType.equal("VRayNodeVRayProxy"))
	{
		return exportVRaySOP(*renderSOP);
	}

	GU_DetailHandleAutoReadLock gdl(renderSOP->getCookedGeoHandle(ctx));
	if (!gdl) {
		return VRay::Plugin();
	}

	const GU_Detail &gdp = *gdl;

	if (isPointInstancer(gdp)) {
		return exportPointInstancer(gdp);
	}

	return exportDetail(*renderSOP, gdp);
}

VRay::Plugin ObjectExporter::exportNode()
{
	using namespace Attrs;

	OpPluginCache::iterator pIt = opPluginCache.find(&objNode);
	if (pIt != opPluginCache.end()) {
		return pIt.data();
	}

	OP_Context &cxt = pluginExporter.getContext();

	GeometryExporter geoExporter(*this, objNode, pluginExporter);
	VRay::Plugin geometry = geoExporter.exportGeometry();

	OP_Node *matNode = objNode.getMaterialNode(cxt.getTime());
	VRay::Plugin material = pluginExporter.exportMaterial(matNode);

	const VRay::Transform transform = pluginExporter.getObjTransform(&objNode, cxt);

	PluginDesc nodeDesc(VRayExporter::getPluginName(objNode, "Node"),"Node");
	nodeDesc.add(PluginAttr("geometry", geometry));
	nodeDesc.add(PluginAttr("material", material));
	nodeDesc.add(PluginAttr("transform", transform));
	nodeDesc.add(PluginAttr("visible", GeometryExporter::isNodeVisible(pluginExporter.getRop(), objNode)));

	VRay::Plugin node = pluginExporter.exportPlugin(nodeDesc);
	UT_ASSERT(node);

	opPluginCache.insert(&objNode, node);

	return node;
}
