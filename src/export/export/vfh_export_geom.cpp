//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <QStringBuilder>
#include <QString>

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
#include "rop/vfh_rop.h"
#include "sop/sop_node_base.h"
#include "sop/sop_vrayscene.h"
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

	/// Checks if primitive will be exported as VolumeGrid.
	bool isVolumePrim(const GA_PrimitiveTypeId &primTypeID) const {
		return primTypeID == vrayVolumeGridRef ||
		       primTypeID == GEO_PRIMVOLUME ||
		       primTypeID == GEO_PRIMVDB;
	}

	/// Checks if primitive could be used directly as Instancer2 particle geometry.
	bool isInstancerParticlePrim(const GA_PrimitiveTypeId &primTypeID) const {
		return primTypeID == alembicRef ||
		       primTypeID == vrayProxyRef ||
		       primTypeID == geomPlaneRef ||
		       primTypeID == pgYetiRef;
	}

	/// Checks if primitive will be exported as polygon / hair geometry.
	static bool isGeometryPrim(const GA_PrimitiveTypeId &primTypeID) {
		return primTypeID == GEO_PRIMPOLYSOUP ||
		       primTypeID == GEO_PRIMPOLY ||
		       primTypeID == GEO_PRIMNURBCURVE ||
		       primTypeID == GEO_PRIMBEZCURVE;
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

static const char intrAlembicFilename[] = "abcfilename";
static const char intrAlembicObjectPath[] = "abcobjectpath";
static const char intrAlembicUseTransform[] = "abcusetransform";
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

static const QString attrCd("Cd");
static const QString attrIntensity("intensity");

static const PluginNameMap dummyGeomCache;

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
	typename ContainerType::const_iterator it = container.find(key);
	if (it != container.end()) {
		plugin = it.value();
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
#if 0
	vassert(container.find(key) == container.end());
#endif
	container.insert(key, plugin);
}

InstancerItem::InstancerItem(VRay::Plugin geometry, VRay::Plugin material)
	: prim(nullptr)
	, primID(0)
	, geometry(geometry)
	, material(material)
	, tm(1)
	, vel(1)
	, objectID(objectIdUndefined)
	, timeOffset(0.0)
	, flags(itemFlagsNone)
{}

void PrimContextStack::pushContext(const PrimContext &value)
{
	currentContextInitialized = false;
	primContexts.push(value);
}

PrimContext PrimContextStack::popContext()
{
	currentContextInitialized = false;
	return primContexts.pop();
}

VRay::Transform PrimContextStack::getWorldTm() const
{
	initCurrentContext();
	return currentContext.worldTm;
}

VRay::Transform PrimContextStack::getTm() const
{
	initCurrentContext();
	return currentContext.tm;
}

VRay::Transform PrimContextStack::getVel() const
{
	initCurrentContext();
	return currentContext.vel;
}

exint PrimContextStack::getDetailID() const
{
	initCurrentContext();
	return currentContext.id;
}

const OBJ_Node &PrimContextStack::getGenerator(const OBJ_Node &currentObj) const
{
	if (primContexts.isEmpty())
		return currentObj;
	return *primContexts.first().objNode;
}

const PrimMaterial& PrimContextStack::getPrimMaterial() const
{
	initCurrentContext();
	return currentContext.mat;
}

STY_Styler PrimContextStack::getStyler() const
{
	initCurrentContext();
	return currentContext.styler;
}

const GA_Primitive * PrimContextStack::getPrim() const
{
	initCurrentContext();
	return currentContext.prim;
}

void PrimContextStack::initCurrentContext() const
{
	if (currentContextInitialized)
		return;
	currentContextInitialized = true;

	currentContext = PrimContext();

	if (primContexts.isEmpty())
		return;

	PrimContextIt it(primContexts);
	it.toBack();
	while (it.hasPrevious()) {
		const PrimContext &ctx = it.previous();

		if (ctx.mat.matNode) {
			currentContext.mat.matNode = ctx.mat.matNode;
		}

		currentContext.mat.appendOverrides(ctx.mat.overrides);

		currentContext.id ^= ctx.id;
		currentContext.vel = ctx.vel * currentContext.vel;
		currentContext.tm  = ctx.tm * currentContext.tm;

		currentContext.worldTm = ctx.worldTm * currentContext.worldTm;
	}

	currentContext.worldTm = currentContext.worldTm * currentContext.tm;

	const PrimContext &lastCtx = primContexts.last();

	currentContext.styler = lastCtx.styler;
	currentContext.prim = lastCtx.prim;
}

ObjCacheEntry &OpCacheMan::getCreateObjEntry(const OBJ_Node &objNode)
{
	return objPlugins[&objNode];
}

const ObjCacheEntry &OpCacheMan::getObjEntry(const OBJ_Node &objNode) const
{
	static const ObjCacheEntry fakeEntry;

	const ObjPluginCache::const_iterator it = objPlugins.find(&objNode);
	return it != objPlugins.end() ? it.value() : fakeEntry;
}

ObjLightCacheEntry &OpCacheMan::getCreateLightEntry(const OBJ_Light &objLight)
{
	return objLightPlugins[&objLight];
}

const ObjLightCacheEntry &OpCacheMan::getLightEntry(const OBJ_Light &objLight) const
{
	static const ObjLightCacheEntry fakeEntry;

	const ObjLightPluginsCache::const_iterator it = objLightPlugins.find(&objLight);
	return it != objLightPlugins.end() ? it.value() : fakeEntry;
}

int OpCacheMan::getShaderPlugin(const OP_Node &opNode, VRay::PluginRef &shaderPlugin)
{
	const ShaderPluginCache::const_iterator it = matCache.find(&opNode);
	if (it != matCache.end()) {
		shaderPlugin = it.value();
		return true;
	}
	return false;
}

void OpCacheMan::addShaderPlugin(const OP_Node &opNode, const VRay::PluginRef &shaderPlugin)
{
	vassert(matCache.find(&opNode) == matCache.end());
	matCache[&opNode] = shaderPlugin;
}

void OpCacheMan::clear()
{
	objPlugins.clear();
	objLightPlugins.clear();
	matCache.clear();
}

ObjectExporter::ObjectExporter(VRayExporter &pluginExporter)
	: pluginExporter(pluginExporter)
	, ctx(pluginExporter.getContext())
	, partitionAttribute("")
	, doExportGeometry(true)
	, cacheMan(pluginExporter.getCacheMan())
{}

void ObjectExporter::reset()
{
	clearPrimPluginCache();
	clearOpDepPluginCache();
	clearOpPluginCache();

	cacheMan.clear();
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

	cacheMan.clear();
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
					OP_Node *shopNode = getOpNodeFromPath(objNode, matPath);
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

VRay::Plugin ObjectExporter::getNodeForInstancerGeometry(const InstancerItem &primItem, ObjCacheEntry &cacheEntry)
{
	if (primItem.geometry.isEmpty()) {
		return VRay::Plugin();
	}

	// Already a Node plugin.
	if (vrayPluginTypeNode.equal(primItem.geometry.getType())) {
		return primItem.geometry;
	}

	PluginNameMap::iterator it = cacheEntry.nodes.find(primItem.geometry.getName());
	if (it != cacheEntry.nodes.end()) {
		return it.value();
	}

	const VRay::Plugin objMaterial = pluginExporter.exportDefaultMaterial();

	// Wrap into Node plugin.
	Attrs::PluginDesc nodeDesc(SL("Node@%1").arg(primItem.geometry.getName()),
							   vrayPluginTypeNode.buffer());
	nodeDesc.add(Attrs::PluginAttr(SL("geometry"), primItem.geometry));
	nodeDesc.add(Attrs::PluginAttr(SL("material"), objMaterial));
	nodeDesc.add(Attrs::PluginAttr(SL("objectID"), primItem.objectID != objectIdUndefined ? primItem.objectID : 0));
	nodeDesc.add(Attrs::PluginAttr(SL("transform"), VRay::Transform(1)));
	nodeDesc.add(Attrs::PluginAttr(SL("visible"), false));

	VRay::Plugin node = pluginExporter.exportPlugin(nodeDesc);
	vassert(node.isNotEmpty());

	cacheEntry.nodes.insert(primItem.geometry.getName(), node);

	return node;
}

/// Ensures "dynamic_geometry" is set for GeomStaticMesh.
/// @param geometry Node or geometry plugin.
static void ensureDynamicGeometryForInstancer(VRay::PluginRef geometry)
{
	if (vrayPluginTypeNode.equal(geometry.getType())) {
		geometry = geometry.getPlugin("geometry");
	}
	if (geometry.isNotEmpty() && vrayPluginTypeGeomStaticMesh.equal(geometry.getType())) {
		geometry.setValue("dynamic_geometry", true);
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
	// If key is 0 don't check cache.
	if (key == 0)
		return false;
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

int ObjectExporter::getPluginFromCache(const QString &key, VRay::Plugin &plugin) const
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
	if (key == 0)
		return;
	if (plugin.isEmpty())
		return;
	addPluginToCacheImpl(pluginCache.prim, key, plugin);
}

void ObjectExporter::addPluginToCache(const char *key, VRay::Plugin &plugin)
{
	addPluginToCacheImpl(pluginCache.op, key, plugin);
}

void ObjectExporter::addPluginToCache(const QString &key, VRay::Plugin &plugin)
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
	Attrs::PluginDesc geomSphere(VRayExporter::getPluginName(objNode, SL("GeomSphere")),
								 SL("GeomSphere"));
	geomSphere.add(Attrs::PluginAttr(SL("radius"), 1.0));
	geomSphere.add(Attrs::PluginAttr(SL("subdivs"), 8));

	return pluginExporter.exportPlugin(geomSphere);
}

void ObjectExporter::exportPrimVolume(OBJ_Node &objNode, const GA_Primitive &prim)
{
#ifdef CGR_HAS_AUR
	const GA_PrimitiveTypeId &primTypeID = prim.getTypeId();
	
	// We need to export a separate plugin per volumetric primitive.
#pragma pack(push, 1)
	struct VolumePrimHash {
		VolumePrimHash(exint detailID, exint primOffset)
			: detailID(detailID)
			, primOffset(primOffset)
		{}
		exint detailID;
		exint primOffset;
	} volumePrimHash(getDetailID(), prim.getMapOffset());
#pragma pack(pop)

	exint primID = 0;
	Hash::MurmurHash3_x86_32(&volumePrimHash, sizeof(volumePrimHash), 42, &primID);

	const VRay::Transform &tm = getWorldTm();

	PluginList volumePlugins;

	const PrimMaterial &primMaterial = getPrimMaterial();

	if (primTypeID == primPackedTypeIDs.vrayVolumeGridRef) {
		VolumeExporter volumeGridExp(objNode, ctx, pluginExporter);
		volumeGridExp.setTM(tm);
		volumeGridExp.setDetailID(primID);
		volumeGridExp.exportPrimitive(prim, primMaterial, volumePlugins);
	}
	else if (primTypeID == GEO_PRIMVOLUME ||
			 primTypeID == GEO_PRIMVDB)
	{
		HoudiniVolumeExporter volumeExp(objNode, ctx, pluginExporter);
		volumeExp.setTM(tm);
		volumeExp.setDetailID(primID);
		volumeExp.exportPrimitive(prim, primMaterial, volumePlugins);
	}
	else {
		UT_ASSERT(false && "Unsupported volume primitive type!");
	}

	for (const VRay::Plugin &plugin : volumePlugins) {
		addGenerated(objNode, plugin);
	}

	ObjCacheEntry &objEntry = cacheMan.getCreateObjEntry(objNode);
	mergePluginList(objEntry.volumes, volumePlugins);
#endif
}

void ObjectExporter::processPrimitives(OBJ_Node &objNode, const GU_Detail &gdp, const GA_Range &primRange)
{
	const GA_Size numPoints = gdp.getNumPoints();
	const GA_Size numPrims = gdp.getNumPrimitives();

	int objectID = objectIdUndefined;
	if (Parm::isParmExist(objNode, VFH_ATTRIB_VRAY_OBJECTID)) {
		objectID = objNode.evalInt(VFH_ATTRIB_VRAY_OBJECTID, 0, ctx.getTime());
	}

	const STY_Styler &objStyler = getStylerForObject(getStyler(), pluginExporter.getBundleMap(), objNode);

	PrimMaterial objMaterialOverride;
	appendOverrideValues(objStyler, objMaterialOverride, overrideMerge);

	PrimContext objCtx;
	objCtx.objNode = &objNode;
	objCtx.styler = objStyler;
	objCtx.mat = objMaterialOverride;

	PrimContextAuto objCtxPush(*this, objCtx);

	const GA_ROHandleV3 velocityHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_VELOCITY));
	const GA_ROHandleS materialStyleSheetHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTR_MATERIAL_STYLESHEET));
	const GA_ROHandleS materialOverrideHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTR_MATERIAL_OVERRIDE));
	const GA_ROHandleS materialPathHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, GA_Names::shop_materialpath));
	const GA_ROHandleI objectIdHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTRIB_VRAY_OBJECTID));
	const GA_ROHandleF animOffsetHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTRIB_VRAY_ANIM_OFFSET));
	const GA_ROHandleS pathHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, intrPackedPath));

	MtlOverrideAttrExporter attrExp(gdp);

	const GA_Range &primitiveRange = primRange.isValid() ? primRange : gdp.getPrimitiveRange();

	// Packed, volumes, etc.
	for (GA_Iterator jt(primitiveRange); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive &prim = *gdp.getGEOPrimitive(*jt);

		const GA_Offset primOffset = prim.getMapOffset();
		const GA_Index primIndex = prim.getMapIndex();

		const GA_PrimitiveTypeId &primTypeID = prim.getTypeId();

		if (primPackedTypeIDs.isGeometryPrim(primTypeID))
			continue;

		const bool isPackedPrim = GU_PrimPacked::isPackedPrimitive(prim);
		const bool isVolumePrim = primPackedTypeIDs.isVolumePrim(primTypeID);

		PrimContext primCtx;
		primCtx.prim = &prim;
		primCtx.objNode = &objNode;
		primCtx.styler = getStylerForPrimitive(objStyler, prim);

		if (pathHndl.isValid()) {
			const UT_String path(pathHndl.get(primOffset));
			primCtx.id = path.hash();
		}
		else {
			primCtx.id = primOffset;
		}

		/* Material overrides */ {
			primCtx.mat = objMaterialOverride;

			// Style sheet overrides.
			getOverridesForPrimitive(objStyler, prim, primCtx.mat);

			// Material overrides.
			appendMaterialOverride(primCtx.mat, materialStyleSheetHndl, materialPathHndl, materialOverrideHndl, primOffset, ctx.getTime());

			// Primitive attributes
			attrExp.fromPrimitive(primCtx.mat.overrides, primOffset);
		}

		// Current level transform and velocity.
		if (isPackedPrim) {
			const GU_PrimPacked &primPacked = static_cast<const GU_PrimPacked&>(prim);

			UT_Matrix4D tm4;
			if (isVolumePrim) {
				// XXX: Check why local.
				primPacked.getLocalTransform4(tm4);
			}
			else {
				primPacked.getFullTransform4(tm4);
			}

			primCtx.tm = utMatrixToVRayTransform(tm4);

			// Point attributes for packed instancing.
			if (numPoints == numPrims) {
				const GA_Offset pointOffset = gdp.pointOffset(primIndex);

				attrExp.fromPoint(primCtx.mat.overrides, pointOffset);

				if (ctx.hasMotionBlur && velocityHndl.isValid()) {
					UT_Vector3F v = velocityHndl.get(pointOffset);
					v /= OPgetDirector()->getChannelManager()->getSamplesPerSec();

					primCtx.vel.offset.set(v(0), v(1), v(2));
				}
			}
		}
		else if (primTypeID == GEO_PRIMSPHERE) {
			const GU_PrimSphere &primSphere = static_cast<const GU_PrimSphere&>(prim);

			UT_Matrix4 tm4;
			primSphere.getTransform4(tm4);

			primCtx.tm = utMatrixToVRayTransform(tm4, true);
		}

		PrimContextAuto primCtxPush(*this, primCtx);

		// Primitive geometry.
		// May be NONE if primitive generates non-directly "instanceable" plugin.
		VRay::Plugin geometry;

		if (isVolumePrim) {
			exportPrimVolume(objNode, prim);
		}
		else if (isPackedPrim) {
			const int primKey = getPrimKey(prim);

			if (!getPrimPluginFromCache(primKey, geometry)) {
				const GU_PrimPacked &primPacked = static_cast<const GU_PrimPacked&>(prim);

				geometry = exportPrimPacked(objNode, primPacked);

				if (geometry.isNotEmpty()) {
					OP_Node *matNode = primCtx.mat.matNode
						                   ? primCtx.mat.matNode
						                   : objNode.getMaterialNode(ctx.getTime());

					const SubdivInfo &subdivInfo = getSubdivInfo(objNode, matNode);
					geometry = pluginExporter.exportDisplacement(objNode, geometry, subdivInfo);
				}

				addPrimPluginToCache(primKey, geometry);
			}
		}
		else if (primTypeID == GEO_PRIMSPHERE) {
			const int primKey = getPrimKey(prim);

			if (!getPrimPluginFromCache(primKey, geometry)) {
				geometry = exportPrimSphere(objNode, prim);

				addPrimPluginToCache(primKey, geometry);
			}
		}

		if (geometry.isNotEmpty()) {
			InstancerItem item(geometry);
			item.prim = primCtx.prim;
			item.primMaterial = primCtx.mat;
			item.primID = primCtx.id;
			item.objectID = objectIdHndl.isValid() ? objectIdHndl.get(primOffset) : objectID;
			item.tm = getTm();
			item.vel = getVel();

			if (animOffsetHndl.isValid()) {
				item.timeOffset = animOffsetHndl.get(primOffset);
				item.flags |= InstancerItem::itemFlagsUseTimeOffset;
			}

			instancerItems += item;
		}
	}

	// Mesh data is intanced with Instancer and cached.
	if (exportMode == geoExportModeNonInstancerOnly)
		return;

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
			numStylerHashes += result.size();
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
}

