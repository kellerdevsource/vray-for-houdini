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

static struct PrimPackedTypeIDs {
	PrimPackedTypeIDs()
		: initialized(false)
		, alembicRef(0)
		, packedDisk(0)
		, packedGeometry(0)
		, vrayProxyRef(0)
	{}

	void init() {
		if (initialized)
			return;

		alembicRef = GU_PrimPacked::lookupTypeId("AlembicRef");
		packedDisk = GU_PrimPacked::lookupTypeId("PackedDisk");
		packedGeometry = GU_PrimPacked::lookupTypeId("PackedGeometry");
		vrayProxyRef = GU_PrimPacked::lookupTypeId("VRayProxyRef");

		initialized = true;
	}

private:
	int initialized;

public:
	GA_PrimitiveTypeId alembicRef;
	GA_PrimitiveTypeId packedDisk;
	GA_PrimitiveTypeId packedGeometry;
	GA_PrimitiveTypeId vrayProxyRef;
} primPackedTypeIDs;

/// XXX: Move to ObjectExporter
typedef VUtils::HashMapKey<OP_Node*, VRay::Plugin> OpPluginCache;
typedef VUtils::HashMapKey<int, VRay::Plugin> PrimPluginCache;
typedef VUtils::HashMap<VRay::Plugin> GeomNodeCache;
static OpPluginCache opPluginCache;
static PrimPluginCache primPluginCache;
static GeomNodeCache geomNodeCache;

void VRayForHoudini::clearOpPluginCache() {
	// clearOpPluginCache() is called before export,
	// so we could init types here.
	primPackedTypeIDs.init();

	opPluginCache.clear();
}

void VRayForHoudini::clearPrimPluginCache() {
	// clearOpPluginCache() is called before export,
	// so we could init types here.
	primPackedTypeIDs.init();

	primPluginCache.clear();
	geomNodeCache.clear();
}

static const char intrAlembicFilename[] = "abcfilename";
static const char intrAlembicObjectPath[] = "abcobjectpath";
static const char intrPackedPrimName[] = "packedprimname";
static const char intrPackedPrimitiveName[] = "packedprimitivename";
static const char intrPackedLocalTransform[] = "packedlocaltransform";
static const char intrGeometryID[] = "geometryid";
static const char intrFilename[] = "filename";

static const char VFH_ATTR_MATERIAL_ID[] = "switchmtl";

static const UT_String typeGeomStaticMesh = "GeomStaticMesh";
static const UT_String typeNode = "Node";

/// Identity transform.
static VRay::Transform identityTm(1);

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

VRay::Plugin GeometryExporter::getNodeForInstancerGeometry(VRay::Plugin geometry)
{
	if (!geometry) {
		return VRay::Plugin();
	}

	// Already a Node plugin.
	if (UT_String("Node").equal(geometry.getType())) {
		return geometry;
	}

	GeomNodeCache::iterator gnIt = geomNodeCache.find(geometry.getName());
	if (gnIt != geomNodeCache.end()) {
		return gnIt.data();
	}

	static boost::format nodeNameFmt("Node@%s");

	// Wrap into Node plugin.
	Attrs::PluginDesc nodeDesc(boost::str(nodeNameFmt % geometry.getName()),
								"Node");
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geometry));
	nodeDesc.addAttribute(Attrs::PluginAttr("material", pluginExporter.exportDefaultMaterial()));
	nodeDesc.addAttribute(Attrs::PluginAttr("transform", VRay::Transform(1)));
	nodeDesc.addAttribute(Attrs::PluginAttr("visible", false));

	VRay::Plugin node = pluginExporter.exportPlugin(nodeDesc);
	UT_ASSERT(node);

	geomNodeCache.insert(geometry.getName(), node);

	return node;
}

/// Ensures "dynamic_geometry" is set for GeomStaticMesh.
/// @param plugin Node or geometry plugin.
static void ensureDynamicGeometryForInstancer(VRay::Plugin plugin)
{
	VRay::Plugin geometry = plugin;
	if (typeNode.equal(geometry.getType())) {
		geometry = geometry.getPlugin("geometry");
	}
	if (typeGeomStaticMesh.equal(geometry.getType())) {
		geometry.setValue("dynamic_geometry", true);
	}
}

