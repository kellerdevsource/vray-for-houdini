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
#include "vfh_op_utils.h"
#include "vfh_exporter.h"
#include "vfh_geoutils.h"

#include "gu/gu_vrayproxyref.h"
#include "gu/gu_volumegridref.h"
#include "rop/vfh_rop.h"
#include "sop/sop_node_base.h"
#include "vop/vop_node_base.h"
#include "obj/obj_node_base.h"

#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimSphere.h>
#include <OP/OP_Bundle.h>
#include <GA/GA_Types.h>
#include <GA/GA_Names.h>

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
		vrayVolumeGridRef = GU_PrimPacked::lookupTypeId("VRayVolumeGridRef");

		initialized = true;
	}

private:
	int initialized;

public:
	GA_PrimitiveTypeId alembicRef;
	GA_PrimitiveTypeId packedDisk;
	GA_PrimitiveTypeId packedGeometry;
	GA_PrimitiveTypeId vrayProxyRef;
	GA_PrimitiveTypeId vrayVolumeGridRef;
} primPackedTypeIDs;

static boost::format objGeomNameFmt("%s|%i@%s");

static const char intrAlembicFilename[] = "abcfilename";
static const char intrAlembicObjectPath[] = "abcobjectpath";
static const char intrPackedPrimName[] = "packedprimname";
static const char intrPackedPrimitiveName[] = "packedprimitivename";
static const char intrPackedLocalTransform[] = "packedlocaltransform";
static const char intrGeometryID[] = "geometryid";
static const char intrFilename[] = "filename";

static const UT_String vrayPluginTypeGeomStaticMesh = "GeomStaticMesh";
static const UT_String vrayPluginTypeNode = "Node";

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

ObjectExporter::ObjectExporter(VRayExporter &pluginExporter)
	: pluginExporter(pluginExporter)
	, ctx(pluginExporter.getContext())
	, doExportGeometry(true)
{}

VRay::Transform ObjectExporter::getTm() const
{
	VRay::Transform tm(1);

	PrimContextIt it(primContextStack);
	while (it.hasNext()) {
		tm = it.next().tm * tm;
	}

	return tm;
}

exint ObjectExporter::getDetailID() const
{
	exint detailID = 0;

	PrimContextIt it(primContextStack);
	while (it.hasNext()) {
		detailID ^= it.next().detailID;
	}

	return detailID;
}

OP_Node* ObjectExporter::getGenerator() const
{
	if (!primContextStack.size())
		return nullptr;
	// Return the bottom-most generator.
	return primContextStack[0].generator;
}

void ObjectExporter::getPrimMaterial(PrimMaterial &primMaterial) const
{
	PrimContextIt it(primContextStack);
	it.toBack();
	while (it.hasPrevious()) {
		const PrimMaterial &primMtl(it.previous().primMaterial);
		if (primMtl.matNode) {
			primMaterial.matNode = primMtl.matNode;
		}

		FOR_CONST_IT (MtlOverrideItems, moIt, primMtl.overrides) {
			primMaterial.overrides.insert(moIt.key(), moIt.data());
		}
	}
}

void ObjectExporter::clearOpPluginCache()
{
	// clearOpPluginCache() is called before export,
	// so we could init types here.
	primPackedTypeIDs.init();

	pluginCache.op.clear();
}

void ObjectExporter::clearOpDepPluginCache()
{
	pluginCache.generated.clear();
}

void ObjectExporter::clearPrimPluginCache()
{
	// clearOpPluginCache() is called before export,
	// so we could init types here.
	primPackedTypeIDs.init();

	pluginCache.prim.clear();
	pluginCache.instancerNodeWrapper.clear();
}

bool ObjectExporter::hasSubdivApplied(OBJ_Node &objNode) const
{
	// Here we check if subdivision has been assigned to this node
	// at render time. V-Ray subdivision is implemented in 2 ways:
	// 1. as a custom VOP available in V-Ray material context
	// 2. as spare parameters added to the object node.
	bool res = false;

	fpreal t = ctx.getTime();
	bool hasDispl = objNode.hasParm("vray_use_displ") && objNode.evalInt("vray_use_displ", 0, t);
	if (NOT(hasDispl)) {
		return res;
	}

	const int displType = objNode.evalInt("vray_displ_type", 0, t);
	switch (displType) {
		case displacementTypeFromMat: {
			UT_String shopPath;
			objNode.evalString(shopPath, "vray_displshoppath", 0, t);
			OP_Node *shop = getOpNodeFromPath(shopPath, t);
			if (shop) {
				if (getVRayNodeFromOp(*shop, "Geometry", "GeomStaticSmoothedMesh")) {
					res = true;
				}
			}
			break;
		}
		case displacementTypeDisplace:
		case displacementTypeSmooth: {
			res = true;
			break;
		}
		default: {
			break;
		}
	}

	return res;
}

int ObjectExporter::isNodeVisible(VRayRendererNode &rop, OBJ_Node &objNode)
{
	OP_Bundle *bundle = rop.getForcedGeometryBundle();
	if (!bundle) {
		return objNode.getVisible();
	}
	return bundle->contains(&objNode, false) || objNode.getVisible();
}

int ObjectExporter::isNodeVisible(OBJ_Node &objNode) const
{
	return isNodeVisible(pluginExporter.getRop(), objNode);
}

