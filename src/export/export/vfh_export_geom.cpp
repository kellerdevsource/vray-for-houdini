//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
#include "vfh_attr_utils.h"
#include "vfh_exporter.h"
#include "vfh_geoutils.h"
#include "vfh_log.h"
#include "vfh_material_override.h"

#include "gu/gu_vrayproxyref.h"
#include "gu/gu_volumegridref.h"
#include "gu/gu_vraysceneref.h"
#include "gu/gu_geomplaneref.h"
#include "rop/vfh_rop.h"
#include "sop/sop_node_base.h"
#include "vop/vop_node_base.h"
#include "obj/obj_node_base.h"

#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimSphere.h>
#include <GU/GU_PackedFragment.h>
#include <OP/OP_Bundle.h>
#include <GA/GA_Types.h>
#include <GA/GA_Names.h>

#include <parse.h>
#include <gu_pgyeti.h>

using namespace VRayForHoudini;

/// Used for flipping Y and Z axis
static const VRay::Transform flipYZTm{ {
		VRay::Vector(1.f, 0.f,  0.f),
		VRay::Vector(0.f, 0.f, -1.f),
		VRay::Vector(0.f, 1.f,  0.f)
	}, {
		VRay::Vector(0.f, 0.f,  0.f)
	}
};

static struct PrimPackedTypeIDs {
	void init() {
		if (initialized)
			return;

		alembicRef = GU_PrimPacked::lookupTypeId("AlembicRef");
		packedDisk = GU_PrimPacked::lookupTypeId("PackedDisk");
		packedGeometry = GU_PrimPacked::lookupTypeId("PackedGeometry");
		vrayProxyRef = GU_PrimPacked::lookupTypeId("VRayProxyRef");
		vrayVolumeGridRef = GU_PrimPacked::lookupTypeId("VRayVolumeGridRef");
		vraySceneRef = GU_PrimPacked::lookupTypeId("VRaySceneRef");
		geomPlaneRef = GU_PrimPacked::lookupTypeId("GeomPlaneRef");
		pgYetiRef = GU_PrimPacked::lookupTypeId("VRayPgYetiRef");

		initialized = true;
	}

private:
	int initialized{false};

public:
	GA_PrimitiveTypeId alembicRef{0};
	GA_PrimitiveTypeId packedDisk{0};
	GA_PrimitiveTypeId packedGeometry{0};
	GA_PrimitiveTypeId vrayProxyRef{0};
	GA_PrimitiveTypeId vrayVolumeGridRef{0};
	GA_PrimitiveTypeId vraySceneRef{0};
	GA_PrimitiveTypeId geomPlaneRef{0};
	GA_PrimitiveTypeId pgYetiRef{0};
} primPackedTypeIDs;

static boost::format objGeomNameFmt("%s|%i@%s");
static boost::format objInstancerNameFmt("Instancer@%s");
static boost::format hairNameFmt("GeomMayaHair|%i@%s");
static boost::format polyNameFmt("GeomStaticMesh|%i@%s");
static boost::format alembicNameFmt("Alembic|%i@%s");
static boost::format vrmeshNameFmt("VRayProxy|%i");
static boost::format vrsceneNameFmt("VRayScene|%X@%X");
static boost::format pgYetiNameFmt("VRayPgYeti|%i@%s");

static const char intrAlembicFilename[] = "abcfilename";
static const char intrAlembicObjectPath[] = "abcobjectpath";
static const char intrPackedPrimName[] = "packedprimname";
static const char intrPackedPrimitiveName[] = "packedprimitivename";
static const char intrPackedLocalTransform[] = "packedlocaltransform";
static const char intrPackedPath[] = "path";
static const char intrGeometryID[] = "geometryid";
static const char intrFilename[] = "filename";

static const UT_String vrayPluginTypeGeomStaticMesh = "GeomStaticMesh";
static const UT_String vrayPluginTypeGeomPlane = "GeomPlane";
static const UT_String vrayPluginTypeNode = "Node";

static const UT_String vrayUserAttrSceneName = "VRay_Scene_Node_Name";

/// Identity transform.
static VRay::Transform identityTm(1);

/// Zero transform.
static VRay::Transform zeroTm(0);

/// A bit-set for detecting unconnected points.
typedef std::vector<bool> DynamicBitset;

/// Same detail could contain mesh and hair data.
/// We'll XOR getUniqueId() with those values to get a cache key.
static const int keyDataHair = 0xA41857F8;
static const int keyDataPoly = 0xF1625C6B;
static const int keyDataPoints = 0xE163445C;

/// Plugin is always the same.
static const int keyDataSphereID = 100;

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

/// Returns full OP_Node path for using as a hash key.
/// @param opNode OP_Node instance.
static UT_StringHolder getKeyFromOpNode(const OP_Node &opNode)
{
	return opNode.getFullPath();
}

/// Gets plugin from cache.
/// @param container Hash container. NOTE: Non-const because of HashMapKey.
/// @param key Hash key.
/// @param plugin Output plugin.
/// @returns 1 if plugin was found; 0 - otherwise.
template <typename ContainerType, typename KeyType>
static int getPluginFromCacheImpl(ContainerType &container, const KeyType &key, VRay::Plugin &plugin)
{
	typename ContainerType::iterator it = container.find(key);
	if (it != container.end()) {
		plugin = it.data();
		return true;
	}
	return false;
}

/// Adds plugin from cache.
/// @param container Hash container.
/// @param key Hash key.
/// @param plugin Output plugin.
template <typename ContainerType, typename KeyType>
static void addPluginToCacheImpl(ContainerType &container, const KeyType &key, VRay::Plugin &plugin)
{
	vassert(key);
	vassert(container.find(key) == container.end());

	container.insert(key, plugin);
}

ObjectExporter::ObjectExporter(VRayExporter &pluginExporter)
	: pluginExporter(pluginExporter)
	, ctx(pluginExporter.getContext())
	, doExportGeometry(true)
	, partitionAttribute("")
{}

VRay::Transform ObjectExporter::getTm() const
{
	VRay::Transform tm(1);

	PrimContextIt it(primContextStack);
	while (it.hasNext()) {
		tm = it.next().parentItem.tm * tm;
	}

	return tm;
}

VRay::Transform ObjectExporter::getVel() const
{
	VRay::Transform vel(1);

	PrimContextIt it(primContextStack);
	while (it.hasNext()) {
		vel = it.next().parentItem.vel * vel;
	}

	return vel;
}

exint ObjectExporter::getDetailID() const
{
	exint detailID = 0;

	PrimContextIt it(primContextStack);
	while (it.hasNext()) {
		detailID ^= it.next().parentItem.primID;
	}

	return detailID;
}

OP_Node* ObjectExporter::getGenerator() const
{
	if (primContextStack.isEmpty())
		return nullptr;

	return primContextStack[0].objNode;
}

void ObjectExporter::getPrimMaterial(PrimMaterial &primMaterial) const
{
	PrimContextIt it(primContextStack);
	it.toBack();
	while (it.hasPrevious()) {
		const PrimMaterial &primMtl(it.previous().parentItem.primMaterial);
		if (primMtl.matNode) {
			primMaterial.matNode = primMtl.matNode;
		}

		primMaterial.appendOverrides(primMtl.overrides);
	}
}

STY_Styler ObjectExporter::getStyler() const
{
	if (primContextStack.isEmpty())
		return STY_Styler();
	return primContextStack.top().styler;
}

void ObjectExporter::fillFromContext(PrimitiveItem &item) const
{
	item.tm = getTm();
	item.primID = getDetailID();
	getPrimMaterial(item.primMaterial);
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
	pluginCache.meshPrim.clear();
	pluginCache.polyMapChannels.clear();
	pluginCache.polyMaterial.clear();
	pluginCache.hashCache.clear();
	pluginCache.instancerNodesMap.clear();
}