VRay::Plugin GeometryExporter::exportDetail(const GU_Detail &gdp)
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
#ifdef CGR_HAS_AUR
		VolumeExporter volExp(objNode, ctx, pluginExporter);
		volExp.exportPrimitives(gdp, instancerItems);

		HoudiniVolumeExporter hVoldExp(objNode, ctx, pluginExporter);
		hVoldExp.exportPrimitives(gdp, instancerItems);
#endif
		HairPrimitiveExporter hairExp(objNode, ctx, pluginExporter);
		hairExp.exportPrimitives(gdp, instancerItems);

		if (GU_PrimPacked::hasPackedPrimitives(gdp)) {
			UT_Array<const GA_Primitive*> prims;
			GU_PrimPacked::getPackedPrimitives(gdp, prims);
			for (const GA_Primitive *prim : prims) {
				const GU_PrimPacked *primPacked = UTverify_cast<const GU_PrimPacked*>(prim);
				VRay::Plugin fromPacked = exportPacked(*primPacked);
				if (fromPacked) {
					const GA_Detail &parentGdp = prim->getDetail();

					GA_ROHandleS materialPathHndl(parentGdp.findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
					// GA_ROHandleS materialOverHndl(parentGdp.findAttribute(GA_ATTRIB_PRIMITIVE, "material_override"));

					UT_Matrix4D fullxform;
					primPacked->getFullTransform4(fullxform);

					ensureDynamicGeometryForInstancer(fromPacked);

					InstancerItem item;
					item.geometry = fromPacked;
					item.tm = VRayExporter::Matrix4ToTransform(fullxform);

					if (materialPathHndl.isValid()) {
						const UT_String &matPath = materialPathHndl.get(prim->getMapOffset());
						OP_Node *matNode = getOpNodeFromPath(matPath, ctx.getTime());
						if (matNode) {
							VRay::Plugin material = pluginExporter.exportMaterial(matNode);
							if (material) {
								item.material = material;
							}
						}
					}

					instancerItems += item;
				}
			}
		}

		VRay::Plugin fromPoly = exportPolyMesh(gdp);
		if (fromPoly) {
			plugins += fromPoly;
		}
	}

	if (!plugins.count() && !instancerItems.count()) {
		return VRay::Plugin();
	}
	if (plugins.count() == 1 || instancerItems.count() == 1) {
		if (plugins.count())
			return plugins[0];
		if (instancerItems.count())
			return instancerItems[0].geometry;
	}

	// Combine point particles and geometry under Instancer2.
	int instanceIdx = 0;
	int instancesListIdx = 0;

	const int numParticles = plugins.count() + instancerItems.count();

	// +1 because first value is time.
	VRay::VUtils::ValueRefList instances(numParticles+1);
	instances[instancesListIdx++].setDouble(0.0);

	for (int i = 0; i < plugins.count(); ++i) {
		VRay::Plugin plugin = plugins[i];

		// Instancer works only with Node plugins.
		VRay::Plugin node = getNodeForInstancerGeometry(plugin);

		// Index + TM + VEL_TM + Node
		VRay::VUtils::ValueRefList item(4);
		item[0].setDouble(instanceIdx++);
		item[1].setTransform(identityTm);
		item[2].setTransform(identityTm);
		item[3].setPlugin(node);

		instances[instancesListIdx++].setList(item);
	}

	for (int i = 0; i < instancerItems.count(); ++i) {
		const InstancerItem &instancerItem = instancerItems[i];
		
		uint32_t additional_params_flags = useMaterial;

		// Instancer works only with Node plugins.
		VRay::Plugin node = getNodeForInstancerGeometry(instancerItem.geometry);

		// Index + TM + VEL_TM + flags + Mtl + Node
		const int itemSize = 6;

		VRay::Plugin itemMtl = instancerItem.material
		                ? instancerItem.material
		                : pluginExporter.exportDefaultMaterial();

		VRay::VUtils::ValueRefList item(itemSize);
		int indexOffs = 0;
		item[indexOffs++].setDouble(instanceIdx++);
		item[indexOffs++].setTransform(instancerItem.tm);
		item[indexOffs++].setTransform(VRay::Transform(0));
		item[indexOffs++].setDouble(additional_params_flags);
		item[indexOffs++].setPlugin(itemMtl);
		item[indexOffs++].setPlugin(node);

		instances[instancesListIdx++].setList(item);
	}

	instancerItems.clear();

	Attrs::PluginDesc instancer2(VRayExporter::getPluginName(objNode, "Geom"),
								 "Instancer2");
	instancer2.addAttribute(Attrs::PluginAttr("instances", instances));
	instancer2.addAttribute(Attrs::PluginAttr("use_additional_params", true));
	instancer2.addAttribute(Attrs::PluginAttr("use_time_instancing", false));

	return pluginExporter.exportPlugin(instancer2);
}