int ObjectExporter::isNodeMatte(OBJ_Node &objNode) const
{
	VRayRendererNode &rop = pluginExporter.getRop();
	OP_Bundle *bundle = rop.getMatteGeometryBundle();
	if (!bundle) {
		return false;
	}
	return bundle->contains(&objNode, false);
}

int ObjectExporter::isNodePhantom(OBJ_Node &objNode) const
{
	VRayRendererNode &rop = pluginExporter.getRop();
	OP_Bundle *bundle = rop.getPhantomGeometryBundle();
	if (!bundle) {
		return false;
	}
	return bundle->contains(&objNode, false);
}

VRay::Plugin ObjectExporter::exportVRaySOP(OBJ_Node&, SOP_Node &sop)
{
	if (!doExportGeometry) {
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

VRay::Plugin ObjectExporter::getNodeForInstancerGeometry(VRay::Plugin geometry, VRay::Plugin objMaterial)
{
	if (!geometry) {
		return VRay::Plugin();
	}

	// Already a Node plugin.
	if (vrayPluginTypeNode.equal(geometry.getType())) {
		return geometry;
	}

	GeomNodeCache::iterator gnIt = pluginCache.instancerNodeWrapper.find(geometry.getName());
	if (gnIt != pluginCache.instancerNodeWrapper.end()) {
		return gnIt.data();
	}

	static boost::format nodeNameFmt("Node@%s");

	// Wrap into Node plugin.
	Attrs::PluginDesc nodeDesc(boost::str(nodeNameFmt % geometry.getName()),
							   "Node");
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geometry));
	nodeDesc.addAttribute(Attrs::PluginAttr("material", objMaterial));
	nodeDesc.addAttribute(Attrs::PluginAttr("transform", VRay::Transform(1)));
	nodeDesc.addAttribute(Attrs::PluginAttr("visible", false));

	VRay::Plugin node = pluginExporter.exportPlugin(nodeDesc);
	UT_ASSERT(node);

	pluginCache.instancerNodeWrapper.insert(geometry.getName(), node);

	return node;
}

/// Ensures "dynamic_geometry" is set for GeomStaticMesh.
/// @param plugin Node or geometry plugin.
static void ensureDynamicGeometryForInstancer(VRay::Plugin plugin)
{
	VRay::Plugin geometry = plugin;
	if (vrayPluginTypeNode.equal(geometry.getType())) {
		geometry = geometry.getPlugin("geometry");
	}
	if (geometry && vrayPluginTypeGeomStaticMesh.equal(geometry.getType())) {
		geometry.setValue("dynamic_geometry", true);
	}
}

static void overrideItemsToUserAttributes(const MtlOverrideItems &overrides, QString &userAttributes) {
	QString buf;

	FOR_CONST_IT (MtlOverrideItems, oiIt, overrides) {
		const tchar *overrideName = oiIt.key();
		const MtlOverrideItem &overrideItem = oiIt.data();

		switch (overrideItem.getType()) {
			case MtlOverrideItem::itemTypeInt: {
				userAttributes += buf.sprintf("%s=%lld;",
											  overrideName,
											  overrideItem.valueInt);
				break;
			}
			case MtlOverrideItem::itemTypeDouble: {
				userAttributes += buf.sprintf("%s=%g",
											  overrideName,
											  overrideItem.valueDouble);
				break;
			}
			case MtlOverrideItem::itemTypeVector: {
				userAttributes += buf.sprintf("%s=%g,%g,%g;",
											  overrideName,
											  overrideItem.valueVector.x, overrideItem.valueVector.y, overrideItem.valueVector.z);
				break;
			}
			default: {
				vassert(false);
			}
		}
	}
}

int ObjectExporter::getPrimKey(const GA_Primitive &prim)
{
	int primKey = 0;

	if (prim.getTypeId() == GEO_PRIMSPHERE) {
		primKey = reinterpret_cast<uintptr_t>(&prim);
	}
	else if (GU_PrimPacked::isPackedPrimitive(prim)) {
		primKey = getPrimPackedID(static_cast<const GU_PrimPacked&>(prim));
	}

	return primKey;
}

int ObjectExporter::getPrimPluginFromCache(int primKey, VRay::Plugin &plugin)
{
	PrimPluginCache::iterator pIt = pluginCache.prim.find(primKey);
	if (pIt != pluginCache.prim.end()) {
		plugin = pIt.data();
		return true;
	}
	return false;
}

void ObjectExporter::addPrimPluginToCache(int primKey, VRay::Plugin &plugin)
{
	UT_ASSERT(primKey != 0);
	UT_ASSERT(plugin);

	pluginCache.prim.insert(primKey, plugin);
}

VRay::Plugin ObjectExporter::exportPrimSphere(OBJ_Node &objNode, const GA_Primitive&)
{
	Attrs::PluginDesc geomSphere(VRayExporter::getPluginName(objNode, "GeomSphere"),
								 "GeomSphere");
	geomSphere.addAttribute(Attrs::PluginAttr("radius", 1.0));
	geomSphere.addAttribute(Attrs::PluginAttr("subdivs", 8));

	return pluginExporter.exportPlugin(geomSphere);
}