SubdivInfo ObjectExporter::getSubdivInfoFromMatNode(OP_Node &matNode)
{
	if (isOpType(matNode, "VRayNodeGeomDisplacedMesh")) {
		return SubdivInfo(&matNode, SubdivisionType::displacement);
	}
	if (isOpType(matNode, "VRayNodeGeomStaticSmoothedMesh")) {
		return SubdivInfo(&matNode, SubdivisionType::subdivision);
	}
	return SubdivInfo();
}

SubdivInfo ObjectExporter::getSubdivInfoFromVRayMaterialOutput(OP_Node &matNode)
{
	SubdivInfo subdivInfo;

	OP_Node *dispNode = getVRayNodeFromOp(matNode, vfhSocketMaterialOutputSurface);
	if (dispNode) {
		subdivInfo = getSubdivInfoFromMatNode(*dispNode);
	}

	return subdivInfo;
}

static SubdivisionType subdivisionTypeFromMenu(ObjSubdivMenu subdivMenuItem)
{
	switch (subdivMenuItem) {
		case ObjSubdivMenu::displacement: return SubdivisionType::displacement;
		case ObjSubdivMenu::subdivision:  return SubdivisionType::subdivision;
		default:
			return SubdivisionType::none;
	}
}

SubdivInfo ObjectExporter::getSubdivInfo(OBJ_Node &objNode, OP_Node *matNode)
{
	SubdivInfo subdivInfo;

	if (!Parm::isParmExist(objNode, "vray_displ_use")) {
		if (matNode) {
			subdivInfo = getSubdivInfoFromVRayMaterialOutput(*matNode);
		}
	}
	else {
		const ObjSubdivMenu subdivMenuItem = static_cast<ObjSubdivMenu>(objNode.evalInt("vray_displ_type", 0, 0.0));
		switch (subdivMenuItem) {
			case ObjSubdivMenu::fromMat: {
				UT_String matPath;
				objNode.evalString(matPath, "vray_displ_shoppath", 0, 0.0);

				if (!matPath.isstring()) {
					Log::getLog().warning("Material displacement path is not set for \"%s\"",
										  objNode.getFullPath().buffer());
				}
				else {
					OP_Node *shopNode = getOpNodeFromPath(matPath, 0.0);
					if (shopNode) {
						subdivInfo = getSubdivInfoFromVRayMaterialOutput(*shopNode);
					}

					if (!subdivInfo.hasSubdiv()) {
						Log::getLog().warning("Compatible displacement node is not found for \"%s\"",
							                    shopNode->getFullPath().buffer());
					}
				}
				break;
			}
			case ObjSubdivMenu::displacement:
			case ObjSubdivMenu::subdivision: {
				subdivInfo.parmHolder = &objNode;
				subdivInfo.type = subdivisionTypeFromMenu(subdivMenuItem);
				break;
			}
			default:
				break;
		}
	}

	return subdivInfo;
}

int ObjectExporter::isNodeVisible(OP_Node &rop, OBJ_Node &objNode, fpreal t)
{
	OP_Bundle *bundle = getForcedGeometryBundle(rop, t);
	if (!bundle) {
		return objNode.getVisible();
	}
	return bundle->contains(&objNode, false) || objNode.getVisible();
}

int ObjectExporter::isNodeVisible(OBJ_Node &objNode) const
{
	if (pluginExporter.getRopPtr()) {
		return isNodeVisible(*pluginExporter.getRopPtr(), objNode, ctx.getTime());
	}
	// If there is no ROP - there is no option to hide the obj
	return true;
}

int ObjectExporter::isNodeMatte(OBJ_Node &objNode) const
{
	if (!pluginExporter.getRopPtr()) {
		return false;
	}
	OP_Bundle *bundle = getMatteGeometryBundle(*pluginExporter.getRopPtr(), ctx.getTime());
	if (!bundle) {
		return false;
	}
	return bundle->contains(&objNode, false);
}

int ObjectExporter::isNodePhantom(OBJ_Node &objNode) const
{
	if (!pluginExporter.getRopPtr()) {
		return false;
	}
	OP_Bundle *bundle = getPhantomGeometryBundle(*pluginExporter.getRopPtr(), ctx.getTime());
	if (!bundle) {
		return false;
	}
	return bundle->contains(&objNode, false);
}

VRay::Plugin ObjectExporter::getNodeForInstancerGeometry(const PrimitiveItem &primItem, const OBJ_Node &objNode)
{
	if (!primItem.geometry) {
		return VRay::Plugin();
	}

	// Already a Node plugin.
	if (vrayPluginTypeNode.equal(primItem.geometry.getType())) {
		return primItem.geometry;
	}

	GeomNodeCache &cache = pluginCache.instancerNodesMap[&objNode];
	GeomNodeCache::iterator gnIt = cache.find(primItem.geometry.getName());
	if (gnIt != cache.end()) {
		return gnIt.data();
	}

	static boost::format nodeNameFmt("Node@%s");

	const VRay::Plugin objMaterial = pluginExporter.exportDefaultMaterial();

	// Wrap into Node plugin.
	Attrs::PluginDesc nodeDesc(boost::str(nodeNameFmt % primItem.geometry.getName()),
							   vrayPluginTypeNode.buffer());
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", primItem.geometry));
	nodeDesc.addAttribute(Attrs::PluginAttr("material", objMaterial));
	nodeDesc.addAttribute(Attrs::PluginAttr("objectID", primItem.objectID != objectIdUndefined ? primItem.objectID : 0));
	nodeDesc.addAttribute(Attrs::PluginAttr("transform", VRay::Transform(1)));
	nodeDesc.addAttribute(Attrs::PluginAttr("visible", false));

	VRay::Plugin node = pluginExporter.exportPlugin(nodeDesc);
	UT_ASSERT(node);

	cache.insert(primItem.geometry.getName(), node);

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
				userAttributes += buf.sprintf("%s=%.3f;",
											  overrideName,
											  overrideItem.valueDouble);
				break;
			}
			case MtlOverrideItem::itemTypeVector: {
				userAttributes += buf.sprintf("%s=%.3f,%.3f,%.3f;",
											  overrideName,
											  overrideItem.valueVector.x, overrideItem.valueVector.y, overrideItem.valueVector.z);
				break;
			}
			case MtlOverrideItem::itemTypeString: {
				userAttributes += buf.sprintf("%s=%s;",
											  overrideName,
											  _toChar(overrideItem.valueString));
				break;
			}
			default: {
				break;
			}
		}
	}
}

int ObjectExporter::getPrimKey(const GA_Primitive &prim) const
{
	int primKey = 0;

	if (prim.getTypeId() == GEO_PRIMSPHERE) {
		primKey = keyDataSphereID;
	}
	else if (GU_PrimPacked::isPackedPrimitive(prim)) {
		primKey = getPrimPackedID(static_cast<const GU_PrimPacked&>(prim));
	}

	return primKey;
}

int ObjectExporter::getPrimPluginFromCache(int key, VRay::Plugin &plugin) const
{
	return getPluginFromCacheImpl(pluginCache.prim, key, plugin);
}

int ObjectExporter::getMeshPluginFromCache(int key, VRay::Plugin &plugin) const
{
	return getPluginFromCacheImpl(pluginCache.meshPrim, key, plugin);
}

int ObjectExporter::getPluginFromCache(Hash::MHash key, VRay::Plugin &plugin) const
{
	return getPluginFromCacheImpl(pluginCache.hashCache, key, plugin);
}