VRay::Plugin GeometryExporter::exportPolyMesh(const GU_Detail &gdp)
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
		}
	}
	return VRay::Plugin();
}

VRay::Plugin GeometryExporter::exportPacked(const GU_PrimPacked &prim)
{
	const int packedID = getPrimPackedID(prim);

	PrimPluginCache::iterator pIt = primPluginCache.find(packedID);
	if (pIt != primPluginCache.end()) {
		return pIt.data();
	}

	VRay::Plugin primPlugin = exportPrimPacked(prim);
	if (!primPlugin) {
		UT_ASSERT_MSG(false, "Unsupported packed primitive type!");
	}
	else {
		primPluginCache.insert(packedID, primPlugin);
	}

	return primPlugin;

#if 0
	SHOPHasher hasher;
	for (Attrs::PluginDesc &pluginDesc : primPluginList) {
		// we assign material and user_attributes only if material
		// has not been previously assigned. Existing VFH_ATTR_MATERIAL_ID
		// property signals that the node already has material and overrides set
		attr = nodeDesc.get(VFH_ATTR_MATERIAL_ID);
		if (NOT(attr) && mtlpath.isValid()) {
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
#endif
}

int GeometryExporter::getPrimPackedID(const GU_PrimPacked &prim)
{
	const GA_PrimitiveTypeId primTypeID = prim.getTypeId();

	if (primTypeID == primPackedTypeIDs.packedGeometry) {
		int geoID = -1;
		prim.getIntrinsic(prim.findIntrinsic(intrGeometryID), geoID);
		return geoID;
	}
	if (primTypeID == primPackedTypeIDs.alembicRef ||
		primTypeID == primPackedTypeIDs.packedDisk)
	{
		UT_String primname;
		prim.getIntrinsic(prim.findIntrinsic(intrPackedPrimitiveName), primname);
		return primname.hash();
	}
	if (primTypeID ==  primPackedTypeIDs.vrayProxyRef) {
		const VRayProxyRef *vrayproxyref = UTverify_cast<const VRayProxyRef*>(prim.implementation());
		return vrayproxyref->getOptions().hash();
	}

	UT_ASSERT(false);

	return 0;
}

VRay::Plugin GeometryExporter::exportPrimPacked(const GU_PrimPacked &prim)
{
	const GA_PrimitiveTypeId primTypeID = prim.getTypeId();

	if (primTypeID == primPackedTypeIDs.packedGeometry) {
		return exportPackedGeometry(prim);
	}
	if (primTypeID == primPackedTypeIDs.alembicRef) {
		return exportAlembicRef(prim);
	}
	if (primTypeID == primPackedTypeIDs.vrayProxyRef) {
		return exportVRayProxyRef(prim);
	}
	if (primTypeID == primPackedTypeIDs.packedDisk) {
		return exportPackedDisk(prim);
	}

	UT_ASSERT(false);

	return VRay::Plugin();
}

VRay::Plugin GeometryExporter::exportAlembicRef(const GU_PrimPacked &prim)
{
	if (!m_exportGeometry) {
		return VRay::Plugin();
	}

	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic(intrPackedPrimitiveName), primname);

	UT_String filename;
	prim.getIntrinsic(prim.findIntrinsic(intrAlembicFilename), filename);

	UT_String objname;
	prim.getIntrinsic(prim.findIntrinsic(intrAlembicObjectPath), objname);

	VRay::VUtils::CharStringRefList visibilityList(1);
	visibilityList[0] = objname;

	Attrs::PluginDesc pluginDesc(primname.buffer(),
								 "GeomMeshFile");
	pluginDesc.addAttribute(Attrs::PluginAttr("use_full_names", true));
	pluginDesc.addAttribute(Attrs::PluginAttr("visibility_lists_type", 1));
	pluginDesc.addAttribute(Attrs::PluginAttr("visibility_list_names", visibilityList));
	pluginDesc.addAttribute(Attrs::PluginAttr("file", filename.toStdString()));
	pluginDesc.addAttribute(Attrs::PluginAttr("use_alembic_offset", true));

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin GeometryExporter::exportVRayProxyRef(const GU_PrimPacked &prim)
{
	if (!m_exportGeometry) {
		return VRay::Plugin();
	}

	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic(intrPackedPrimitiveName), primname);

	Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(objNode, primname.buffer()),
								 "GeomMeshFile");

	const VRayProxyRef *vrayproxyref = UTverify_cast<const VRayProxyRef*>(prim.implementation());

	UT_Options options = vrayproxyref->getOptions();
#if 0
	// TODO: Utilize "use_time_instancing" here
	if (options.hasOption("anim_offset")) {
		options.setOptionF("anim_offset", options.getOptionF("anim_offset") - ctx.getFrame());
	}
#endif
	pluginExporter.setAttrsFromUTOptions(pluginDesc, options);

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin GeometryExporter::exportPackedDisk(const GU_PrimPacked &prim)
{
	if (!m_exportGeometry) {
		return VRay::Plugin();
	}

	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic(intrPackedPrimName), primname);

	UT_String filename;
	prim.getIntrinsic(prim.findIntrinsic(intrFilename), filename);

	Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(objNode, primname.buffer()),
								 "GeomMeshFile");
	pluginDesc.addAttribute(Attrs::PluginAttr("file", filename.toStdString()));

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin GeometryExporter::exportPackedGeometry(const GU_PrimPacked &prim)
{
	GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
	if (!gdl) {
		return VRay::Plugin();
	}
	return exportDetail(*gdl);
}