void ObjectExporter::exportPrimVolume(OBJ_Node &objNode, const PrimitiveItem &item)
{
#ifdef CGR_HAS_AUR
	const GA_PrimitiveTypeId &primTypeID = item.prim->getTypeId();

	const VRay::Transform &tm = getTm();
	const exint detailID = getDetailID();

	PluginSet volumePlugins;

	if (primTypeID == primPackedTypeIDs.vrayVolumeGridRef) {
		VolumeExporter volumeGridExp(objNode, ctx, pluginExporter);
		volumeGridExp.setTM(tm);
		volumeGridExp.setDetailID(detailID);
		volumeGridExp.exportPrimitive(item, volumePlugins);
	}
	else if (primTypeID == GEO_PRIMVOLUME ||
			 primTypeID == GEO_PRIMVDB)
	{
		HoudiniVolumeExporter volumeExp(objNode, ctx, pluginExporter);
		volumeExp.setTM(tm);
		volumeExp.setDetailID(detailID);
		volumeExp.exportPrimitive(item, volumePlugins);
	}
	else {
		UT_ASSERT(false && "Unsupported volume primitive type!");
	}

	FOR_IT(PluginSet, pIt, volumePlugins) {
		addGenerated(objNode, pIt.key());
	}
#endif
}

void ObjectExporter::processPrimitives(OBJ_Node &objNode, const GU_Detail &gdp, PrimitiveItems &instancerItems)
{
	// TODO: Preallocate at least some space.
	GEOPrimList polyPrims;
	GEOPrimList hairPrims;

	const GA_Size numPoints = gdp.getNumPoints();
	const GA_Size numPrims = gdp.getNumPrimitives();

	GA_ROHandleS materialStyleSheetHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTR_MATERIAL_STYLESHEET));
	GA_ROHandleS materialOverrideHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTR_MATERIAL_OVERRIDE));
	GA_ROHandleS materialPathHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, GA_Names::shop_materialpath));

	GA_ROHandleI objectIdHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTRIB_OBJECTID));
	GA_ROHandleF animOffsetHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTRIB_ANIM_OFFSET));

	MtlOverrideAttrExporter attrExp(gdp);

	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);
		if (!prim) {
			continue;
		}

		const GA_Offset primOffset = prim->getMapOffset();
		const GA_Index primIndex = prim->getMapIndex();

		const GA_PrimitiveTypeId &primTypeID = prim->getTypeId();

		const bool isPackedPrim = GU_PrimPacked::isPackedPrimitive(*prim);
		const bool isVolumePrim = primTypeID == primPackedTypeIDs.vrayVolumeGridRef ||
								  primTypeID == GEO_PRIMVOLUME ||
								  primTypeID == GEO_PRIMVDB;

		PrimitiveItem item;
		item.prim = prim;
		item.primID = gdp.getUniqueId() ^ primOffset;

		if (objectIdHndl.isValid()) {
			item.objectID = objectIdHndl.get(primOffset);
		}

		if (animOffsetHndl.isValid()) {
			item.t = animOffsetHndl.get(primOffset);
		}

		// Material overrides.
		mergeMaterialOverride(item.primMaterial, materialStyleSheetHndl, materialPathHndl, materialOverrideHndl, primOffset, ctx.getTime());

		attrExp.fromPrimitive(item.primMaterial.overrides, primOffset);

		// Check parent overrides.
		getPrimMaterial(item.primMaterial);

		// Primitive attributes
		if (isPackedPrim) {
			const GU_PrimPacked &primPacked = static_cast<const GU_PrimPacked&>(*prim);

			// Getting transform here because VRayVolumeGridRef is also a packed primitive
			UT_Matrix4D tm4;
			if (isVolumePrim) {
				primPacked.getLocalTransform4(tm4);
			}
			else {
				primPacked.getFullTransform4(tm4);
			}
			item.tm = utMatrixToVRayTransform(tm4);

			// Point attributes for packed instancing.
			if (numPoints == numPrims) {
				const GA_Offset pointOffset = gdp.pointOffset(primIndex);

				attrExp.fromPoint(item.primMaterial.overrides, pointOffset);
			}
		}

		if (isVolumePrim) {
			pushContext(PrimContext(&objNode, item.tm, item.primID, item.primMaterial));
			exportPrimVolume(objNode, item);
			popContext();
		}
		else if (isPackedPrim) {
			const GU_PrimPacked &primPacked = static_cast<const GU_PrimPacked&>(*prim);
			const int primKey = getPrimKey(primPacked);

			// If key is 0 don't check cache.
			if (primKey > 0) {
				getPrimPluginFromCache(primKey, item.geometry);
			}
			if (!item.geometry) {
				pushContext(PrimContext(&objNode, item.tm, item.primID, item.primMaterial));
				item.geometry = exportPrimPacked(objNode, primPacked);
				popContext();
			}
			if (primKey > 0 && item.geometry) {
				addPrimPluginToCache(primKey, item.geometry);
			}
		}
		else if (primTypeID == GEO_PRIMSPHERE) {
			const GU_PrimSphere &primSphere = static_cast<const GU_PrimSphere&>(*prim);
			const int primKey = getPrimKey(primSphere);

			UT_Matrix4 tm4;
			primSphere.getTransform4(tm4);
			item.tm = utMatrixToVRayTransform(tm4);

			// If key is 0 don't check cache.
			if (primKey) {
				getPrimPluginFromCache(primKey, item.geometry);
			}
			if (!getPrimPluginFromCache(primKey, item.geometry)) {
				item.geometry = exportPrimSphere(objNode, *prim);
				if (item.geometry) {
					addPrimPluginToCache(primKey, item.geometry);
				}
			}
		}
		else if (primTypeID == GEO_PRIMPOLYSOUP) {
			polyPrims.append(prim);
		}
		else if (primTypeID == GEO_PRIMPOLY) {
			const GEO_PrimPoly &primPoly = static_cast<const GEO_PrimPoly&>(*prim);

			// TODO: Hair from open poly detection.
			const int isHair = true;
			if (primPoly.isClosed()) {
				polyPrims.append(prim);
			}
			else if (isHair) {
				hairPrims.append(prim);
			}
		}
		else if (primTypeID == GEO_PRIMNURBCURVE ||
				 primTypeID == GEO_PRIMBEZCURVE)
		{
			hairPrims.append(prim);
		}

		if (item.geometry) {
			instancerItems += item;
		}
	}

	// NOTE: For polygon and hair material overrides are baked as map channels.
	exportPolyMesh(objNode, gdp, polyPrims, instancerItems);
	exportHair(objNode, gdp, hairPrims, instancerItems);
}