int ObjectExporter::getPluginFromCache(const char *key, VRay::Plugin &plugin) const
{
	return getPluginFromCacheImpl(pluginCache.op, key, plugin);
}

int ObjectExporter::getPluginFromCache(const OP_Node &opNode, VRay::Plugin &plugin) const
{
	const UT_StringHolder &opKey = getKeyFromOpNode(opNode);
	return getPluginFromCache(opKey.buffer(), plugin);
}

void ObjectExporter::addPluginToCache(Hash::MHash key, VRay::Plugin &plugin)
{
	addPluginToCacheImpl(pluginCache.hashCache, key, plugin);
}

void ObjectExporter::addMeshPluginToCache(int key, VRay::Plugin &plugin)
{
	addPluginToCacheImpl(pluginCache.meshPrim, key, plugin);
}

void ObjectExporter::addPrimPluginToCache(int key, VRay::Plugin &plugin)
{
	addPluginToCacheImpl(pluginCache.prim, key, plugin);
}

void ObjectExporter::addPluginToCache(const char *key, VRay::Plugin &plugin)
{
	addPluginToCacheImpl(pluginCache.op, key, plugin);
}

void ObjectExporter::addPluginToCache(OP_Node &opNode, VRay::Plugin &plugin)
{
	const UT_StringHolder &key = getKeyFromOpNode(opNode);
	addPluginToCache(key, plugin);
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

	const VRay::Transform &tm = VRayExporter::getObjTransform(&objNode, ctx, false) *  item.tm;

	PluginSet volumePlugins;

	if (primTypeID == primPackedTypeIDs.vrayVolumeGridRef) {
		VolumeExporter volumeGridExp(objNode, ctx, pluginExporter);
		volumeGridExp.setTM(tm);
		volumeGridExp.setDetailID(item.primID);
		volumeGridExp.exportPrimitive(item, volumePlugins);
	}
	else if (primTypeID == GEO_PRIMVOLUME ||
			 primTypeID == GEO_PRIMVDB)
	{
		HoudiniVolumeExporter volumeExp(objNode, ctx, pluginExporter);
		volumeExp.setTM(tm);
		volumeExp.setDetailID(item.primID);
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

void ObjectExporter::processPrimitives(OBJ_Node &objNode, const GU_Detail &gdp, const GA_Range &primRange)
{
	const GA_Size numPoints = gdp.getNumPoints();
	const GA_Size numPrims = gdp.getNumPrimitives();

	int objectID = objectIdUndefined;
	if (Parm::isParmExist(objNode, VFH_ATTRIB_OBJECTID)) {
		objectID = objNode.evalInt(VFH_ATTRIB_OBJECTID, 0, ctx.getTime());
	}

	const STY_Styler &objStyler = getStylerForObject(getStyler(), pluginExporter.getBundleMap(), objNode);
	PrimitiveItem objItem;
	objItem.tm = getTm();
	objItem.vel = getVel();
	appendOverrideValues(objStyler, objItem.primMaterial, overrideMerge);
	pushContext(PrimContext(&objNode, objItem, objStyler));

	const GA_ROHandleV3 velocityHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_VELOCITY));
	const GA_ROHandleS materialStyleSheetHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTR_MATERIAL_STYLESHEET));
	const GA_ROHandleS materialOverrideHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTR_MATERIAL_OVERRIDE));
	const GA_ROHandleS materialPathHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, GA_Names::shop_materialpath));
	const GA_ROHandleI objectIdHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTRIB_OBJECTID));
	const GA_ROHandleF animOffsetHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTRIB_ANIM_OFFSET));
	const GA_ROHandleS pathHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, intrPackedPath));

	MtlOverrideAttrExporter attrExp(gdp);

	const GA_Range &primitiveRange = primRange.isValid() ? primRange : gdp.getPrimitiveRange();

	// Packed, volumes, etc.
	for (GA_Iterator jt(primitiveRange); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);
		if (!prim) {
			continue;
		}

		const GA_Offset primOffset = prim->getMapOffset();
		const GA_Index primIndex = prim->getMapIndex();

		const GA_PrimitiveTypeId &primTypeID = prim->getTypeId();

		if (primTypeID == GEO_PRIMPOLYSOUP ||
		    primTypeID == GEO_PRIMPOLY ||
		    primTypeID == GEO_PRIMNURBCURVE ||
		    primTypeID == GEO_PRIMBEZCURVE) {
			continue;
		}

		const bool isPackedPrim = GU_PrimPacked::isPackedPrimitive(*prim);
		const bool isVolumePrim = primTypeID == primPackedTypeIDs.vrayVolumeGridRef ||
								  primTypeID == GEO_PRIMVOLUME ||
								  primTypeID == GEO_PRIMVDB;

		const STY_Styler &primStyler = getStylerForPrimitive(objStyler, *prim);

		PrimitiveItem item(objItem);
		item.prim = prim;
		item.primID = getDetailID() ^ primOffset;
		item.objectID = objectIdHndl.isValid() ? objectIdHndl.get(primOffset) : objectID;

		if (pathHndl.isValid()) {
			const UT_String path(pathHndl.get(primOffset));
			item.primID = path.hash();
		}

		if (animOffsetHndl.isValid()) {
			item.t = animOffsetHndl.get(primOffset);
		}

		// Style sheet overrides.
		getOverridesForPrimitive(objStyler, *prim, item.primMaterial);

		// Primitive attributes
		if (isPackedPrim) {
			const GU_PrimPacked &primPacked = static_cast<const GU_PrimPacked&>(*prim);

			int usePackedTm = true;

			if (primTypeID == primPackedTypeIDs.alembicRef) {
				int abcUseTransform = 0;
				if (prim->getIntrinsic(prim->findIntrinsic("abcusetransform"), abcUseTransform)) {
					usePackedTm = !abcUseTransform;
				}
			}

			if (usePackedTm) {
				// Getting transform here because VRayVolumeGridRef is also a packed primitive
				UT_Matrix4D tm4;
				if (isVolumePrim) {
					primPacked.getLocalTransform4(tm4);
				}
				else {
					primPacked.getFullTransform4(tm4);
				}
				item.tm = item.tm * utMatrixToVRayTransform(tm4);
			}

			// Point attributes for packed instancing.
			if (numPoints == numPrims) {
				const GA_Offset pointOffset = gdp.pointOffset(primIndex);

				attrExp.fromPoint(item.primMaterial.overrides, pointOffset);

				if (ctx.hasMotionBlur && velocityHndl.isValid()) {
					UT_Vector3F v = velocityHndl.get(pointOffset);
					v /= OPgetDirector()->getChannelManager()->getSamplesPerSec();

					VRay::Transform primVel;
					primVel.offset.set(v(0), v(1), v(2));

					item.vel = item.vel * primVel;
				}
			}
		}

		// Material overrides.
		appendMaterialOverride(item.primMaterial, materialStyleSheetHndl, materialPathHndl, materialOverrideHndl, primOffset, ctx.getTime());

		// Primitive attributes
		attrExp.fromPrimitive(item.primMaterial.overrides, primOffset);

		// Check parent overrides.
		getPrimMaterial(item.primMaterial);

		if (isVolumePrim) {
			// We need to export a separate plugin per volumetric primitive.
#pragma pack(push, 1)
			struct VolumePrimID {
				VolumePrimID(exint detailID, exint primOffset)
					: detailID(detailID)
					, primOffset(primOffset)
				{}
				exint detailID;
				exint primOffset;
			} volumePrimID(getDetailID(), primOffset);
#pragma pack(pop)
			Hash::MurmurHash3_x86_32(&volumePrimID, sizeof(volumePrimID), 42, &item.primID);

			pushContext(PrimContext(&objNode, item, primStyler));
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
				pushContext(PrimContext(&objNode, item, primStyler));
				item.geometry = exportPrimPacked(objNode, primPacked);

				if (item.geometry) {
					OP_Node *matNode = item.primMaterial.matNode
						               ? item.primMaterial.matNode
						               : objNode.getMaterialNode(ctx.getTime());

					const SubdivInfo &subdivInfo = getSubdivInfo(objNode, matNode);
					item.geometry = pluginExporter.exportDisplacement(objNode, item.geometry, subdivInfo);
				}

				popContext();
				if (primKey > 0 && item.geometry) {
					addPrimPluginToCache(primKey, item.geometry);
				}
			}
		}
		else if (primTypeID == GEO_PRIMSPHERE) {
			const GU_PrimSphere &primSphere = static_cast<const GU_PrimSphere&>(*prim);
			const int primKey = getPrimKey(primSphere);

			UT_Matrix4 tm4;
			primSphere.getTransform4(tm4);
			item.tm = item.tm * utMatrixToVRayTransform(tm4);

			// If key is 0 don't check cache.
			if (primKey > 0) {
				getPrimPluginFromCache(primKey, item.geometry);
			}
			if (!item.geometry) {
				item.geometry = exportPrimSphere(objNode, *prim);
				if (primKey > 0 && item.geometry) {
					addPrimPluginToCache(primKey, item.geometry);
				}
			}
		}

		if (item.geometry) {
			instancerItems += item;
		}
	}

	// Polygon / hair.
	// NOTE: For polygon and hair material overrides are baked as map channels.
	GEOPrimList polyPrims;
	GEOPrimList hairPrims;

	for (GA_Iterator jt(primitiveRange); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);
		const GA_PrimitiveTypeId &primTypeID = prim->getTypeId();

		if (primTypeID == GEO_PRIMPOLYSOUP) {
			polyPrims.append(prim);
		}
		else if (primTypeID == GEO_PRIMPOLY) {
			const GEO_PrimPoly &primPoly = static_cast<const GEO_PrimPoly&>(*prim);
			if (primPoly.isClosed()) {
				polyPrims.append(prim);
			}
			else {
				hairPrims.append(prim);
			}
		}
		else if (primTypeID == GEO_PRIMNURBCURVE ||
		         primTypeID == GEO_PRIMBEZCURVE)
		{
			hairPrims.append(prim);
		}
	}

	if (polyPrims.size()) {
		Hash::MHash meshStylerHash = 0;

		const GSTY_SubjectPrimGroup gdpStyler(gdp, polyPrims);

		STY_StylerGroup gdpStylers;
		gdpStylers.append(objStyler, gdpStyler);

		const STY_OverrideValuesFilter filter(NULL);
		UT_Array<STY_OverrideValues> results;
#ifdef HDK_16_5
		gdpStylers.getResults(results, filter);
#else
		gdpStylers.getOverrides(results, filter);
#endif

		int numStylerHashes = 0;
		for (const STY_OverrideValues &result : results) {
			for (const auto &res : result) {
				numStylerHashes++;
			}
		}

		if (numStylerHashes) {
			VUtils::IntRefList stylerHashes(numStylerHashes);
			numStylerHashes = 0;
			for (const STY_OverrideValues &result : results) {
				for (const auto &res : result) {
					const STY_OverrideValueMap &resMap = res.second;
					stylerHashes[numStylerHashes++] = resMap.hash();
				}
			}

			Hash::MurmurHash3_x64_128(stylerHashes.get(), numStylerHashes * sizeof(int), 42, &meshStylerHash);
		}

		exportPolyMesh(objNode, gdp, polyPrims, meshStylerHash);
	}

	if (hairPrims.size()) {
		exportHair(objNode, gdp, hairPrims);
	}

	popContext();
}