static void appendSeparator(QString &userAttributes)
{
	if (!userAttributes.isEmpty())
		userAttributes.append(';');
}

static void overrideItemsToUserAttributes(const MtlOverrideItems &overrides, QString &userAttributes)
{
	FOR_CONST_IT (MtlOverrideItems, oiIt, overrides) {
		const QString overrideName = oiIt.key();
		const MtlOverrideItem &overrideItem = oiIt.value();

		QString overrideAttr;

		switch (overrideItem.getType()) {
			case MtlOverrideItem::itemTypeInt: {
				overrideAttr = overrideName % SL("=") % QString::number(overrideItem.valueInt);
				break;
			}
			case MtlOverrideItem::itemTypeDouble: {
				overrideAttr = overrideName % SL("=") % QString::number(overrideItem.valueDouble);
				break;
			}
			case MtlOverrideItem::itemTypeVector: {
				overrideAttr = overrideName % SL("=") %
				               QString::number(overrideItem.valueVector.x) % SL(",") %
				               QString::number(overrideItem.valueVector.y) % SL(",") %
				               QString::number(overrideItem.valueVector.z);
				break;
			}
			case MtlOverrideItem::itemTypeString: {
				overrideAttr = overrideName % SL("=") % overrideItem.valueString;
				break;
			}
			default:
				break;
		}

		if (!overrideAttr.isEmpty()) {
			appendSeparator(userAttributes);			

			userAttributes.append(overrideAttr);
		}
	}
}