VRay::Plugin ObjectExporter::exportDetailInstancer(OBJ_Node &objNode, const GU_Detail &gdp, const PrimitiveItems &instancerItems, const char *prefix)
{
	if (!instancerItems.count()) {
		return VRay::Plugin();
	}

	int instanceIdx = 0;
	int instancesListIdx = 0;

	const int numParticles = instancerItems.count();

	OP_Node *matNode = objNode.getMaterialNode(ctx.getTime());
	VRay::Plugin objMaterial = pluginExporter.exportMaterial(matNode);
	const int objectID = objNode.evalInt(VFH_ATTRIB_OBJECTID, 0, ctx.getTime());

	// +1 because first value is time.
	VRay::VUtils::ValueRefList instances(numParticles+1);
	instances[instancesListIdx++].setDouble(0.0);

	for (int i = 0; i < instancerItems.count(); ++i) {
		const PrimitiveItem &primItem = instancerItems[i];

		ensureDynamicGeometryForInstancer(primItem.geometry);

		QString userAttributes;
		overrideItemsToUserAttributes(primItem.primMaterial.overrides, userAttributes);

		VRay::Plugin material = objMaterial;
		if (primItem.material) {
			material = primItem.material;
		}
		else if (primItem.primMaterial.matNode) {
			OpPluginCache::iterator pIt = pluginCache.op.find(primItem.primMaterial.matNode);
			if (pIt != pluginCache.op.end()) {
				material = pIt.data();
			}
			else {
				material = pluginExporter.exportMaterial(primItem.primMaterial.matNode);
				pluginCache.op.insert(primItem.primMaterial.matNode, material);
			}
		}

		// Instancer works only with Node plugins.
		VRay::Plugin node = getNodeForInstancerGeometry(primItem.geometry, material);

		uint32_t additional_params_flags = useObjectID;
		if (material) {
			additional_params_flags |= useMaterial;
		}
		if (!userAttributes.isEmpty()) {
			additional_params_flags |= useUserAttributes;
		}
		if (primItem.flags & PrimitiveItem::itemFlagsUseTime) {
			// TODO: Utilize use_time_instancing.
		}

		// Index + TM + VEL_TM + AdditionalParams + Node + AdditionalParamsMembers
		const int itemSize = 5 + VUtils::__popcnt(additional_params_flags);

		VRay::VUtils::ValueRefList item(itemSize);
		int indexOffs = 0;
		item[indexOffs++].setDouble(instanceIdx++);
		item[indexOffs++].setTransform(primItem.tm);
		item[indexOffs++].setTransform(VRay::Transform(0));
		item[indexOffs++].setDouble(additional_params_flags);
		item[indexOffs++].setDouble(primItem.objectID != objectIdUndefined ? primItem.objectID : objectID);
		if (additional_params_flags & useUserAttributes) {
			item[indexOffs++].setString(userAttributes.toLocal8Bit().constData());
		}
		if (additional_params_flags & useMaterial) {
			item[indexOffs++].setPlugin(material);
		}
		item[indexOffs++].setPlugin(node);

		instances[instancesListIdx++].setList(item);
	}

	Attrs::PluginDesc instancer2(boost::str(objGeomNameFmt % prefix % gdp.getUniqueId() % objNode.getName().buffer()),
								 "Instancer2");
	instancer2.addAttribute(Attrs::PluginAttr("instances", instances));
	instancer2.addAttribute(Attrs::PluginAttr("use_additional_params", true));
	instancer2.addAttribute(Attrs::PluginAttr("use_time_instancing", false));

	return pluginExporter.exportPlugin(instancer2);
}


VRay::Plugin ObjectExporter::exportDetail(OBJ_Node &objNode, const GU_Detail &gdp)
{
	PrimitiveItems instancerItems;

	const VMRenderPoints renderPoints = getParticlesMode(objNode);
	if (renderPoints != vmRenderPointsNone) {
		VRay::Plugin fromPart = exportPointParticles(objNode, gdp, renderPoints);
		if (fromPart) {
			instancerItems += PrimitiveItem(fromPart);
		}
	}

	if (renderPoints != vmRenderPointsAll) {
		processPrimitives(objNode, gdp, instancerItems);
	}

	return exportDetailInstancer(objNode, gdp, instancerItems, "GeometryInstancer");
}