/// Add object's scene name as user attribute.
/// @param userAttributes User attributes buffer.
/// @param opNode Scene node.
static void appendSceneName(QString &userAttributes, const OP_Node &opNode)
{
	const VRay::VUtils::CharStringRefList &sceneName = VRayExporter::getSceneName(opNode);

	userAttributes.append(vrayUserAttrSceneName);
	userAttributes.append('=');
	userAttributes.append(sceneName[0].ptr());
	userAttributes.append(',');
	userAttributes.append(sceneName[1].ptr());
	userAttributes.append(';');
}

/// Add object's unique ID for IPR drag-drop.
/// @param userAttributes User attributes buffer.
/// @param opNode Scene node.
static void appendObjUniqueID(QString &userAttributes, const OP_Node &opNode)
{
	userAttributes.append("Op_Id=");
	userAttributes.append(QString::number(opNode.getUniqueId()));
	userAttributes.append(';');
}

VRay::Plugin ObjectExporter::exportDetailInstancer(OBJ_Node &objNode)
{
	using namespace Attrs;

	if (!instancerItems.count()) {
		return VRay::Plugin();
	}

	int instanceIdx = 0;
	int instancesListIdx = 0;

	const int numParticles = instancerItems.count();

	OP_Node *matNode = objNode.getMaterialNode(ctx.getTime());
	const VRay::Plugin objMaterial = pluginExporter.exportMaterial(matNode);

	const fpreal instancerTime = ctx.hasMotionBlur ? ctx.mbParams.mb_start : ctx.getFloatFrame();
	const bool addParitionAttr = partitionAttribute.isstring();

	// +1 because first value is time.
	VRay::VUtils::ValueRefList instances(numParticles + 1);
	instances[instancesListIdx++].setDouble(instancerTime);

	for (int i = 0; i < instancerItems.count(); ++i) {
		const PrimitiveItem &primItem = instancerItems[i];

		ensureDynamicGeometryForInstancer(primItem.geometry);

		QString userAttributes;
		overrideItemsToUserAttributes(primItem.primMaterial.overrides, userAttributes);
		appendSceneName(userAttributes, objNode);
		appendObjUniqueID(userAttributes, objNode);

		if (addParitionAttr && primItem.prim) {
			const GA_Detail & gdp = primItem.prim->getDetail();
			GA_ROHandleS separateAttrHandle(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, partitionAttribute.buffer()));
			if (separateAttrHandle.isValid()) {
				const char *attrValue = separateAttrHandle.get(primItem.prim->getMapOffset());
				if (attrValue) {
					userAttributes += QString().sprintf("vrayPrimPartition=%s;", attrValue);
				}
			}
		}

		VRay::Plugin material = objMaterial;
		if (primItem.material) {
			material = primItem.material;
		}
		else if (primItem.primMaterial.matNode) {
			material = pluginExporter.exportMaterial(primItem.primMaterial.matNode);
		}

		if (isNodeMatte(objNode)) {
			// NOTE [MacOS]: Do not remove namespace here.
			Attrs::PluginDesc mtlWrapperDesc(VRayExporter::getPluginName(&objNode, "MtlWrapper"),
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
			// NOTE [MacOS]: Do not remove namespace here.
			Attrs::PluginDesc mtlStatsDesc(VRayExporter::getPluginName(&objNode, "MtlRenderStats"),
										   "MtlRenderStats");
			mtlStatsDesc.addAttribute(PluginAttr("base_mtl", material));
			mtlStatsDesc.addAttribute(PluginAttr("camera_visibility", 0));

			material = pluginExporter.exportPlugin(mtlStatsDesc);
		}

		const VRay::Plugin node = getNodeForInstancerGeometry(primItem, objNode);

		uint32_t additional_params_flags = 0;
		if (primItem.objectID != objectIdUndefined) {
			additional_params_flags |= VRay::InstancerParamFlags::useObjectID;
		}
		if (material) {
			additional_params_flags |= VRay::InstancerParamFlags::useMaterial;
		}
		if (!userAttributes.isEmpty()) {
			additional_params_flags |= VRay::InstancerParamFlags::useUserAttributes;
		}
		if (primItem.flags & PrimitiveItem::itemFlagsUseTime) {
			// TODO: Utilize use_time_instancing.
		}
		static const int useMapChannels = (1 << 7);
		if (primItem.mapChannels) {
			additional_params_flags |= useMapChannels;
		}

		// Index + TM + VEL_TM + AdditionalParams + Node + AdditionalParamsMembers
		const int itemSize = 5 + VUtils::__popcnt(additional_params_flags);

		VRay::Transform vel = primItem.vel;
		vel.matrix.makeZero();

		VRay::Transform tm = primItem.tm;

		const int forceFlipTm =
			vrayPluginTypeGeomPlane.equal(primItem.geometry.getType());
		if (forceFlipTm) {
			tm = flipYZTm * tm;
		}

		tm.offset -= vel.offset;

		VRay::VUtils::ValueRefList item(itemSize);
		int indexOffs = 0;
		item[indexOffs++].setDouble(instanceIdx++);
		item[indexOffs++].setTransform(tm);
		item[indexOffs++].setTransform(vel);
		item[indexOffs++].setDouble(additional_params_flags);
		if (additional_params_flags & VRay::InstancerParamFlags::useObjectID) {
			item[indexOffs++].setDouble(primItem.objectID);
		}
		if (additional_params_flags & VRay::InstancerParamFlags::useUserAttributes) {
			item[indexOffs++].setString(_toChar(userAttributes));
		}
		if (additional_params_flags & VRay::InstancerParamFlags::useMaterial) {
			item[indexOffs++].setPlugin(material);
		}
		if (additional_params_flags & useMapChannels) {
			item[indexOffs++].setPlugin(primItem.mapChannels);
		}
		item[indexOffs++].setPlugin(node);

		instances[instancesListIdx++].setList(item);
	}

	instancerItems.clear();

	Attrs::PluginDesc instancer2(str(objInstancerNameFmt % objNode.getName().buffer()),
								 "Instancer2");

	const int needKeyFrames = pluginExporter.isAnimation() || pluginExporter.needVelocity();

	instancer2.addAttribute(Attrs::PluginAttr("instances", instances, needKeyFrames));
	instancer2.addAttribute(Attrs::PluginAttr("use_additional_params", true));
	instancer2.addAttribute(Attrs::PluginAttr("use_time_instancing", false));

	return pluginExporter.exportPlugin(instancer2);
}