ObjectExporter::ObjectExporter(VRayExporter &pluginExporter, OBJ_Node &objNode)
	: pluginExporter(pluginExporter)
	, objNode(objNode)
	, exportGeometry(true)
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

int GeometryExporter::isInstanceNode(const OP_Node &node)
{
	return node.getOperator()->getName().equal("instance");	
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
				color[positionsIdx].b = cd.z();
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

/// Holds attributes for calculating instance transform:
///   http://www.sidefx.com/docs/houdini/copy/instanceattrs
struct PointInstanceAttrs {
	explicit PointInstanceAttrs(const GU_Detail &gdp) {
		orient     = gdp.findPointAttribute(GEO_STD_ATTRIB_ORIENT);
		pscale     = gdp.findPointAttribute(GEO_STD_ATTRIB_PSCALE);
		scale      = gdp.findPointAttribute("scale");
		n          = gdp.findPointAttribute(GEO_STD_ATTRIB_NORMAL);
		up         = gdp.findPointAttribute(GEO_STD_ATTRIB_UP);
		v          = gdp.findPointAttribute(GEO_STD_ATTRIB_VELOCITY);
		rot        = gdp.findPointAttribute("rot");
		trans      = gdp.findPointAttribute("trans");
		pivot      = gdp.findPointAttribute("pivot");
		transform3 = gdp.findPointAttribute("transform");
		transform4 = gdp.findPointAttribute("transform");
	}

	GA_ROHandleV4 orient;
	GA_ROHandleF  pscale;
	GA_ROHandleV3 scale;
	GA_ROHandleV3 n;
	GA_ROHandleV3 up;
	GA_ROHandleV3 v;
	GA_ROHandleV4 rot;
	GA_ROHandleV3 trans;
	GA_ROHandleV3 pivot;
	GA_ROHandleM3 transform3;
	GA_ROHandleM4 transform4;
};

/// Returns instance transform from point attributes:
///   http://www.sidefx.com/docs/houdini/copy/instanceattrs
/// @param gdp Detail.
/// @param ptOff Point offset.
static VRay::Transform getPointInstanceTM(const GU_Detail &gdp, const PointInstanceAttrs &attrs, GA_Offset ptOff)
{
	// X = pivot matrix (translate by -pivot)
	UT_Matrix4F X(1);
	if (attrs.pivot.isValid()) {
		const UT_Vector3F pivot = attrs.pivot.get(ptOff);
		X.translate(-pivot);
	}

	// O = orient matrix 
	UT_Matrix3F O(1);
	if (attrs.orient.isValid()) {
		const UT_QuaternionF orient = attrs.orient.get(ptOff);
		orient.getRotationMatrix(O);
	}

	// S = scale matrix (scale * pscale) 
	UT_Matrix3F S(1);
	UT_Vector3F scale(1.0f, 1.0f, 1.0f);
	if (attrs.scale.isValid()) {
		scale = attrs.scale.get(ptOff);
	}
	if (attrs.pscale.isValid()) {
		scale *= attrs.pscale.get(ptOff);
	}
	S.scale(scale);

	// L = alignment matrix
	// IF N exists AND up exists and isn't {0,0,0}: 
	//    L = mlookatup(N,0,up) 
	// ELSE IF N exists: 
	//    L = dihedral({0,0,1},N) 
	// ELSE IF v exists AND up exists and isn't {0,0,0}: 
	//    L = mlookatup(v,0,up) 
	// ELSE IF v exists: 
	//    L = dihedral({0,0,1},v) 
	UT_Matrix3F L(1);
	if (attrs.n.isValid() || attrs.v.isValid()) {
		GA_ROHandleV3 nvHndl = attrs.n.isValid() ? attrs.n : attrs.v;

		UT_Vector3F nv = nvHndl.get(ptOff);
		if (attrs.up.isValid()) {
			const UT_Vector3F &up = attrs.up.get(ptOff);
			if (!up.isEqual(UT_Vector3F(0.0f, 0.0f, 0.0f))) {
				L.lookat(nv, up, 0);
			}
		}
		else {
			UT_Vector3F a(0.0f, 0.0f, 1.0f);
			L.dihedral(a, nv);
		}
	}

	// R = rot matrix 
	UT_Matrix3F R(1);
	if (attrs.rot.isValid()) {
		const UT_QuaternionF rot = attrs.rot.get(ptOff);
		rot.getRotationMatrix(R);
	}

	// T = trans matrix (trans + P) 
	UT_Matrix4F T(1);
	UT_Vector3F P = gdp.getPos3(ptOff);
	if (attrs.trans.isValid()) {
		P += attrs.trans.get(ptOff);
	}
	T.setTranslates(P);

	// M = transform matrix
	UT_Matrix3F M(1);
	if (attrs.transform3.isValid()) {
		M = attrs.transform3.get(ptOff);
	}
	else if (attrs.transform4.isValid()) {
		M = attrs.transform4.get(ptOff);
	}

	// IF transform exists:
	//    Transform = X*M*T
	// ELSE IF orient exists: 
	//    Transform = X*S*(O*R)*T
	// ELSE: 
	//    Transform = X*S*L*R*T
	UT_Matrix4F Transform;
	if (attrs.transform3.isValid() ||
		attrs.transform4.isValid())
	{
		Transform = X * toM4(M) * T;
	}
	else if (attrs.orient.isValid()) {
		Transform = X * toM4(S * (O * R)) * T;
	}
	else {
		Transform = X * toM4(S * L * R) * T;
	}

	return utMatrixToVRayTransform(Transform);
}

VRay::Plugin GeometryExporter::exportPointInstancer(const GU_Detail &gdp, int isInstanceNode)
{
	const GA_Size numPoints = gdp.getNumPoints();

	PointInstanceAttrs pointInstanceAttrs(gdp);

	GA_ROHandleV3 velocityHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_VELOCITY));

	GA_ROHandleS instanceHndl(gdp.findAttribute(GA_ATTRIB_POINT, "instance"));
	GA_ROHandleS instancePathHndl(gdp.findAttribute(GA_ATTRIB_POINT, "instancepath"));
	GA_ROHandleS materialPathHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_MATERIAL));

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
		else if (isInstanceNode) {
			objNode.evalString(instanceObjectPath, "instancepath", 0, ctx.getTime());
		}
		UT_ASSERT_MSG(instanceObjectPath.length(), "Instance object path is not set!");

		OP_Node *instaceOpNode = getOpNodeFromPath(instanceObjectPath, ctx.getTime());
		UT_ASSERT_MSG(instanceObjectPath, "Instance object is not found!");

		OBJ_Node *instaceObjNode = CAST_OBJNODE(instaceOpNode);
		UT_ASSERT_MSG(instaceObjNode, "Instance object is not an OBJ node!");

		uint32_t additional_params_flags = 0;

		VRay::Plugin instanceNode = pluginExporter.exportObject(instaceObjNode);
		ensureDynamicGeometryForInstancer(instanceNode);

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
		// TODO: Export non-vector attributes.
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

		// Particle transform.
		VRay::Transform tm = getPointInstanceTM(gdp, pointInstanceAttrs, ptOff);

		// Mult with object inv. tm.
		VRay::Transform objTm = VRayExporter::getObjTransform(instaceObjNode, ctx, false);
		objTm.makeInverse();
		tm = tm * objTm;

		item[itemListOffs++].setTransform(tm);

		// Particle velocity.
		VRay::Transform vel(0);
		if (velocityHndl.isValid()) {
			// const UT_Vector3 &velocity = velocityHndl.get(ptOff);
			// vel.offset.set(velocity.x(), velocity.y(), velocity.z());
			// vel.offset = vel.offset * objTm;
		}

		item[itemListOffs++].setTransform(vel);

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