void ObjectExporter::exportHair(OBJ_Node &objNode, const GU_Detail &gdp, const GEOPrimList &primList, PrimitiveItems &instancerItems)
{
	if (!doExportGeometry) {
		return;
	}

	// NOTE: Try caching hair data by hairPrims.
	HairPrimitiveExporter hairExp(objNode, ctx, pluginExporter, primList);
	Attrs::PluginDesc hairDesc;
	if (hairExp.asPluginDesc(gdp, hairDesc)) {
		VRay::Plugin geometry = pluginExporter.exportPlugin(hairDesc);
		if (geometry) {
			instancerItems += PrimitiveItem(geometry);
		}
	}
}

void ObjectExporter::exportPolyMesh(OBJ_Node &objNode, const GU_Detail &gdp, const GEOPrimList &primList, PrimitiveItems &instancerItems)
{
	if (!doExportGeometry) {
		return;
	}

	// NOTE: Try caching poly data by polyPrims.
	MeshExporter polyMeshExporter(objNode, gdp, ctx, pluginExporter, *this, primList);
	polyMeshExporter.setSubdivApplied(hasSubdivApplied(objNode));
	if (polyMeshExporter.hasPolyGeometry()) {
		Attrs::PluginDesc geomDesc;
		if (polyMeshExporter.asPluginDesc(gdp, geomDesc)) {
			VRay::Plugin geometry = pluginExporter.exportPlugin(geomDesc);
			if (geometry) {
				VRay::Plugin material = polyMeshExporter.getMaterial();
				instancerItems += PrimitiveItem(geometry, material);
			}
		}
	}
}

int ObjectExporter::getPrimPackedID(const GU_PrimPacked &prim)
{
	const GA_PrimitiveTypeId primTypeID = prim.getTypeId();

	if (primTypeID == primPackedTypeIDs.vrayVolumeGridRef) {
		// 0 means don't cache.
		return 0;
	}
	if (primTypeID == primPackedTypeIDs.vrayProxyRef) {
		const VRayProxyRef *vrayProxyRref = UTverify_cast<const VRayProxyRef*>(prim.implementation());
		return vrayProxyRref->getOptions().hash();
	}
	if (primTypeID == primPackedTypeIDs.packedGeometry) {
		int geoID = -1;
		prim.getIntrinsic(prim.findIntrinsic(intrGeometryID), geoID);
		return geoID;
	}
	if (primTypeID == primPackedTypeIDs.alembicRef ||
		primTypeID == primPackedTypeIDs.packedDisk)
	{
		UT_String objname;
		prim.getIntrinsic(prim.findIntrinsic(intrAlembicObjectPath), objname);
		UT_String primname;
		prim.getIntrinsic(prim.findIntrinsic(intrPackedPrimitiveName), primname);
		return primname.hash() ^ objname.hash();
	}

	UT_ASSERT_MSG(false, "Unsupported packed primitive type!");

	return 0;
}

VRay::Plugin ObjectExporter::exportPrimPacked(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	if (!doExportGeometry) {
		return VRay::Plugin();
	}

	const GA_PrimitiveTypeId primTypeID = prim.getTypeId();

	if (primTypeID == primPackedTypeIDs.vrayProxyRef) {
		return exportVRayProxyRef(objNode, prim);
	}
	if (primTypeID == primPackedTypeIDs.packedGeometry) {
		return exportPackedGeometry(objNode, prim);
	}
	if (primTypeID == primPackedTypeIDs.alembicRef) {
		return exportAlembicRef(objNode, prim);
	}
	if (primTypeID == primPackedTypeIDs.packedDisk) {
		return exportPackedDisk(objNode, prim);
	}

	UT_ASSERT_MSG(false, "Unsupported packed primitive type!");

	return VRay::Plugin();
}