void ObjectExporter::exportDetail(OBJ_Node &objNode, const GU_Detail &gdp, const GA_Range &primRange)
{
	const VMRenderPoints renderPoints = getParticlesMode(objNode);
	if (renderPoints != vmRenderPointsNone) {
#pragma pack(push, 1)
		const struct MeshParticlesKey {
			MeshParticlesKey(exint detailID, exint dataKey)
				: detailID(detailID)
				, dataKey(dataKey)
			{}

			exint detailID;
			exint dataKey;
		} meshParticlesKey(getDetailID(), keyDataPoints);
#pragma pack(pop)

		Hash::MHash meshParticlesHash;
		Hash::MurmurHash3_x86_32(&meshParticlesKey, sizeof(MeshParticlesKey), 42, &meshParticlesHash);

		VRay::Plugin fromPart;
		if (!getMeshPluginFromCache(meshParticlesHash, fromPart)) {
			fromPart = exportPointParticles(objNode, gdp, renderPoints);
			addMeshPluginToCache(meshParticlesHash, fromPart);
		}
		if (fromPart) {
			instancerItems += PrimitiveItem(fromPart);
		}
	}

	if (renderPoints != vmRenderPointsAll) {
		processPrimitives(objNode, gdp, primRange);
	}
}

void ObjectExporter::exportHair(OBJ_Node &objNode, const GU_Detail &gdp, const GEOPrimList &primList)
{
	HairPrimitiveExporter hairExporter(objNode, ctx, pluginExporter, primList);
	if (!hairExporter.hasData())
		return;

	// The top of the stack contains the final tranform.
	const PrimitiveItem topItem(primContextStack.back().parentItem);

#pragma pack(push, 1)
	struct HairPrimKey {
		HairPrimKey(exint parentID, exint detailID, exint keyDataPoly)
			: parentID(parentID)
			, detailID(detailID)
			, keyDataPoly(keyDataPoly)
		{}
		exint parentID;
		exint detailID;
		exint keyDataPoly;
	} hairPrimKey(topItem.primID, getDetailID(), keyDataHair);
#pragma pack(pop)

	Hash::MHash hairKey;
	Hash::MurmurHash3_x86_32(&hairPrimKey, sizeof(HairPrimKey), 42, &hairKey);

	PrimitiveItem item;
	getPrimMaterial(item.primMaterial);
	item.tm = topItem.tm;
	item.vel = topItem.vel;
	item.primID = hairKey;
	if (Parm::isParmExist(objNode, VFH_ATTRIB_OBJECTID)) {
		item.objectID = objNode.evalInt(VFH_ATTRIB_OBJECTID, 0, ctx.getTime());
	}

	if (doExportGeometry) {
		if (!getMeshPluginFromCache(item.primID, item.geometry)) {
			Attrs::PluginDesc hairDesc(boost::str(hairNameFmt % item.primID % objNode.getName().buffer()),
									   "GeomMayaHair");
			if (hairExporter.asPluginDesc(gdp, hairDesc)) {
				item.geometry = pluginExporter.exportPlugin(hairDesc);
			}

			addMeshPluginToCache(item.primID, item.geometry);
		}
	}

	if (item.geometry) {
		instancerItems += item;
	}
}