VRay::Plugin GeometryExporter::exportGeometry(SOP_Node &renderSOP)
{
	const UT_String &renderOpType = renderSOP.getOperator()->getName();
	if (renderOpType.startsWith("VRayNode") &&
		!renderOpType.equal("VRayNodePhxShaderCache") &&
		!renderOpType.equal("VRayNodeVRayProxy"))
	{
		return exportVRaySOP(renderSOP);
	}

	GU_DetailHandleAutoReadLock gdl(renderSOP.getCookedGeoHandle(ctx));
	if (!gdl) {
		return VRay::Plugin();
	}

	const GU_Detail &gdp = *gdl;

	const int isInstance = isInstanceNode(objNode);
	if (isInstance || isPointInstancer(gdp)) {
		return exportPointInstancer(gdp, isInstance);
	}

	return exportDetail(gdp);
}

VRay::Plugin GeometryExporter::exportGeometry()
{
	SOP_Node *renderSOP = objNode.getRenderSopPtr();
	if (!renderSOP) {
		return VRay::Plugin();
	}

	return exportGeometry(*renderSOP);
}

VRay::Plugin ObjectExporter::exportNode()
{
	using namespace Attrs;

	OpPluginCache::iterator pIt = opPluginCache.find(&objNode);
	if (pIt != opPluginCache.end()) {
		return pIt.data();
	}

	OP_Context &cxt = pluginExporter.getContext();

	VRay::Plugin geometry;
	if (exportGeometry) {
		GeometryExporter geoExporter(*this, objNode, pluginExporter);
		geometry = geoExporter.exportGeometry();
	}

	OP_Node *matNode = objNode.getMaterialNode(cxt.getTime());
	VRay::Plugin material = pluginExporter.exportMaterial(matNode);

	const VRay::Transform transform = pluginExporter.getObjTransform(&objNode, cxt);

	PluginDesc nodeDesc(VRayExporter::getPluginName(objNode, "Node"),"Node");
	if (exportGeometry) {
		nodeDesc.add(PluginAttr("geometry", geometry));
	}
	nodeDesc.add(PluginAttr("material", material));
	nodeDesc.add(PluginAttr("transform", transform));
	nodeDesc.add(PluginAttr("visible", GeometryExporter::isNodeVisible(pluginExporter.getRop(), objNode)));

	VRay::Plugin node = pluginExporter.exportPlugin(nodeDesc);
	UT_ASSERT(node);

	opPluginCache.insert(&objNode, node);

	return node;
}