VRay::Plugin ObjectExporter::exportAlembicRef(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	if (!doExportGeometry) {
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
	pluginDesc.addAttribute(Attrs::PluginAttr("particle_width_multiplier", 0.05f));

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin ObjectExporter::exportVRayProxyRef(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	if (!doExportGeometry) {
		return VRay::Plugin();
	}

	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic(intrPackedPrimitiveName), primname);

	Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(objNode, primname.buffer()),
								 "GeomMeshFile");

	const VRayProxyRef *vrayproxyref = UTverify_cast<const VRayProxyRef*>(prim.implementation());

	UT_Options options = vrayproxyref->getOptions();
	pluginExporter.setAttrsFromUTOptions(pluginDesc, options);

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin ObjectExporter::exportPackedDisk(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	if (!doExportGeometry) {
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

VRay::Plugin ObjectExporter::exportPackedGeometry(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
	if (!gdl.isValid()) {
		return VRay::Plugin();
	}
	return exportDetail(objNode, *gdl);
}

VMRenderPoints ObjectExporter::getParticlesMode(OBJ_Node &objNode) const
{
	return static_cast<VMRenderPoints>(objNode.evalInt("vm_renderpoints", 0, ctx.getTime()));
}

static GA_ROHandleS getAttribInstancePath(const GU_Detail &gdp)
{
	return GA_ROHandleS(gdp.findAttribute(GA_ATTRIB_POINT, "instancepath"));
}

int ObjectExporter::isInstanceNode(const OP_Node &node)
{
	return node.getOperator()->getName().equal("instance");
}

int ObjectExporter::isPointInstancer(const GU_Detail &gdp)
{
	const GA_ROHandleS instancePathHndl = getAttribInstancePath(gdp);
	return instancePathHndl.isValid();
}

VRay::Plugin ObjectExporter::exportPointParticles(OBJ_Node &objNode, const GU_Detail &gdp, VMRenderPoints pointsMode)
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

/// Holds attributes for calculating instance transform:
///   http://www.sidefx.com/docs/houdini/copy/instanceattrs
struct PointInstanceAttrs {
	explicit PointInstanceAttrs(const GU_Detail &gdp) {
		orient     = gdp.findPointAttribute(GEO_STD_ATTRIB_ORIENT);
		pscale     = gdp.findPointAttribute(GEO_STD_ATTRIB_PSCALE);
		scale      = gdp.findPointAttribute(GA_Names::scale);
		n          = gdp.findPointAttribute(GEO_STD_ATTRIB_NORMAL);
		up         = gdp.findPointAttribute(GEO_STD_ATTRIB_UP);
		v          = gdp.findPointAttribute(GEO_STD_ATTRIB_VELOCITY);
		rot        = gdp.findPointAttribute(GA_Names::rot);
		trans      = gdp.findPointAttribute(GA_Names::trans);
		pivot      = gdp.findPointAttribute(GA_Names::pivot);
		transform3 = gdp.findPointAttribute(VFH_ATTRIB_TRANSFORM);
		transform4 = gdp.findPointAttribute(VFH_ATTRIB_TRANSFORM);
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
		const UT_Vector3F &pivot = attrs.pivot.get(ptOff);
		X.translate(-pivot);
	}

	// O = orient matrix
	UT_Matrix3F O(1);
	if (attrs.orient.isValid()) {
		const UT_QuaternionF &orient = attrs.orient.get(ptOff);
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
	UT_Matrix3F L(1);
	UT_Vector3F up(0.0f, 0.0f, 0.0f);
	static UT_Vector3F a(0.0f, 0.0f, 1.0f);
	if (attrs.up.isValid()) {
		up = attrs.up.get(ptOff);
	}
	UT_Vector3F n(0.0f, 0.0f, 0.0f);
	if (attrs.n.isValid()) {
		n = attrs.n.get(ptOff);
	}
	UT_Vector3F v(0.0f, 0.0f, 0.0f);
	if (attrs.v.isValid()) {
		v = attrs.v.get(ptOff);
	}

	// IF N exists AND up exists and isn't {0,0,0}:
	//    L = mlookatup(N,0,up)
	// ELSE IF N exists:
	//    L = dihedral({0,0,1},N)
	// ELSE IF v exists AND up exists and isn't {0,0,0}:
	//    L = mlookatup(v,0,up)
	// ELSE IF v exists:
	//    L = dihedral({0,0,1},v)
	if (attrs.n.isValid() &&
		attrs.up.isValid() &&
		!up.isEqual(UT_Vector3F(0.0f, 0.0f, 0.0f)))
	{
		L.lookat(n, up, 0);
	}
	else if (attrs.n.isValid()) {
		L.dihedral(a, n);
	}
	else if (attrs.v.isValid() &&
			 attrs.up.isValid() &&
			 !up.isEqual(UT_Vector3F(0.0f, 0.0f, 0.0f)))
	{
		L.lookat(v, up, 0);
	}
	else if (attrs.v.isValid()) {
		L.dihedral(a, v);
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

VRay::Plugin ObjectExporter::exportPointInstancer(OBJ_Node &objNode, const GU_Detail &gdp, int isInstanceNode)
{
	PrimitiveItems instancerItems;

	GA_ROHandleV3 velocityHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_VELOCITY));

	GA_ROHandleS instanceHndl(gdp.findAttribute(GA_ATTRIB_POINT, "instance"));
	GA_ROHandleS instancePathHndl(gdp.findAttribute(GA_ATTRIB_POINT, "instancepath"));

	GA_ROHandleS materialStyleSheetHndl(gdp.findAttribute(GA_ATTRIB_POINT, VFH_ATTR_MATERIAL_STYLESHEET));
	GA_ROHandleS materialOverrideHndl(gdp.findAttribute(GA_ATTRIB_POINT, VFH_ATTR_MATERIAL_OVERRIDE));
	GA_ROHandleS materialPathHndl(gdp.findAttribute(GA_ATTRIB_POINT, GA_Names::shop_materialpath));

	PointInstanceAttrs pointInstanceAttrs(gdp);
	MtlOverrideAttrExporter attrExp(gdp);

	const GA_Size numPoints = gdp.getNumPoints();

	int validPointIdx = 0;
	for (GA_Index i = 0; i < numPoints; ++i) {
		const GA_Offset pointOffset = gdp.pointOffset(i);

		UT_String instanceObjectPath;
		if (instanceHndl.isValid()) {
			instanceObjectPath = instanceHndl.get(pointOffset);
		}
		else if (instancePathHndl.isValid()) {
			instanceObjectPath = instancePathHndl.get(pointOffset);
		}
		else if (isInstanceNode) {
			objNode.evalString(instanceObjectPath, "instancepath", 0, ctx.getTime());
		}
		UT_ASSERT_MSG(instanceObjectPath.length(), "Instance object path is not set!");

		OP_Node *instaceOpNode = getOpNodeFromPath(instanceObjectPath, ctx.getTime());
		UT_ASSERT_MSG(instanceObjectPath, "Instance object is not found!");

		OBJ_Node *instaceObjNode = CAST_OBJNODE(instaceOpNode);
		UT_ASSERT_MSG(instaceObjNode, "Instance object is not an OBJ node!");

		PrimitiveItem item;

		// Particle transform.
		item.tm = getPointInstanceTM(gdp, pointInstanceAttrs, pointOffset);

		// Material overrides.
		mergeMaterialOverride(item.primMaterial, materialStyleSheetHndl, materialPathHndl, materialOverrideHndl, pointOffset, ctx.getTime());
		attrExp.fromPoint(item.primMaterial.overrides, pointOffset);

		// Check parent overrides.
		getPrimMaterial(item.primMaterial);

		// Mult with object inv. tm.
		VRay::Transform objTm = VRayExporter::getObjTransform(instaceObjNode, ctx, false);
		objTm.makeInverse();
		// NOTE: Houdini seems to ignore object offset for point instancing.
		objTm.offset.makeZero();
		item.tm = item.tm * objTm;

		pushContext(PrimContext(&objNode, item.tm, pointOffset, item.primMaterial));
		item.geometry = pluginExporter.exportObject(instaceObjNode);
		popContext();

		const int isLight = !!instaceObjNode->castToOBJLight();
		if (!isLight && item.geometry) {
			instancerItems += item;
		}

		++validPointIdx;
	}

	return exportDetailInstancer(objNode, gdp, instancerItems, "PointInstancer");
}

VRay::Plugin ObjectExporter::exportGeometry(OBJ_Node &objNode, SOP_Node &sopNode)
{
	GU_DetailHandleAutoReadLock gdl(sopNode.getCookedGeoHandle(ctx));
	if (!gdl.isValid()) {
		return VRay::Plugin();
	}

	const GU_Detail &gdp = *gdl;

	const VRay::Transform tm = pluginExporter.getObjTransform(&objNode, ctx);

	pushContext(PrimContext(&objNode, tm, gdp.getUniqueId()));
	
	VRay::Plugin geometry;

	const int isInstance = isInstanceNode(objNode);
	if (isInstance || isPointInstancer(gdp)) {
		geometry = exportPointInstancer(objNode, gdp, isInstance);
	}
	else {
		geometry = exportDetail(objNode, gdp);
	}

	popContext();

	return geometry;
}

VRay::Plugin ObjectExporter::exportGeometry(OBJ_Node &objNode)
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
		return exportVRaySOP(objNode, *renderSOP);
	}

	return exportGeometry(objNode, *renderSOP);
}

int ObjectExporter::isLightEnabled(OBJ_Node &objLight) const
{
	VRayRendererNode &rop = pluginExporter.getRop();

	int enabled = 0;
	objLight.evalParameterOrProperty("enabled", 0, ctx.getTime(), enabled);

	OP_Bundle *bundle = rop.getForcedLightsBundle();
	return bundle && (bundle->contains(&objLight, false) || (enabled > 0));
}

VRay::Plugin ObjectExporter::exportLight(OBJ_Light &objLight)
{
	const fpreal t = ctx.getTime();

	Attrs::PluginDesc pluginDesc;

	PrimMaterial primMaterial;
	getPrimMaterial(primMaterial);

	OP::VRayNode *vrayNode = dynamic_cast<OP::VRayNode*>(&objLight);
	if (vrayNode) {
		pluginDesc.addAttribute(Attrs::PluginAttr("enabled", isLightEnabled(objLight)));

		ExportContext expContext(CT_OBJ, pluginExporter, objLight);
		OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(pluginDesc, pluginExporter, &expContext);

		if (res == OP::VRayNode::PluginResultError) {
			Log::getLog().error("Error creating plugin descripion for node: \"%s\" [%s]",
						objLight.getName().buffer(),
						objLight.getOperator()->getName().buffer());
		}
		else if (res == OP::VRayNode::PluginResultNA ||
				 res == OP::VRayNode::PluginResultContinue)
		{
			pluginExporter.setAttrsFromOpNodePrms(pluginDesc, &objLight);
		}

		VRay::Transform tm = getTm();

		const bool flipTM = vrayNode->getVRayPluginID() == OBJ::getVRayPluginIDName(OBJ::VRayPluginID::LightDome);
		if (flipTM) {
			VUtils::swap(tm.matrix[1], tm.matrix[2]);
		}

		MtlOverrideItems::iterator oIt = primMaterial.overrides.find("Cd");
		if (oIt != primMaterial.overrides.end()) {
			const MtlOverrideItem &cdItem = oIt.data();

			if (cdItem.getType() == MtlOverrideItem::itemTypeVector) {
				pluginDesc.addAttribute(Attrs::PluginAttr("color_tex", cdItem.valueVector[0], cdItem.valueVector[1], cdItem.valueVector[2]));
			}
		}

		pluginDesc.addAttribute(Attrs::PluginAttr("transform", tm));
	}
	else {
		const VRayLightType lightType = static_cast<VRayLightType>(objLight.evalInt("light_type", 0, 0.0));

		Log::getLog().info("  Found light: type = %i",
				   lightType);

		// Point
		if (lightType == VRayLightOmni) {
			pluginDesc.pluginID = "LightOmniMax";
		}
		// Grid
		else if (lightType == VRayLightRectangle) {
			pluginDesc.pluginID = "LightRectangle";

			pluginDesc.addAttribute(Attrs::PluginAttr("u_size", objLight.evalFloat("areasize", 0, t) / 2.0));
			pluginDesc.addAttribute(Attrs::PluginAttr("v_size", objLight.evalFloat("areasize", 1, t) / 2.0));

			pluginDesc.addAttribute(Attrs::PluginAttr("invisible", NOT(objLight.evalInt("light_contribprimary", 0, t))));
		}
		// Sphere
		else if (lightType == VRayLightSphere) {
			pluginDesc.pluginID = "LightSphere";

			pluginDesc.addAttribute(Attrs::PluginAttr("radius", objLight.evalFloat("areasize", 0, t) / 2.0));
		}
		// Distant
		else if (lightType == VRayLightDome) {
			pluginDesc.pluginID = "LightDome";
		}
		// Sun
		else if (lightType == VRayLightSun) {
			pluginDesc.pluginID = "SunLight";
		}

		pluginDesc.addAttribute(Attrs::PluginAttr("intensity", objLight.evalFloat("light_intensity", 0, t)));
		pluginDesc.addAttribute(Attrs::PluginAttr("enabled",   objLight.evalInt("light_enable", 0, t)));

		if (lightType != VRayLightSun) {
			pluginDesc.addAttribute(Attrs::PluginAttr("color", Attrs::PluginAttr::AttrTypeColor,
															   objLight.evalFloat("light_color", 0, t),
															   objLight.evalFloat("light_color", 1, t),
															   objLight.evalFloat("light_color", 2, t)));
		}

		pluginDesc.addAttribute(Attrs::PluginAttr("transform", getTm()));
	}

	pluginDesc.pluginName = boost::str(objGeomNameFmt % "Light" % getDetailID() % objLight.getName().buffer());

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin ObjectExporter::exportNode(OBJ_Node &objNode)
{
	using namespace Attrs;

	OpPluginCache::iterator pIt = pluginCache.op.find(&objNode);
	if (pIt != pluginCache.op.end()) {
		return pIt.data();
	}

	VRay::Plugin geometry = exportGeometry(objNode);

	OP_Node *matNode = objNode.getMaterialNode(ctx.getTime());
	VRay::Plugin material = pluginExporter.exportMaterial(matNode);
	const VRay::Transform tm = pluginExporter.getObjTransform(&objNode, ctx);

	if (isNodeMatte(objNode)) {
		PluginDesc mtlWrapperDesc(VRayExporter::getPluginName(&objNode, "MtlWrapper"),
								  "MtlWrapper");

		mtlWrapperDesc.addAttribute(PluginAttr("base_material", material));
		mtlWrapperDesc.addAttribute(PluginAttr("matte_surface", 1));
		mtlWrapperDesc.addAttribute(PluginAttr("alpha_contribution", -1));
		mtlWrapperDesc.addAttribute(PluginAttr("affect_alpha", 1));
		mtlWrapperDesc.addAttribute(PluginAttr("reflection_amount", 0));
		mtlWrapperDesc.addAttribute(PluginAttr("refraction_amount", 0));

		material = pluginExporter.exportPlugin(mtlWrapperDesc);
	}

	if (isNodePhantom(objNode)) {
		PluginDesc mtlStatsDesc(VRayExporter::getPluginName(&objNode, "MtlRenderStats"),
								"MtlRenderStats");
		mtlStatsDesc.addAttribute(PluginAttr("base_mtl", material));
		mtlStatsDesc.addAttribute(PluginAttr("camera_visibility", 0));

		material = pluginExporter.exportPlugin(mtlStatsDesc);
	}

	PluginDesc nodeDesc(VRayExporter::getPluginName(objNode, "Node"),"Node");
	if (geometry) {
		nodeDesc.add(PluginAttr("geometry", geometry));
	}
	nodeDesc.add(PluginAttr("material", material));
	nodeDesc.add(PluginAttr("transform", tm));
	nodeDesc.add(PluginAttr("visible", isNodeVisible(objNode)));

	VRay::Plugin node = pluginExporter.exportPlugin(nodeDesc);
	UT_ASSERT(node);

	pluginCache.op.insert(&objNode, node);

	return node;
}

void ObjectExporter::addGenerated(OP_Node &key, VRay::Plugin plugin)
{
	if (!plugin) {
		return;
	}

	PluginSet &pluginsSet = pluginCache.generated[&key];
	pluginsSet.insert(plugin);
}

void ObjectExporter::removeGenerated(OP_Node &key)
{
	OpPluginGenCache::iterator cIt = pluginCache.generated.find(&key);
	if (cIt == pluginCache.generated.end()) {
		return;
	}

	PluginSet &pluginsSet = cIt.data();

	FOR_IT (PluginSet, pIt, pluginsSet) {
		pluginExporter.removePlugin(pIt.key());
	}

	pluginCache.generated.erase(cIt);
}

void ObjectExporter::removeObject(OBJ_Node &objNode)
{
	// Remove generated plugin (lights, volumes).
	removeGenerated(objNode);

	// TODO: Remove object plugin itself.
}

VRay::Plugin ObjectExporter::exportObject(OBJ_Node &objNode)
{
	VRay::Plugin plugin;

	OBJ_Light *objLight = objNode.castToOBJLight();
	if (objLight) {
		const VRay::Transform tm = pluginExporter.getObjTransform(&objNode, ctx);

		pushContext(PrimContext(&objNode, tm));

		plugin = exportLight(*objLight);

		OP_Node *objGen = getGenerator();
		if (objGen) {
			addGenerated(*objGen, plugin);
		}

		popContext();
	}
	else {
		ObjectStyleSheet objSheet;
		getObjectStyleSheet(objNode, objSheet, ctx.getTime());

		plugin = exportNode(objNode);
	}

	return plugin;
}