void ObjectExporter::exportPolyMesh(OBJ_Node &objNode, const GU_Detail &gdp, const GEOPrimList &primList, Hash::MHash styleHash)
{
	MeshExporter polyMeshExporter(objNode, gdp, ctx, pluginExporter, *this, primList);
	if (!polyMeshExporter.hasData())
		return;

	// The top of the stack contains the final tranform.
	const PrimitiveItem topItem(primContextStack.back().parentItem);

#pragma pack(push, 1)
	struct MeshPrimKey {
		MeshPrimKey(exint parentID, exint detailID, exint keyDataPoly)
			: parentID(parentID)
			, detailID(detailID)
			, keyDataPoly(keyDataPoly)
		{}
		exint parentID;
		exint detailID;
		exint keyDataPoly;

		Hash::MHash getHash() const {
			Hash::MHash meshKey;
			Hash::MurmurHash3_x86_32(this, sizeof(MeshPrimKey), 42, &meshKey);
			return meshKey;
		}
	} meshPrimKey(topItem.primID, getDetailID(), keyDataPoly);
#pragma pack(pop)

	const Hash::MHash meshKey = meshPrimKey.getHash();

	PrimitiveItem item;
	getPrimMaterial(item.primMaterial);
	item.tm = topItem.tm;
	item.vel = topItem.vel;
	item.primID = meshKey;
	if (Parm::isParmExist(objNode, VFH_ATTRIB_OBJECTID)) {
		item.objectID = objNode.evalInt(VFH_ATTRIB_OBJECTID, 0, ctx.getTime());
	}

	polyMeshExporter.setDetailID(meshKey);

	// This will set/update material/override.
#pragma pack(push, 1)
	struct MeshOverridesKey {
		MeshOverridesKey(Hash::MHash meshKey, Hash::MHash styleHash)
			: meshKey(meshKey)
			, styleHash(styleHash)
		{}
		Hash::MHash meshKey;
		Hash::MHash styleHash;
	} meshOverridesKey(meshKey, styleHash);
#pragma pack(pop)

	Hash::MHash styleKey;
	Hash::MurmurHash3_x86_32(&meshOverridesKey, sizeof(MeshOverridesKey), 42, &styleKey);

	if (!getPluginFromCacheImpl(pluginCache.polyMaterial, styleKey, item.material)) {
		item.material = polyMeshExporter.getMaterial();

		addPluginToCacheImpl(pluginCache.polyMaterial, styleKey, item.material);
	}
	if (!getPluginFromCacheImpl(pluginCache.polyMapChannels, styleKey, item.mapChannels)) {
		item.mapChannels = polyMeshExporter.getExtMapChannels();

		addPluginToCacheImpl(pluginCache.polyMapChannels, styleKey, item.mapChannels);
	}

	bool hasPolySoup = true;
	for (const auto *prim : primList) {
		hasPolySoup = hasPolySoup && prim->getTypeId() == GEO_PRIMPOLYSOUP;
	}

	if (doExportGeometry) {
		if (hasPolySoup && partitionAttribute.isstring()) {
			bool allCached = false;

			PrimitiveItems soupItems;
			const exint keyDetailID = getDetailID();
			// check what we have in cache
			for (const auto *prim : primList) {
				if (prim->getTypeId() != GEO_PRIMPOLYSOUP) {
					continue;
				}
				MeshPrimKey key(prim->getMapIndex(), keyDetailID, keyDataPoly);
				VRay::Plugin polySoupGeom;

				if (!getMeshPluginFromCache(key.getHash(), polySoupGeom)) {
					allCached = false;
					break;
				}
				PrimitiveItem soupItem;
				soupItem.primID = prim->getMapIndex();
				soupItem.prim = prim;
				soupItem.tm = topItem.tm;
				soupItem.vel = topItem.vel;
				soupItem.material = topItem.material;
				soupItem.geometry = polySoupGeom;

				soupItems += soupItem;
			}

			if (!allCached) {
				// something changed - re-export primitives
				soupItems.freeMem();
				polyMeshExporter.asPolySoupPrimitives(gdp, soupItems, topItem, pluginExporter);

				// re-add to cache
				for (const PrimitiveItem & primSoup : soupItems) {
					MeshPrimKey key(primSoup.primID, keyDetailID, keyDataPoly);
					addMeshPluginToCache(key.getHash(), item.geometry);
				}
			}

			instancerItems += soupItems;
		} else if (!getMeshPluginFromCache(item.primID, item.geometry)) {
			OP_Node *matNode = item.primMaterial.matNode
				? item.primMaterial.matNode
				: objNode.getMaterialNode(ctx.getTime());

			const SubdivInfo &subdivInfo = getSubdivInfo(objNode, matNode);
			polyMeshExporter.setSubdivApplied(subdivInfo.hasSubdiv());

			Attrs::PluginDesc geomDesc(str(polyNameFmt % item.primID % objNode.getName().buffer()),
									   "GeomStaticMesh");
			if (polyMeshExporter.asPluginDesc(gdp, geomDesc)) {
				item.geometry = pluginExporter.exportPlugin(geomDesc);
				if (item.geometry) {
					item.geometry = pluginExporter.exportDisplacement(objNode, item.geometry, subdivInfo);
				}
			}

			addMeshPluginToCache(item.primID, item.geometry);
		}

		if (item.geometry) {
			instancerItems += item;
		}
	}
}

int ObjectExporter::getPrimPackedID(const GU_PrimPacked &prim) const
{
	const GA_PrimitiveTypeId primTypeID = prim.getTypeId();

	if (primTypeID == primPackedTypeIDs.vrayVolumeGridRef) {
		// 0 means don't cache.
		return 0;
	}
	if (primTypeID == primPackedTypeIDs.geomPlaneRef) {
		return 0;
	}
	if (primTypeID == primPackedTypeIDs.vrayProxyRef) {
		const VRayProxyRef *vrayProxyRref = UTverify_cast<const VRayProxyRef*>(prim.implementation());
		return vrayProxyRref->getOptions().hash();
	}
	if (primTypeID == primPackedTypeIDs.vraySceneRef) {
		const VRaySceneRef *vraySceneRef = UTverify_cast<const VRaySceneRef*>(prim.implementation());
		return vraySceneRef->getOptions().hash();
	}
	if (primTypeID == primPackedTypeIDs.packedGeometry) {
		int geoID = -1;
		prim.getIntrinsic(prim.findIntrinsic(intrGeometryID), geoID);
		return geoID;
	}
	if (primTypeID == primPackedTypeIDs.alembicRef ||
		primTypeID == primPackedTypeIDs.packedDisk)
	{
		UT_String objName;
		prim.getIntrinsic(prim.findIntrinsic(intrAlembicObjectPath), objName);
		UT_String fileName;
		prim.getIntrinsic(prim.findIntrinsic(intrAlembicFilename), fileName);
		return objName.hash() ^ fileName.hash();
	}
	if (primTypeID == primPackedTypeIDs.pgYetiRef) {
		const VRayPgYetiRef *pgYetiRef =
			UTverify_cast<const VRayPgYetiRef*>(prim.implementation());
		return pgYetiRef->getOptions().hash();
	}
	if (primTypeID == GU_PackedFragment::typeId()) {
		const GU_PackedFragment *primFragment = UTverify_cast<const GU_PackedFragment*>(prim.implementation());
		if (!primFragment)
			return 0;
		return 0;
	}
	const GA_PrimitiveDefinition &lookupTypeDef = prim.getTypeDef();

	Log::getLog().error("Unsupported packed primitive type: %s [%s]!",
						lookupTypeDef.getLabel().buffer(), lookupTypeDef.getToken().buffer());

	UT_ASSERT_MSG(false, "Unsupported packed primitive type!");

	return 0;
}