/// Add object's scene name as user attribute.
/// @param userAttributes User attributes buffer.
/// @param opNode Scene node.
static void appendSceneName(QString &userAttributes, const OP_Node &opNode)
{
	const VRay::VUtils::CharStringRefList &sceneName = VRayExporter::getSceneName(opNode);

	appendSeparator(userAttributes);

	userAttributes.append(vrayUserAttrSceneName);
	userAttributes.append('=');
	userAttributes.append(sceneName[0].ptr());
	userAttributes.append(',');
	userAttributes.append(sceneName[1].ptr());
}

/// Add object's unique ID for IPR drag-drop.
/// @param userAttributes User attributes buffer.
/// @param opNode Scene node.
static void appendObjUniqueID(QString &userAttributes, const OP_Node &opNode)
{
	appendSeparator(userAttributes);

	userAttributes.append(SL("Op_Id="));
	userAttributes.append(QString::number(opNode.getUniqueId()));
}

static void appendPartitionAttribute(QString &userAttributes, const GA_Primitive &prim, const UT_String &partitionAttribute)
{
	const GA_Detail &gdp = prim.getDetail();

	GA_ROHandleS separateAttrHandle(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, partitionAttribute.buffer()));
	if (!separateAttrHandle.isValid())
		return;

	const char *attrValue = separateAttrHandle.get(prim.getMapOffset());
	if (UTisstring(attrValue))
		return;

	appendSeparator(userAttributes);

	userAttributes.append(SL("vrayPrimPartition=%1;").arg(attrValue));
}

static VRay::Plugin exportObjectProperties(VRayExporter &exporter, OBJ_Node &objNode, const VRay::Plugin &baseMaterial)
{
	if (!Parm::getParm(objNode, "MtlWrapper_use"))
		return baseMaterial;

	const int use = objNode.evalInt("MtlWrapper_use", 0, 0.0);
	if (!use)
		return baseMaterial;

	Attrs::PluginDesc mtlWrapper(VRayExporter::getPluginName(objNode, SL("ObjectPropertiesMtlWrapper")),
	                             SL("MtlWrapper"));
	mtlWrapper.add(Attrs::PluginAttr(SL("base_material"), baseMaterial));
	exporter.setAttrsFromOpNodePrms(mtlWrapper, &objNode, SL("MtlWrapper_"));

	return exporter.exportPlugin(mtlWrapper);
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

	// Instancer node items must be stored on the top-level node.
	const OBJ_Node &cacheOwner = getGenerator(objNode);
	ObjCacheEntry &objEntry = cacheMan.getCreateObjEntry(cacheOwner);

	for (const InstancerItem &primItem : instancerItems) {
		ensureDynamicGeometryForInstancer(primItem.geometry);

		QString userAttributes;
		overrideItemsToUserAttributes(primItem.primMaterial.overrides, userAttributes);
		appendSceneName(userAttributes, objNode);
		appendObjUniqueID(userAttributes, objNode);

		if (addParitionAttr && primItem.prim) {
			appendPartitionAttribute(userAttributes, *primItem.prim, partitionAttribute);
		}

		VRay::Plugin material = objMaterial;
		if (primItem.primMaterial.matNode) {
			material = pluginExporter.exportMaterial(primItem.primMaterial.matNode);
		}
		else if (primItem.material.isNotEmpty()) {
			material = primItem.material;
		}

		if (isNodeMatte(objNode)) {
			// NOTE [MacOS]: Do not remove namespace here.
			Attrs::PluginDesc mtlWrapperDesc(VRayExporter::getPluginName(objNode, SL("MtlWrapper")),
											 SL("MtlWrapper"));

			mtlWrapperDesc.add(PluginAttr("base_material", material));
			mtlWrapperDesc.add(PluginAttr("matte_surface", 1));
			mtlWrapperDesc.add(PluginAttr("alpha_contribution", -1));
			mtlWrapperDesc.add(PluginAttr("affect_alpha", 1));
			mtlWrapperDesc.add(PluginAttr("reflection_amount", 0));
			mtlWrapperDesc.add(PluginAttr("refraction_amount", 0));

			material = pluginExporter.exportPlugin(mtlWrapperDesc);
		}

		if (isNodePhantom(objNode)) {
			// NOTE [MacOS]: Do not remove namespace here.
			Attrs::PluginDesc mtlStatsDesc(VRayExporter::getPluginName(objNode, SL("MtlRenderStats")),
										   SL("MtlRenderStats"));

			mtlStatsDesc.add(PluginAttr("base_mtl", material));
			mtlStatsDesc.add(PluginAttr("camera_visibility", 0));

			material = pluginExporter.exportPlugin(mtlStatsDesc);
		}

		material = exportObjectProperties(pluginExporter, objNode, material);

		const VRay::Plugin node = getNodeForInstancerGeometry(primItem, objEntry);

		uint32_t additional_params_flags = 0;
		if (primItem.objectID != objectIdUndefined) {
			additional_params_flags |= VRay::InstancerParamFlags::useObjectID;
		}
		if (material.isNotEmpty()) {
			additional_params_flags |= VRay::InstancerParamFlags::useMaterial;
		}
		if (!userAttributes.isEmpty()) {
			if (!userAttributes.endsWith(';')) {
				userAttributes.append(';');
			}
			additional_params_flags |= VRay::InstancerParamFlags::useUserAttributes;
		}
		static const int useMapChannels = (1 << 7);
		if (primItem.mapChannels.isNotEmpty()) {
			additional_params_flags |= useMapChannels;
		}

		// Index + TM + VEL_TM + UseInstanceTime + Time + AdditionalParams + Node + AdditionalParamsMembers
		const int itemSize = 7 + VUtils::popcnt_u32(additional_params_flags);

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
		item[indexOffs++].setTransform(toVRayVutilsTm(tm));
		item[indexOffs++].setTransform(toVRayVutilsTm(vel));
		item[indexOffs++].setDouble(bool(additional_params_flags & VRay::InstancerParamFlags::useParentTimes));
		item[indexOffs++].setDouble(instancerTime + primItem.timeOffset);
		item[indexOffs++].setDouble(additional_params_flags);
		if (additional_params_flags & VRay::InstancerParamFlags::useObjectID) {
			item[indexOffs++].setDouble(primItem.objectID);
		}
		if (additional_params_flags & VRay::InstancerParamFlags::useUserAttributes) {
			item[indexOffs++].setString(qPrintable(userAttributes));
		}
		if (additional_params_flags & VRay::InstancerParamFlags::useMaterial) {
			item[indexOffs++].setPlugin(material);
		}
		if (additional_params_flags & useMapChannels) {
			item[indexOffs++].setPlugin(primItem.mapChannels);
		}
		item[indexOffs].setPlugin(node);

		instances[instancesListIdx++].setList(item);
	}

	instancerItems.clear();

	Attrs::PluginDesc instancer2(SL("Instancer@") % objNode.getName().buffer(),
	                             SL("Instancer2"));
	instancer2.add(SL("instances"), instances, true);
	instancer2.add(SL("use_additional_params"), true);
	instancer2.add(SL("use_time_instancing"), true);

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

		if (fromPart.isNotEmpty()) {
			instancerItems += InstancerItem(fromPart);
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

#pragma pack(push, 1)
	struct HairPrimKey {
		exint gdpID;
		exint keyDataPoly;
	} hairPrimKey = {
		gdp.getUniqueId(),
		keyDataHair
	};
#pragma pack(pop)
	Hash::MHash hairKey;
	Hash::MurmurHash3_x86_32(&hairPrimKey, sizeof(HairPrimKey), 42, &hairKey);

	InstancerItem item;
	item.primMaterial = getPrimMaterial();
	item.tm = getTm();
	item.vel = getVel();
	item.primID = hairKey;
	if (Parm::isParmExist(objNode, VFH_ATTRIB_VRAY_OBJECTID)) {
		item.objectID = objNode.evalInt(VFH_ATTRIB_VRAY_OBJECTID, 0, ctx.getTime());
	}

	if (doExportGeometry) {
		if (!getMeshPluginFromCache(item.primID, item.geometry)) {
			Attrs::PluginDesc hairDesc(SL("GeomMayaHair|") % QString::number(item.primID) % SL("@") % objNode.getName().buffer(),
									   SL("GeomMayaHair"));
			if (hairExporter.asPluginDesc(gdp, hairDesc)) {
				item.geometry = pluginExporter.exportPlugin(hairDesc);
			}

			addMeshPluginToCache(item.primID, item.geometry);
		}
	}

	if (item.geometry.isNotEmpty()) {
		instancerItems += item;
	}
}

void ObjectExporter::exportPolyMesh(OBJ_Node &objNode, const GU_Detail &gdp, const GEOPrimList &primList, Hash::MHash styleHash)
{
	MeshExporter polyMeshExporter(objNode, gdp, ctx, pluginExporter, *this, primList);
	if (!polyMeshExporter.hasData())
		return;

#pragma pack(push, 1)
	const struct MeshPrimKey {
		exint gdpID;
		exint keyDataPoly;
	} meshPrimKey = {
		gdp.getUniqueId(),
		keyDataPoly
	};
#pragma pack(pop)
	const Hash::MHash meshKey = Hash::hashMur(meshPrimKey);

	InstancerItem item;
	item.primMaterial = getPrimMaterial();
	item.tm = getTm();
	item.vel = getVel();
	item.primID = meshKey;
	if (Parm::isParmExist(objNode, VFH_ATTRIB_VRAY_OBJECTID)) {
		item.objectID = objNode.evalInt(VFH_ATTRIB_VRAY_OBJECTID, 0, ctx.getTime());
	}

	polyMeshExporter.setDetailID(meshKey);

	// This will set/update material/override.
#pragma pack(push, 1)
	const struct MeshOverridesKey {
		Hash::MHash meshKey;
		Hash::MHash styleHash;
	} meshOverridesKey = {
		meshKey, styleHash
	};
#pragma pack(pop)
	const Hash::MHash styleKey = Hash::hashMur(meshOverridesKey);

	if (!getPluginFromCacheImpl(pluginCache.polyMaterial, styleKey, item.material)) {
		item.material = polyMeshExporter.getMaterial();

		addPluginToCacheImpl(pluginCache.polyMaterial, styleKey, item.material);
	}
	if (!getPluginFromCacheImpl(pluginCache.polyMapChannels, styleKey, item.mapChannels)) {
		item.mapChannels = polyMeshExporter.getExtMapChannels();

		addPluginToCacheImpl(pluginCache.polyMapChannels, styleKey, item.mapChannels);
	}

	if (doExportGeometry) {
		const int hasPolySoup = gdp.containsPrimitiveType(GEO_PRIMPOLYSOUP);

		if (hasPolySoup && partitionAttribute.isstring()) {
#pragma pack(push, 1)
			struct MeshPrimSoupKey {
				exint primIndex;
				exint gdpID;
				exint keyDataPoly;
			};
#pragma pack(pop)

			bool allCached = false;

			InstancerItems soupItems;

			const exint keyDetailID = getDetailID();

			UT_Array<const GA_Primitive*> primSoupList;
			gdp.getPrimitivesOfType(GEO_PRIMPOLYSOUP, primSoupList);

			// Check what we have in cache.
			for (const GA_Primitive *prim : primSoupList) {
				const MeshPrimSoupKey meshPrimSoupKey = {
					prim->getMapIndex(), keyDetailID, keyDataPoly
				};

				const Hash::MHash polySoupHash = Hash::hashMur(meshPrimSoupKey);
				VRay::Plugin polySoupGeom;

				if (!getMeshPluginFromCache(polySoupHash, polySoupGeom)) {
					allCached = false;
					break;
				}

				InstancerItem soupItem;
				soupItem.primID = prim->getMapIndex();
				soupItem.prim = prim;
				soupItem.primMaterial = getPrimMaterial();
				soupItem.tm = getTm();
				soupItem.vel = getVel();
				soupItem.geometry = polySoupGeom;

				soupItems += soupItem;
			}

			if (!allCached) {
				// Something changed - re-export primitives.
				soupItems.clear();
				polyMeshExporter.asPolySoupPrimitives(gdp, soupItems, pluginExporter);

				// re-add to cache
				for (const InstancerItem &primSoup : soupItems) {
					const MeshPrimSoupKey key = { primSoup.primID, keyDetailID, keyDataPoly };
					const Hash::MHash meshKeyHash = Hash::hashMur(key);

					addMeshPluginToCache(meshKeyHash, item.geometry);
				}
			}

			instancerItems += soupItems;
		}
		else if (!getMeshPluginFromCache(item.primID, item.geometry)) {
			OP_Node *matNode = item.primMaterial.matNode
				                   ? item.primMaterial.matNode
				                   : objNode.getMaterialNode(ctx.getTime());

			const SubdivInfo &subdivInfo = getSubdivInfo(objNode, matNode);
			polyMeshExporter.setSubdivApplied(subdivInfo.type == SubdivisionType::subdivision);

			Attrs::PluginDesc geomDesc(SL("GeomStaticMesh|") % QString::number(item.primID) % SL("@") % objNode.getName().buffer(),
									   SL("GeomStaticMesh"));
			if (polyMeshExporter.asPluginDesc(gdp, geomDesc)) {
				item.geometry = pluginExporter.exportPlugin(geomDesc);
				if (item.geometry.isNotEmpty()) {
					item.geometry = pluginExporter.exportDisplacement(objNode, item.geometry, subdivInfo);
				}
			}

			addMeshPluginToCache(item.primID, item.geometry);
		}

		if (item.geometry.isNotEmpty()) {
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
	if (primTypeID == primPackedTypeIDs.packedDisk) {
		UT_String path;
		prim.getIntrinsic(prim.findIntrinsic(intrPackedPath), path);
		return path.hash();
	}
	if (primTypeID == primPackedTypeIDs.alembicRef) {
		UT_String objName;
		prim.getIntrinsic(prim.findIntrinsic(intrAlembicObjectPath), objName);

		UT_String fileName;
		prim.getIntrinsic(prim.findIntrinsic(intrAlembicFilename), fileName);

		int isWorldTransform = true;
		prim.getIntrinsic(prim.findIntrinsic(intrAlembicUseTransform), isWorldTransform);

#pragma pack(push, 1)
		const struct AlembicPrimKey {
			uint32 fileName;
			uint32 objName;
			int isWorld;
		} alembicPrimKey = {
			fileName.hash(),
			objName.hash(),
			isWorldTransform
		};
#pragma pack(pop)

		return Hash::hashMur(alembicPrimKey);
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

	if (primTypeID == primPackedTypeIDs.vraySceneRef) {
		exportVRaySceneRef(objNode, prim);
		return VRay::Plugin();
	}
	if (primTypeID == primPackedTypeIDs.packedGeometry ||
		primTypeID == primPackedTypeIDs.packedDisk)
	{
		// This will add plugins to instances table and
		// does not return any plugin.
		exportPackedGeometry(objNode, prim);
		return VRay::Plugin();
	}
	if (primTypeID == GU_PackedFragment::typeId()) {
		exportPackedFragment(objNode, prim);
		return VRay::Plugin();
	}

	// Mesh data is instanced with Instancer and cached.
	if (exportMode == geoExportModeNonInstancerOnly)
		return VRay::Plugin();

	if (primTypeID == primPackedTypeIDs.vrayProxyRef) {
		return exportVRayProxyRef(objNode, prim);
	}
	if (primTypeID == primPackedTypeIDs.alembicRef) {
		return exportAlembicRef(objNode, prim);
	}
	if (primTypeID == primPackedTypeIDs.geomPlaneRef) {
		return exportGeomPlaneRef(objNode, prim);
	}
	if (primTypeID == primPackedTypeIDs.pgYetiRef) {
		return exportPgYetiRef(objNode, prim);
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

	UT_String filename;
	prim.getIntrinsic(prim.findIntrinsic(intrAlembicFilename), filename);

	UT_String objname;
	prim.getIntrinsic(prim.findIntrinsic(intrAlembicObjectPath), objname);

	int isWorldTransform = true;
	prim.getIntrinsic(prim.findIntrinsic(intrAlembicUseTransform), isWorldTransform);

	VRay::VUtils::CharStringRefList visibilityList(1);
	visibilityList[0] = objname;

	const int key = getPrimPackedID(prim);

	Attrs::PluginDesc pluginDesc(SL("Alembic|") % QString::number(key),
								 SL("GeomMeshFile"));

	pluginDesc.add(SL("file"), filename);
	pluginDesc.add(SL("visibility_lists_type"), 1);
	pluginDesc.add(SL("visibility_list_names"), visibilityList);
	pluginDesc.add(SL("particle_width_multiplier"), 0.05f);
	pluginDesc.add(SL("use_alembic_offset"), true);
	pluginDesc.add(SL("use_alembic_transform"), !isWorldTransform);
	pluginDesc.add(SL("use_full_names"), true);

	return pluginExporter.exportPlugin(pluginDesc);
}

static QString vrayProxyObjectTypeToVisilityListName(VRayProxyObjectType objectType)
{
	switch (objectType) {
		case VRayProxyObjectType::geometry:   return SL("visibility");
		case VRayProxyObjectType::hair:       return SL("hair_visibility");
		case VRayProxyObjectType::particles:  return SL("particle_visibility");
		default: {
			vassert(false);
			return SL("unknown");
		}
	}
}

VRay::Plugin ObjectExporter::exportVRayProxyRef(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	if (!doExportGeometry) {
		return VRay::Plugin();
	}

	const int key = getPrimPackedID(prim);

	Attrs::PluginDesc pluginDesc(SL("VRayProxy|") % QString::number(key),
								 SL("GeomMeshFile"));

	const VRayProxyRef &vrayproxyref = static_cast<const VRayProxyRef&>(*prim.implementation());

	// Scale will be exported as primitive transform.
	pluginDesc.add(SL("scale"), 1.0f);

	// Axis flipping is also baked into primitive transform.
	pluginDesc.add(SL("flip_axis"), 0);

	const char *objectPath = vrayproxyref.getObjectPath();
	const VRayProxyObjectType objectType = VRayProxyObjectType(vrayproxyref.getObjectType());
	if (UTisstring(objectPath) && objectType != VRayProxyObjectType::none) {
		const QString visiblity = vrayProxyObjectTypeToVisilityListName(objectType);

		const QString visibilityListType = SL("%1_lists_type").arg(visiblity);
		const QString visibilityListNames = SL("%1_list_names").arg(visiblity);

		VRay::VUtils::CharStringRefList visibilityList(1);
		visibilityList[0] = objectPath;

		pluginDesc.add(visibilityListType, 1);
		pluginDesc.add(visibilityListNames, visibilityList);
	}

	const UT_Options &options = vrayproxyref.getOptions();
	pluginExporter.setAttrsFromUTOptions(pluginDesc, options);

	if (options.hasOption("alembic_layers")) {
		const UT_StringArray &layerFiles = options.getOptionSArray("alembic_layers");
		const int numLayers = layerFiles.size();
		if (numLayers) {
			VRay::VUtils::CharStringRefList alembicLayers(numLayers);
			for (int i = 0; i < numLayers; ++i) {
				alembicLayers[i].set(layerFiles(i).buffer());
			}

			pluginDesc.add(SL("alembic_layers"), alembicLayers);
		}
	}

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin ObjectExporter::exportPgYetiRef(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	if (!doExportGeometry)
		return VRay::Plugin();

	const int key = getPrimPackedID(prim);

	const VRayPgYetiRef *pgYetiRef =
		UTverify_cast<const VRayPgYetiRef*>(prim.implementation());

	const UT_Options &options = pgYetiRef->getOptions();

	Attrs::PluginDesc pluginDesc(SL("VRayPgYeti|") % QString::number(key),
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
	if (!doExportGeometry)
		return VRay::Plugin();

	const VRaySceneRef &vraySceneRef = static_cast<const VRaySceneRef&>(*prim.implementation());

#pragma pack(push, 1)
	const struct VRaySceneKey {
		int optionsHash;
		exint detailID;
		GA_Offset primOffset;
	} vraySceneKey = {
		getPrimPackedID(prim),
		getDetailID(),
		prim.getMapOffset()
	};
#pragma pack(pop)

	const uint32 vraySceneID = Hash::hashLittle(vraySceneKey);

	const SOP::VRaySceneFlipAxisMode flipAxis = SOP::parseFlipAxisMode(vraySceneRef.getFlipAxis());

	Attrs::PluginDesc pluginDesc(QString::asprintf("VRayScene|%X", vraySceneID),
	                             SL("VRayScene"));
	pluginDesc.add(SL("transform"), getWorldTm());
	pluginDesc.add(SL("flip_axis"), static_cast<int>(flipAxis));

	{
		VRay::VUtils::CharStringRefList namesList;

		const VUtils::CharString &objectPath = vraySceneRef.getObjectPath();
		if (!objectPath.empty()) {
			namesList = vraySceneRef.getObjectNamesFromPath();
		}

		if (namesList.count()) {
			pluginDesc.add(Attrs::PluginAttr(SL("hidden_objects"), namesList));
			pluginDesc.add(Attrs::PluginAttr(SL("hidden_objects_inclusive"), false));
		}
	}

	const PrimMaterial &primMaterial = getPrimMaterial();
	if (primMaterial.matNode) {
		const VRay::Plugin &materialOverride = pluginExporter.exportMaterial(primMaterial.matNode);
		if (materialOverride.isNotEmpty()) {
			pluginDesc.add(Attrs::PluginAttr(SL("material_override"), materialOverride));
		}
	}

	if (vraySceneRef.getUseOverrides()) {
		const UT_StringHolder &overrideSnippet = vraySceneRef.getOverrideSnippet();
		const UT_StringHolder &overrideFilePath = vraySceneRef.getOverrideFilepath();

		const int hasOverrideSnippet = overrideSnippet.isstring();
		const int hasOverrideFile = overrideFilePath.isstring();

		const int hasOverrideData = hasOverrideSnippet || hasOverrideFile;

		if (hasOverrideData) {
			// Export plugin mappings.
			const UT_StringHolder &pluginMappings = vraySceneRef.getPluginMapping();
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

						OP_Node *opNode = getOpNodeFromPath(objNode, opPath, ctx.getTime());
						if (opNode) {
							VRay::PluginRef opPlugin;

							if (!cacheMan.getShaderPlugin(*opNode, opPlugin)) {
								// XXX: Move this to method.
								COP2_Node *copNode = opNode->castToCOP2Node();
								if (copNode) {
									opPlugin = pluginExporter.exportCopNodeWithDefaultMapping(*copNode, VRayExporter::defaultMappingTriPlanar);
								}
								else {
									opPlugin = pluginExporter.exportShaderNode(opNode);
								}

								if (opPlugin.isNotEmpty()) {
									opPlugin.setName(pluginName.ptr());

									cacheMan.addShaderPlugin(*opNode, opPlugin);
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

				pluginDesc.add(SL("override_snippet"), snippetText.ptr());
			}
		}
	}

	pluginExporter.setAttrsFromUTOptions(pluginDesc, vraySceneRef.getOptions());

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin ObjectExporter::exportGeomPlaneRef(OBJ_Node &objNode, const GU_PrimPacked &prim)
{
	if (!doExportGeometry) {
		return VRay::Plugin();
	}

	const int key = getPrimPackedID(prim);

	const Attrs::PluginDesc pluginDesc(SL("GeomPlane|") % QString::number(key),
	                                   SL("GeomPlane"));

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

	Attrs::PluginDesc partDesc(VRayExporter::getPluginName(objNode, SL("GeomParticleSystem")),
	                           SL("GeomParticleSystem"));
	partDesc.add(Attrs::PluginAttr("positions", positions));
	partDesc.add(Attrs::PluginAttr("render_type", renderType));
	if (velocities.size()) {
		partDesc.add(Attrs::PluginAttr("velocities", velocities));
	}
	if (color.size()) {
		partDesc.add(Attrs::PluginAttr("colors", color));
	}
	if (opacity.size()) {
		partDesc.add(Attrs::PluginAttr("opacity_pp", opacity));
	}
	if (pscale.size()) {
		partDesc.add(Attrs::PluginAttr("radii", pscale));
		partDesc.add(Attrs::PluginAttr("point_radii", true));
	}
	else {
		partDesc.add(Attrs::PluginAttr("radius", radiusMult));
		partDesc.add(Attrs::PluginAttr("point_size", radiusMult));
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
		L.lookat(n, 0, up);
	}
	else if (attrs.n.isValid()) {
		L.dihedral(a, n);
	}
	else if (attrs.v.isValid() &&
			 attrs.up.isValid() &&
			 !up.isEqual(UT_Vector3F(0.0f, 0.0f, 0.0f)))
	{
		L.lookat(v, 0, up);
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

static void rescaleVelocity(UT_Vector3F &v)
{
	v /= OPgetDirector()->getChannelManager()->getSamplesPerSec();
}

void ObjectExporter::exportPointInstancer(OBJ_Node &objNode, const GU_Detail &gdp, int isInstanceNode)
{
	const fpreal t = ctx.getTime();

	const GA_ROHandleV3 velocityHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_VELOCITY));

	const GA_ROHandleS instanceHndl(gdp.findAttribute(GA_ATTRIB_POINT, "instance"));
	const GA_ROHandleS instancePathHndl(gdp.findAttribute(GA_ATTRIB_POINT, "instancepath"));

	const GA_ROHandleS materialStyleSheetHndl(gdp.findAttribute(GA_ATTRIB_POINT, VFH_ATTR_MATERIAL_STYLESHEET));
	const GA_ROHandleS materialOverrideHndl(gdp.findAttribute(GA_ATTRIB_POINT, VFH_ATTR_MATERIAL_OVERRIDE));
	const GA_ROHandleS materialPathHndl(gdp.findAttribute(GA_ATTRIB_POINT, GA_Names::shop_materialpath));

	const PointInstanceAttrs pointInstanceAttrs(gdp);
	MtlOverrideAttrExporter attrExp(gdp);

	const GA_Size numPoints = gdp.getNumPoints();

	InstancerItems pointInstancerItems;

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
			objNode.evalString(instanceObjectPath, "instancepath", 0, t);
		}

		if (!instanceObjectPath.isstring()) {
			Log::getLog().error("\"%s\": Instance object path is not set!", objNode.getFullPath().buffer());
			continue;
		}

		OP_Node *instaceOpNode = getOpNodeFromPath(objNode, instanceObjectPath, t);
		if (!instaceOpNode) {
			Log::getLog().error("\"%s\": Instance object \"%s\" is not found!",
			                    objNode.getFullPath().buffer(), instanceObjectPath.buffer());
			continue;
		}

		OBJ_Node *instaceObjNode = CAST_OBJNODE(instaceOpNode);
		if (!instaceObjNode) {
			Log::getLog().error("\"%s\": Instance object is not an OBJ node!",
			                    objNode.getFullPath().buffer());
			continue;
		}

#pragma pack(push, 1)
		const struct PointInstanceKey {
			int objID;
			exint instaceObjID;
			GA_Offset pointOffset;
		} pointInstanceKey = {
			objNode.getUniqueId(),
			instaceObjNode->getUniqueId(),
			pointOffset
		};
#pragma pack(pop)

		const uint32 pointInstanceID = Hash::hashLittle(pointInstanceKey);

		const VRay::Transform &pointTm =
			getPointInstanceTM(gdp, pointInstanceAttrs, pointOffset);

		const bool isLight = bool(instaceObjNode->castToOBJLight());
		if (isLight) {
			PrimContext lightCtx;
			lightCtx.objNode = &objNode;
			lightCtx.id = pointInstanceID;
			lightCtx.mat.matNode = instaceObjNode->getMaterialNode(t);
			// Push point transform. Lights are using full transform.
			lightCtx.tm = pointTm;

			// Append point overrides.
			appendMaterialOverride(lightCtx.mat, materialStyleSheetHndl, materialPathHndl, materialOverrideHndl, pointOffset, ctx.getTime());
			attrExp.fromPoint(lightCtx.mat.overrides, pointOffset);

			PrimContextAuto lightCtxPush(*this, lightCtx);
			pluginExporter.exportObject(instaceObjNode);
		}
		else {
			VRay::Transform pointVel(0);
			if (instanceHndl.isValid()) {
				UT_Vector3F vel = velocityHndl.get(pointOffset);
				rescaleVelocity(vel);
				pointVel.offset.set(vel.x(), vel.y(), vel.z());
			}

			PrimContext instanceObjCtx;
			instanceObjCtx.objNode = instaceObjNode;
			instanceObjCtx.id = pointInstanceID;
			instanceObjCtx.mat.matNode = instaceObjNode->getMaterialNode(t);

			// NOTE: Do not push local (point) transform for objects (Node);
			// it'll be baked into InstancerItem, will push "worldTm" here instead.
			instanceObjCtx.worldTm = pointTm;

			const PrimContextAuto instanceObjCtxPush(*this, instanceObjCtx);

			const VRay::Plugin node = pluginExporter.exportObject(instaceObjNode);
			if (node.isNotEmpty()) {
				InstancerItem item;
				item.geometry = node;
				item.primMaterial = getPrimMaterial();
				item.primID = getDetailID();

				// Need to store only local TM here.
				item.tm = pointTm;
				item.vel = pointVel;

				// Append point overrides.
				appendMaterialOverride(item.primMaterial, materialStyleSheetHndl, materialPathHndl, materialOverrideHndl, pointOffset, ctx.getTime());
				attrExp.fromPoint(item.primMaterial.overrides, pointOffset);

				pointInstancerItems += item;
			}
		}

		++validPointIdx;
	}

	instancerItems += pointInstancerItems;
}

void ObjectExporter::exportGeometry(OBJ_Node &objNode, SOP_Node &sopNode)
{
	const GU_DetailHandleAutoReadLock gdl(sopNode.getCookedGeoHandle(ctx));
	if (!gdl.isValid())
		return;

	const GU_Detail &gdp = *gdl;
	const fpreal t = ctx.getTime();

	const STY_Styler &objectStyler = getStylerForObject(objNode, t);
	const STY_Styler &currentPrimStyler = getStyler();
	const STY_Styler &currentStyler = objectStyler.cloneWithAddedStyler(currentPrimStyler, STY_TargetHandle());

	PrimContext objCtx;
	objCtx.id = gdp.getUniqueId();
	objCtx.styler = currentStyler;
	objCtx.mat.matNode = objNode.getMaterialNode(t);
	objCtx.worldTm = VRayExporter::getObjTransform(&objNode, ctx);

	PrimContextAuto rootAutoCtxPush(*this, objCtx);

	const int isInstance = isInstanceNode(objNode);
	if (isInstance || isPointInstancer(gdp)) {
		exportPointInstancer(objNode, gdp, isInstance);
	}
	else {
		exportDetail(objNode, gdp);
	}
}

void ObjectExporter::exportGeometry(OBJ_Node &objNode, InstancerItems &items, SOP_Node *overrideSOP)
{
	SOP_Node *renderSOP = overrideSOP ? overrideSOP : objNode.getRenderSopPtr();
	if (!renderSOP)
		return;

	exportGeometry(objNode, *renderSOP);

	items.swap(instancerItems);
}

VRay::Plugin ObjectExporter::exportGeometry(OBJ_Node &objNode, SOP_Node *overrideSOP)
{
	exportGeometry(objNode, instancerItems, overrideSOP);
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

	const PrimMaterial &primMaterial = getPrimMaterial();

	OP::VRayNode *vrayNode = dynamic_cast<OP::VRayNode*>(&objLight);
	if (vrayNode) {
		pluginDesc.add(Attrs::PluginAttr("enabled", isLightEnabled(objLight)));

		const OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(pluginDesc, pluginExporter);

		if (res == OP::VRayNode::PluginResultError) {
			Log::getLog().error("Error creating plugin descripion for node: \"%s\" [%s]",
						objLight.getName().buffer(),
						objLight.getOperator()->getName().buffer());
		}
		else {
			pluginExporter.setAttrsFromOpNodePrms(pluginDesc, &objLight);

			VRay::Transform tm = getWorldTm();

			const int isDomeLight = vrayNode->getPluginID() == static_cast<int>(VRayPluginID::LightDome);
			if (isDomeLight) {
				tm.makeIdentity();
			}

			const int isIesLight = vrayNode->getPluginID() == static_cast<int>(VRayPluginID::LightIES);
			if (isDomeLight || isIesLight) {
				VUtils::swap(tm.matrix[1], tm.matrix[2]);
			}

			FOR_CONST_IT (MtlOverrideItems, it, primMaterial.overrides) {
				const QString overrideName = it.key();
				const MtlOverrideItem &overrideItem = it.value();

				if (overrideName == attrCd) {
					if (overrideItem.getType() == MtlOverrideItem::itemTypeVector) {
						pluginDesc.add(Attrs::PluginAttr("color_tex",
						                                          overrideItem.valueVector[0],
						                                          overrideItem.valueVector[1],
						                                          overrideItem.valueVector[2]));
					}
				}
				else if (overrideName == attrIntensity) {
					if (overrideItem.getType() == MtlOverrideItem::itemTypeDouble) {
						pluginDesc.add(Attrs::PluginAttr("intensity_tex",
						                                          overrideItem.valueDouble));
					}
				}
			}

			pluginDesc.add(Attrs::PluginAttr("transform", tm));

			if (isDomeLight ||
				vrayNode->getPluginID() == static_cast<int>(VRayPluginID::LightRectangle) ||
				vrayNode->getPluginID() == static_cast<int>(VRayPluginID::LightSphere))
			{
				pluginDesc.add(Attrs::PluginAttr("scene_name", VRayExporter::getSceneName(objLight)));
			}
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

			pluginDesc.add(Attrs::PluginAttr("u_size", objLight.evalFloat("areasize", 0, t) / 2.0));
			pluginDesc.add(Attrs::PluginAttr("v_size", objLight.evalFloat("areasize", 1, t) / 2.0));

			pluginDesc.add(Attrs::PluginAttr("invisible", NOT(objLight.evalInt("light_contribprimary", 0, t))));
		}
		// Sphere
		else if (lightType == VRayLightSphere) {
			pluginDesc.pluginID = "LightSphere";

			pluginDesc.add(Attrs::PluginAttr("radius", objLight.evalFloat("areasize", 0, t) / 2.0));
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
			pluginDesc.add(Attrs::PluginAttr("scene_name", VRayExporter::getSceneName(objLight)));
		}

		pluginDesc.add(Attrs::PluginAttr("intensity", objLight.evalFloat("light_intensity", 0, t)));
		pluginDesc.add(Attrs::PluginAttr("enabled",   objLight.evalInt("light_enable", 0, t)));

		if (lightType != VRayLightSun) {
			pluginDesc.add(SL("color"),
			               objLight.evalFloat("light_color", 0, t),
			               objLight.evalFloat("light_color", 1, t),
			               objLight.evalFloat("light_color", 2, t));
		}

		pluginDesc.add(Attrs::PluginAttr("transform", getWorldTm()));
	}

	pluginDesc.pluginName = SL("Light|") % QString::number(getDetailID()) % SL("@") % objLight.getName().buffer();

	return pluginExporter.exportPlugin(pluginDesc);
}

VRay::Plugin ObjectExporter::exportNode(OBJ_Node &objNode, SOP_Node *overrideSOP)
{
	using namespace Attrs;

	const VRay::Plugin geometry = exportGeometry(objNode, overrideSOP);
	if (exportMode == geoExportModeNonInstancerOnly) {
		// It's ok to return empty plugin in this case.
		return VRay::Plugin();
	}

	// NOTE [MacOS]: Do not remove namespace here.
	Attrs::PluginDesc nodeDesc(VRayExporter::getPluginName(objNode, "Node"),
	                           vrayPluginTypeNode.buffer());

	// May be NULL if geometry was not re-exported during RT sessions.
	if (geometry.isNotEmpty()) {
		nodeDesc.add(PluginAttr("geometry", geometry));
	}
	nodeDesc.add(PluginAttr("material", pluginExporter.exportDefaultMaterial()));
	nodeDesc.add(PluginAttr("transform", VRayExporter::getObjTransform(&objNode, ctx)));
	nodeDesc.add(PluginAttr("visible", isNodeVisible(objNode)));
	nodeDesc.add(PluginAttr("scene_name", VRayExporter::getSceneName(objNode)));

	if (Parm::isParmExist(objNode, VFH_ATTRIB_VRAY_OBJECTID)) {
		const int objectID = objNode.evalInt(VFH_ATTRIB_VRAY_OBJECTID, 0, ctx.getTime());
		nodeDesc.add(PluginAttr("objectID", objectID));
	}

	return pluginExporter.exportPlugin(nodeDesc);
}

void ObjectExporter::addGenerated(OP_Node &opNode, VRay::Plugin plugin)
{
	if (plugin.isEmpty())
		return;

	const UT_StringHolder &key = getKeyFromOpNode(opNode);

	PluginList &pluginsSet = pluginCache.generated[key.buffer()];
	pluginsSet.append(plugin);
}

void ObjectExporter::removeGenerated(const char *key)
{
	OpPluginGenCache::iterator cIt = pluginCache.generated.find(key);
	if (cIt == pluginCache.generated.end()) {
		return;
	}

	PluginList &pluginsSet = cIt.value();

	for (const VRay::Plugin &plugin : pluginsSet) {
		pluginExporter.removePlugin(plugin);
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
	PluginNameMap::iterator pIt = pluginCache.op.find(objNode);
	if (pIt != pluginCache.op.end()) {
		pluginExporter.removePlugin(pIt.value());
		pluginCache.op.erase(pIt);
	}
}

static void appendToLightIlluminationLists(OpCacheMan &cacheMan, OBJ_Node &objNode)
{
	OBJ_Geometry *objGeom = objNode.castToOBJGeometry();
	if (!objGeom || !Parm::isParmExist(objNode, "lightmask"))
		return;

	UT_String lightmask;
	objNode.evalString(lightmask, "lightmask", 0, 0.0);
	if (lightmask.equal("*"))
		return;

	// Get the list of lights that will illuminate this OBJ_Node.
	OP_NodeList lightOpList;
	objGeom->getLightMaskObjects(lightOpList, 0.0);
	if (lightOpList.isEmpty())
		return;

	const ObjCacheEntry &objEntry = cacheMan.getCreateObjEntry(objNode);
	if (objEntry.nodes.isEmpty() &&
		objEntry.volumes.isEmpty())
		return;

	for (OP_Node *lightOp : lightOpList) {
		OBJ_Node *lightObjNode = CAST_OBJNODE(lightOp);
		if (!lightObjNode)
			continue;

		const OBJ_Light *objLight = lightObjNode->castToOBJLight();
		if (!objLight)
			continue;

		ObjLightCacheEntry &lightEntry = cacheMan.getCreateLightEntry(*objLight);
		mergePluginList(lightEntry.includeNodes, objEntry.nodes);
		mergePluginList(lightEntry.includeNodes, objEntry.volumes);
	}
}

VRay::Plugin ObjectExporter::exportObject(OBJ_Node &objNode)
{
	VRay::Plugin plugin;

	if (objNode.castToOBJLight()) {
		OBJ_Light &objLight = *objNode.castToOBJLight();

		PrimContext lightCtx;
		lightCtx.objNode = &objNode;
		lightCtx.worldTm = VRayExporter::getObjTransform(&objNode, ctx);

		const PrimContextAuto lightCtxPush(*this, lightCtx);

		// NOTE: We do not cache light plugins because they are not instancable,
		// so we need to create a new plugin for every instance.
		plugin = exportLight(objLight);

		const OBJ_Node &objCacheOwner = getGenerator(objNode);

		ObjCacheEntry &objEntry = cacheMan.getCreateObjEntry(objCacheOwner);
		objEntry.lights.append(plugin);

		ObjLightCacheEntry &lightEntry = cacheMan.getCreateLightEntry(objLight);
		lightEntry.lights.append(plugin);
	}
	else if (getPluginFromCache(objNode, plugin)) {
		// If plugin is cached we may still need to export instances
		// for non-Instancer2 data types.
		exportMode = geoExportModeNonInstancerOnly;
		exportNode(objNode);
		exportMode = geoExportModeFull;
	}
	else {
		plugin = exportNode(objNode);
		if (plugin.isNotEmpty()) {
			appendToLightIlluminationLists(cacheMan, objNode);

			addPluginToCache(objNode, plugin);
		}
	}

	return plugin;
}