VRay::Plugin ObjectExporter::exportPrimPacked(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	if (!doExportGeometry)
		return VRay::Plugin();

	const GA_PrimitiveTypeId primTypeID = prim.getTypeId();

	if (primTypeID == primPackedTypeIDs.vrayProxyRef) {
		return exportVRayProxyRef(objNode, prim);
	}
	if (primTypeID == primPackedTypeIDs.packedGeometry) {
		exportPackedGeometry(objNode, prim);
		// exportPackedGeometry() will add plugins to instances table and
		// does not return any plugin.
		return VRay::Plugin();
	}
	if (primTypeID == GU_PackedFragment::typeId()) {
		exportPackedFragment(objNode, prim);
		return VRay::Plugin();
	}
	if (primTypeID == primPackedTypeIDs.alembicRef) {
		return exportAlembicRef(objNode, prim);
	}
	if (primTypeID == primPackedTypeIDs.packedDisk) {
		return exportPackedDisk(objNode, prim);
	}
	if (primTypeID == primPackedTypeIDs.vraySceneRef) {
		exportVRaySceneRef(objNode, prim);
		return VRay::Plugin();
	}
	if (primTypeID == primPackedTypeIDs.geomPlaneRef) {
		return exportGeomPlaneRef(objNode, prim);
	}
	if (primTypeID == primPackedTypeIDs.pgYetiRef) {
		return exportPgYetiRef(objNode, prim);;
	}

	const GA_PrimitiveDefinition &lookupTypeDef = prim.getTypeDef();

	Log::getLog().error("Unsupported packed primitive type: %s [%s]!",
						lookupTypeDef.getLabel().buffer(), lookupTypeDef.getToken().buffer());

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

	const int key = getPrimPackedID(prim);

	Attrs::PluginDesc pluginDesc(boost::str(alembicNameFmt % key % primname.buffer()),
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

	const int key = getPrimPackedID(prim);

	Attrs::PluginDesc pluginDesc(boost::str(vrmeshNameFmt % key),
								 "GeomMeshFile");

	const VRayProxyRef *vrayproxyref = UTverify_cast<const VRayProxyRef*>(prim.implementation());

	// Scale will be exported as primitive transform.
	pluginDesc.add(Attrs::PluginAttr("scale", 1.0f));

	// Axis flipping is also baked into primitive transform.
	pluginDesc.add(Attrs::PluginAttr("flip_axis", 0));

	const UT_Options &options = vrayproxyref->getOptions();
	pluginExporter.setAttrsFromUTOptions(pluginDesc, options);

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin ObjectExporter::exportPgYetiRef(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	if (!doExportGeometry)
		return VRay::Plugin();

	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic(intrPackedPrimitiveName), primname);

	const int key = getPrimPackedID(prim);

	const VRayPgYetiRef *pgYetiRef =
		UTverify_cast<const VRayPgYetiRef*>(prim.implementation());

	const UT_Options &options = pgYetiRef->getOptions();

	Attrs::PluginDesc pluginDesc(str(pgYetiNameFmt % key % primname.buffer()),
								 "pgYetiVRay");
	pluginDesc.add(Attrs::PluginAttr("file", options.getOptionS("file").buffer()));
	pluginDesc.add(Attrs::PluginAttr("imageSearchPath", options.getOptionS("imageSearchPath").buffer()));
	pluginDesc.add(Attrs::PluginAttr("density", options.getOptionF("density")));
	pluginDesc.add(Attrs::PluginAttr("length", options.getOptionF("length")));
	pluginDesc.add(Attrs::PluginAttr("width", options.getOptionF("width")));
	pluginDesc.add(Attrs::PluginAttr("dynamicHairTesselation", options.getOptionB("dynamicHairTesselation")));
	pluginDesc.add(Attrs::PluginAttr("segmentLength", options.getOptionB("segmentLength")));

	pluginDesc.add(Attrs::PluginAttr("verbosity", 0));
	pluginDesc.add(Attrs::PluginAttr("threads", 0));

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin ObjectExporter::exportVRaySceneRef(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	if (!doExportGeometry) {
		return VRay::Plugin();
	}

	const int key = getPrimPackedID(prim);
	Attrs::PluginDesc pluginDesc(str(vrsceneNameFmt % key % prim.getMapOffset()), "VRayScene");

	const VRaySceneRef *vraysceneref = UTverify_cast<const VRaySceneRef*>(prim.implementation());

	const UT_Options &options = vraysceneref->getOptions();

	VRay::Transform fullTm = pluginExporter.getObjTransform(&objNode, ctx) * getTm();
	const bool shouldFlip = options.getOptionB("should_flip");
	if (shouldFlip) {
		fullTm = flipYZTm * fullTm;
	}
	pluginDesc.add(Attrs::PluginAttr("transform", fullTm));

	if (options.getOptionI("use_overrides")) {
		const UT_StringHolder &overrideSnippet = options.getOptionS("override_snippet");
		const UT_StringHolder &overrideFilePath = options.getOptionS("override_filepath");

		const int hasOverrideSnippet = overrideSnippet.isstring();
		const int hasOverrideFile = overrideFilePath.isstring();

		const int hasOverrideData = hasOverrideSnippet || hasOverrideFile;

		if (hasOverrideData) {
			// Export plugin mappings.
			const UT_StringHolder &pluginMappings = options.getOptionS("plugin_mapping");
			if (pluginMappings.isstring()) {
				VUtils::Table<VUtils::CharString> pluginMappingPairs;
				VUtils::tokenize(pluginMappings.buffer(), ";", pluginMappingPairs);

				for (int i = 0; i < pluginMappingPairs.count(); ++i) {
					const VUtils::CharString pluginMappingPairStr = pluginMappingPairs[i];

					VUtils::Table<VUtils::CharString> pluginMappingPair;
					VUtils::tokenize(pluginMappingPairStr.ptr(), "=", pluginMappingPair);

					if (pluginMappingPair.count() == 2) {
						const UT_String opPath              = pluginMappingPair[0].ptr();
						const VUtils::CharString pluginName = pluginMappingPair[1].ptr();

						OP_Node *opNode = getOpNodeFromPath(opPath, ctx.getTime());
						if (opNode) {
							VRay::Plugin opPlugin;
							if (!getPluginFromCache(*opNode, opPlugin)) {
								// XXX: Move this to method.
								COP2_Node *copNode = opNode->castToCOP2Node();
								VOP_Node *vopNode = opNode->castToVOPNode();
								if (copNode) {
									opPlugin = pluginExporter.exportCopNodeWithDefaultMapping(*copNode, VRayExporter::defaultMappingTriPlanar);
								}
								else if (vopNode) {
									opPlugin = pluginExporter.exportVop(vopNode);
								}

								if (opPlugin) {
									opPlugin.setName(pluginName.ptr());

									addPluginToCache(*opNode, opPlugin);
								}
							}
						}
					}
				}
			}

			if (hasOverrideSnippet) {
				// Fix illegal chars
				VUtils::CharString snippetText(overrideSnippet.buffer());
				vutils_replaceTokenWithValue(snippetText, "\"", "'");

				pluginDesc.add(Attrs::PluginAttr("override_snippet", snippetText.ptr()));
			}
		}
	}

	pluginExporter.setAttrsFromUTOptions(pluginDesc, options);

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin ObjectExporter::exportGeomPlaneRef(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	if (!doExportGeometry) {
		return VRay::Plugin();
	}

	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic(intrPackedPrimitiveName), primname);

	const int key = getPrimPackedID(prim);
	const Attrs::PluginDesc pluginDesc(boost::str(vrsceneNameFmt % key % primname.buffer()), "GeomPlane");

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

void ObjectExporter::exportPackedFragment(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	const GU_PackedFragment *primFragment = UTverify_cast<const GU_PackedFragment*>(prim.implementation());
	if (!primFragment)
		return;

	const GU_DetailHandleAutoReadLock gdl(primFragment->detailPtr());
	if (!gdl.isValid())
		return;

	const GU_Detail &gdp = *gdl.getGdp();
	const GA_Range &primRange = primFragment->getPrimitiveRange();
	if (!primRange.isValid())
		return;

	exportDetail(objNode, gdp, primRange);
}

void ObjectExporter::exportPackedGeometry(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	const GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
	if (!gdl.isValid())
		return;

	exportDetail(objNode, *gdl);
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
			radiusMult *= 0.05;
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
				UT_Vector3F v = velocityHndl.get(ptOff);
				v /= OPgetDirector()->getChannelManager()->getSamplesPerSec();

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

void ObjectExporter::exportPointInstancer(OBJ_Node &objNode, const GU_Detail &gdp, int isInstanceNode)
{
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

		// Use offset as ID.
		item.primID = gdp.getUniqueId() ^ pointOffset;

		// Particle transform.
		item.tm = getPointInstanceTM(gdp, pointInstanceAttrs, pointOffset);

		// Material overrides.
		appendMaterialOverride(item.primMaterial, materialStyleSheetHndl, materialPathHndl, materialOverrideHndl, pointOffset, ctx.getTime());
		attrExp.fromPoint(item.primMaterial.overrides, pointOffset);

		// Check parent overrides.
		getPrimMaterial(item.primMaterial);

		// Mult with object inv. tm.
		VRay::Transform objTm = VRayExporter::getObjTransform(instaceObjNode, ctx, false);
		objTm.makeInverse();
		// NOTE: Houdini seems to ignore object offset for point instancing.
		objTm.offset.makeZero();
		item.tm = item.tm * objTm;

		pushContext(PrimContext(&objNode, item));
		item.geometry = pluginExporter.exportObject(instaceObjNode);
		popContext();

		const int isLight = !!instaceObjNode->castToOBJLight();
		if (!isLight && item.geometry) {
			instancerItems += item;
		}

		++validPointIdx;
	}
}

void ObjectExporter::exportGeometry(OBJ_Node &objNode, SOP_Node &sopNode)
{
	const GU_DetailHandleAutoReadLock gdl(sopNode.getCookedGeoHandle(ctx));
	if (!gdl.isValid()) {
		return;
	}

	const GU_Detail &gdp = *gdl;

	PrimitiveItem rootItem;
	rootItem.primID = gdp.getUniqueId();

	PrimContext primContext(&objNode, rootItem);

	const STY_Styler &currentStyler = getStyler();
	STY_Styler objectStyler = getStylerForObject(objNode, ctx.getTime());
	primContext.styler = objectStyler.cloneWithAddedStyler(currentStyler, STY_TargetHandle());

	pushContext(primContext);

	const int isInstance = isInstanceNode(objNode);
	if (isInstance || isPointInstancer(gdp)) {
		exportPointInstancer(objNode, gdp, isInstance);
	}
	else {
		exportDetail(objNode, gdp);
	}

	popContext();
}

void ObjectExporter::exportGeometry(OBJ_Node &objNode, PrimitiveItems &items)
{
	SOP_Node *renderSOP = objNode.getRenderSopPtr();
	if (!renderSOP)
		return;

	exportGeometry(objNode, *renderSOP);

	items.swap(instancerItems);
}

VRay::Plugin ObjectExporter::exportGeometry(OBJ_Node &objNode, SOP_Node *specificSop)
{
	SOP_Node *renderSOP = specificSop ? specificSop : objNode.getRenderSopPtr();
	if (!renderSOP) {
		return VRay::Plugin();
	}
	exportGeometry(objNode, instancerItems);

	return exportDetailInstancer(objNode);
}

int ObjectExporter::isLightEnabled(OBJ_Node &objLight) const
{
	int enabled = 0;
	objLight.evalParameterOrProperty("enabled", 0, ctx.getTime(), enabled);
	if (!pluginExporter.getRopPtr()) {
		return enabled;
	}
	OP_Bundle *bundle = getForcedLightsBundle(*pluginExporter.getRopPtr(), ctx.getTime());
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
		const OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(pluginDesc, pluginExporter, &expContext);
		const int isDomeLight = vrayNode->getPluginID() == static_cast<int>(VRayPluginID::LightDome);
		const int isIesLight = vrayNode->getPluginID() == static_cast<int>(VRayPluginID::LightIES);

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

		if (isDomeLight) {
			tm.makeIdentity();
		}
		if (isDomeLight || isIesLight) {
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

		if (isDomeLight ||
			vrayNode->getPluginID() == static_cast<int>(VRayPluginID::LightRectangle) ||
			vrayNode->getPluginID() == static_cast<int>(VRayPluginID::LightSphere))
		{
			pluginDesc.addAttribute(Attrs::PluginAttr("scene_name", VRayExporter::getSceneName(objLight)));
		}
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

		if (lightType == VRayLightSphere ||
			lightType == VRayLightRectangle ||
			lightType == VRayLightDome)
		{
			pluginDesc.addAttribute(Attrs::PluginAttr("scene_name", VRayExporter::getSceneName(objLight)));
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

VRay::Plugin ObjectExporter::exportNode(OBJ_Node &objNode, SOP_Node *specificSop)
{
	using namespace Attrs;

	// NOTE [MacOS]: Do not remove namespace here.
	Attrs::PluginDesc nodeDesc(VRayExporter::getPluginName(objNode, "Node"),
	                           vrayPluginTypeNode.buffer());

	const VRay::Plugin geometry = exportGeometry(objNode, specificSop);
	// May be NULL if geometry was not re-exported during RT sessions.
	if (geometry) {
		nodeDesc.add(PluginAttr("geometry", geometry));
	}
	nodeDesc.add(PluginAttr("material", pluginExporter.exportDefaultMaterial()));
	nodeDesc.add(PluginAttr("transform", VRayExporter::getObjTransform(&objNode, ctx)));
	nodeDesc.add(PluginAttr("visible", isNodeVisible(objNode)));
	nodeDesc.add(PluginAttr("scene_name", VRayExporter::getSceneName(objNode)));

	if (Parm::isParmExist(objNode, VFH_ATTRIB_OBJECTID)) {
		const int objectID = objNode.evalInt(VFH_ATTRIB_OBJECTID, 0, ctx.getTime());
		nodeDesc.add(PluginAttr("objectID", objectID));
	}

	return pluginExporter.exportPlugin(nodeDesc);
}

void ObjectExporter::addGenerated(OP_Node &opNode, VRay::Plugin plugin)
{
	if (!plugin) {
		return;
	}

	const UT_StringHolder &key = getKeyFromOpNode(opNode);

	PluginSet &pluginsSet = pluginCache.generated[key.buffer()];
	pluginsSet.insert(plugin);
}

void ObjectExporter::removeGenerated(const char *key)
{
	OpPluginGenCache::iterator cIt = pluginCache.generated.find(key);
	if (cIt == pluginCache.generated.end()) {
		return;
	}

	PluginSet &pluginsSet = cIt.data();

	FOR_IT (PluginSet, pIt, pluginsSet) {
		pluginExporter.removePlugin(pIt.key());
	}

	pluginCache.generated.erase(cIt);
}

void ObjectExporter::removeGenerated(OP_Node &opNode)
{
	const UT_StringHolder &key = getKeyFromOpNode(opNode);

	removeGenerated(key.buffer());
}

void ObjectExporter::removeObject(OBJ_Node &objNode)
{
	const UT_StringHolder &key = getKeyFromOpNode(objNode);

	removeObject(key.buffer());
}

void ObjectExporter::removeObject(const char *objNode)
{
	// Remove generated plugin (lights, volumes).
	removeGenerated(objNode);

	// Remove self.
	OpPluginCache::iterator pIt = pluginCache.op.find(objNode);
	if (pIt != pluginCache.op.end()) {
		pluginExporter.removePlugin(pIt.data());
		pluginCache.op.erase(pIt);
	}
}

VRay::Plugin ObjectExporter::exportObject(OBJ_Node &objNode)
{
	VRay::Plugin plugin;

	OBJ_Light *objLight = objNode.castToOBJLight();
	if (objLight) {
		PrimitiveItem rootItem;
		rootItem.tm = VRayExporter::getObjTransform(&objNode, ctx);

		pushContext(PrimContext(&objNode, rootItem));

		plugin = exportLight(*objLight);

		addPluginToCache(objNode, plugin);

		OP_Node *objGen = getGenerator();
		if (objGen) {
			addGenerated(*objGen, plugin);
		}

		popContext();
	}
	else {
		if (!getPluginFromCache(objNode, plugin)) {
			plugin = exportNode(objNode);

			addPluginToCache(objNode, plugin);
		}
	}

	return plugin;
}

ObjectExporter::GeomNodeCache* ObjectExporter::getExportedNodes(const OBJ_Node &node)
{
	ObjectExporter::NodeMap::iterator it = pluginCache.instancerNodesMap.find(&node);

	return it != pluginCache.instancerNodesMap.end() ? &it.data() : nullptr;
}
