//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORT_GEOM_H
#define VRAY_FOR_HOUDINI_EXPORT_GEOM_H

#include "vfh_vray.h"
#include "vfh_typedefs.h"
#include "vfh_export_primitive.h"
#include "vfh_geoutils.h"

#include <QStack>

#include <GU/GU_PrimPacked.h>
#include <OBJ/OBJ_Light.h>

namespace VRayForHoudini {

/// A type that maps hash with the plugin instance.
typedef QMap<Hash::MHash, VRay::Plugin> HashPluginCache;

/// A type that maps unique primitive ID with the plugin instance.
typedef QMap<int, VRay::Plugin> PrimPluginCache;

/// A type that maps plugin name with plugin instance.
typedef QMap<QString, VRay::Plugin> PluginNameMap;

typedef QMap<QString, PluginList> OpPluginGenCache;

typedef QMap<const OBJ_Node*, PluginNameMap> NodeMap;

typedef QMap<const OBJ_Node*, PluginList> ObjNodePluginList;
typedef QMap<const OBJ_Light*, PluginList> ObjLightPluginList;

enum class ObjSubdivMenu {
	none = -1, ///< No subdivision.
	displacement = 0, ///< Displacement.
	subdivision, ///< Subdivision.
	fromMat, ///< From specified material.
};

/// Subdivision plugin type.
enum class SubdivisionType {
	none = -1, ///< Invalid.
	displacement = 0, ///< GeomDisplacedMesh plugin.
	subdivision, ///< GeomStaticSmoothedMesh plugin.
};

/// Object subdivision export data.
struct SubdivInfo {
	explicit SubdivInfo(OP_Node *parmHolder = nullptr, SubdivisionType type = SubdivisionType::none)
		: parmHolder(parmHolder)
		, type(type)
	{}

	/// Subdivision properties holder (OBJ or VOP node).
	OP_Node *parmHolder{nullptr};

	/// Subdivision plugin type.
	SubdivisionType type{SubdivisionType::none};

	/// Returns true if we need to prefix node parameters with the plugin name
	/// for evaluation.
	bool needParmNamePrefix() const { return CAST_OBJNODE(parmHolder); }

	/// Returns true if the subdivision data is correct.
	bool hasSubdiv() const { return parmHolder && type != SubdivisionType::none; }
};

enum VMRenderPoints {
	vmRenderPointsNone = 0, ///< Don't render points separately from primitives.
	vmRenderPointsAll, ///< Render only points.
	vmRenderPointsUnconnected, ///< Render unconnected points.
};

enum VMRenderPointsAs {
	vmRenderPointsAsSphere = 0, ///< Render points as spheres. Maps to GeomParticleSystem "render_type" 7 (Spheres).
	vmRenderPointsAsCirle, ///< Render points as circles. Maps GeomParticleSystem "render_type" 6 (Points).
};

struct MotionBlurParams {
	MotionBlurParams()
		: mb_geom_samples(1)
		, mb_duration(0.0)
		, mb_interval_center(0.0)
		, mb_start(0.0)
		, mb_end(0.0)
		, mb_frame_inc(0.0)
	{}

	void   calcParams(fpreal currFrame);

	int    mb_geom_samples;

	/// Motion blur duration in frames.
	fpreal mb_duration;

	/// Motion blur interval center in frames.
	fpreal mb_interval_center;

	fpreal mb_start;
	fpreal mb_end;
	fpreal mb_frame_inc;
};

struct VRayOpContext
	: OP_Context
{
	explicit VRayOpContext(const OP_Context &other=OP_Context())
		: OP_Context(other)
		, hasMotionBlur(false)
	{}

	MotionBlurParams mbParams;

	int hasMotionBlur;
};

/// Primitive export context.
struct PrimContext
{
	/// Object generated current primitive.
	OBJ_Node *objNode = nullptr;

	/// Current primitive.
	const GA_Primitive *prim = nullptr;

	/// Local transform.
	VRay::Transform tm{1};

	/// Local velocity.
	VRay::Transform vel{0};

	/// Packed level ID.
	exint id = 0;

	/// Material & material overrides.
	PrimMaterial mat;

	/// Styler.
	STY_Styler styler;
};

/// Primitive export context stack.
struct PrimContextStack
{
	void pushContext(const PrimContext &value);

	PrimContext popContext();

	/// Get final transform for current level.
	VRay::Transform getTm() const;

	/// Get final velocity for current level.
	VRay::Transform getVel() const;

	/// Get final unique ID for current level.
	exint getDetailID() const;

	/// Get root OBJ_Node that is generating plugins.
	/// @param currentObj Currenly processed OBJ_Node.
	const OBJ_Node &getGenerator(const OBJ_Node &currentObj) const;

	/// Get current level material and material overrides.
	const PrimMaterial &getPrimMaterial() const;

	/// Get current level styler.
	STY_Styler getStyler() const;

	/// Get current primitive.
	const GA_Primitive *getPrim() const;

private:
	/// A type for primitive export context stack iterator.
	typedef QVectorIterator<PrimContext> PrimContextIt;

	/// A type for primitive export context stack.
	typedef QStack<PrimContext> PrimContexts;

	/// Initialize current context.
	void initCurrentContext() const;

	/// Flag indicating that we need to re-initialize @a currentContext.
	mutable int currentContextInitialized = false;

	/// Current level context.
	mutable PrimContext currentContext;

	/// Stack of primitive export levels.
	PrimContexts primContexts;
};

/// Instancer particle.
/// Describes an instancable peace of geometry generated by OBJ_Node.
struct InstancerItem {
	enum InstancerItemFlags {
		itemFlagsNone = 0,
		itemFlagsUseTimeOffset = 1 << 0,
	};

	/// Construct item from geometry and material.
	/// @param geometry Geometry.
	/// @param material Material.
	explicit InstancerItem(VRay::Plugin geometry = VRay::Plugin(), VRay::Plugin material = VRay::Plugin());

	/// Primitive.
	const GA_Primitive *prim;

	/// Primitive ID.
	exint primID;

	/// Material.
	PrimMaterial primMaterial;

	/// Exported geometry plugin.
	VRay::Plugin geometry;

	/// Exported material plugin.
	VRay::Plugin material;

	/// Map channel overrides.
	VRay::Plugin mapChannels;

	/// Transform.
	VRay::Transform tm;

	/// Velocity.
	VRay::Transform vel;

	/// Object ID.
	int objectID;

	/// Time instancing.
	fpreal timeOffset;

	/// Flags.
	uint32_t flags;
};

/// A type for collection of InstancerItem.
typedef QList<InstancerItem> InstancerItems;


/// Collect plugin lists the OBJ_Node has generated.
/// Used by:
///  * LightLinker
///  * IRP plugin removal
struct ObjCacheEntry
{
	/// A list of lights this node has generated.
	PluginList lights;

	/// A list of volume grid plugins this OBJ_Node is generating.
	PluginList volumes;

	/// A list of node plugins used to wrap raw
	/// geometry plugins the OBJ_Node is instancing.
	PluginNameMap nodes;
};

/// A type mapping OBJ_Node with the data this node is generating.
typedef QMap<const OBJ_Node*, ObjCacheEntry> ObjPluginCache;

/// Collect plugin lists the OBJ_Light has generated.
/// Used by:
///  * LightLinker
struct ObjLightCacheEntry
{
	/// A list of lights this light has generated.
	/// It's a list because OBJ_Light could be instanced with "instancer" node. 
	PluginList lights;

	/// A list of node plugins this light illuminates.
	PluginList includeNodes;
};

/// A type mapping OBJ_Light with the data this node is generating.
typedef QMap<const OBJ_Light*, ObjLightCacheEntry> ObjLightPluginsCache;

struct OpCacheMan
{
	/// Get writable object entry.
	/// @param objNode OBJ_Node instance.
	ObjCacheEntry &getObjEntry(const OBJ_Node &objNode);

	/// Get object entry.
	/// @param objNode OBJ_Node instance.
	const ObjCacheEntry &getObjEntry(const OBJ_Node &objNode) const;

	/// Get writable light entry.
	/// @param objLight OBJ_Light instance.
	ObjLightCacheEntry &getObjLightEntry(const OBJ_Light &objLight);

	/// Get light cache entry.
	/// @param objLight OBJ_Light instance.
	const ObjLightCacheEntry &getObjLightEntry(const OBJ_Light &objLight) const;

	/// Get lights cache list.
	const ObjLightPluginsCache &getLightPlugins() const { return objLightPlugins; }

	/// Get the number of processed OBJ_Light nodes.
	int numLights() const { return objLightPlugins.size(); }

	/// Get material plugin from cache.
	/// @param opNode VOP or SHOP node.
	/// @param[out] matPlugin Material plugin.
	/// @returns True if plugin was found in cache, false - otherwise.
	int getMatPlugin(const OP_Node &opNode, VRay::Plugin &matPlugin);

	/// Add material plugin to cache.
	/// @param opNode VOP or SHOP node.
	/// @param matPlugin Material plugin.
	void addMatPlugin(const OP_Node &opNode, const VRay::Plugin &matPlugin);

	/// Clear cached plugins. This won't delete plugin instances.
	void clear();

private:
	/// Object centric plugins cache.
	ObjPluginCache objPlugins;

	/// Light object centric plugins cache; for faster export of light linker.
	ObjLightPluginsCache objLightPlugins;

	/// A type mapping material node with the exported material plugin.
	/// @tparam key VOP or SHOP node.
	/// @tparam value VRay::Plugin instance.
	typedef QMap<const OP_Node*, VRay::Plugin> MatPluginCache;

	/// Shading node centric plugins cache.
	MatPluginCache matCache;
};

template <typename ListType>
static void mergePluginList(PluginList &dstList, const ListType &srcList)
{
	for (const VRay::Plugin &plugin : srcList) {
		if (plugin.isEmpty())
			continue;

		dstList.append(plugin);
	}
}

class VRayExporter;
class VRayRendererNode;
class ObjectExporter
{
public:
	explicit ObjectExporter(VRayExporter &pluginExporter);

	/// Reset exporter state. Clear plugin caches.
	void reset();

	/// Clears OBJ plugin cache.
	void clearOpPluginCache();

	/// Clears OBJ plugin dependency cache for
	/// non directly instancable object.
	void clearOpDepPluginCache();

	/// Clears primitive plugin cache.
	void clearPrimPluginCache();

	/// Sets the flag to actually export geometry.
	void setExportGeometry(int value) { doExportGeometry = value; }

	/// Returns export geometry flag.
	int getExportGeometry() const { return doExportGeometry; }

	/// Get the current value for the partition attribute
	const UT_String &getPartitionAttribute() const { return partitionAttribute; }
	
	/// Set the partition attribute
	void setPartitionAttribute(const UT_String &value) { partitionAttribute = value; }

	/// Test if the current geometry node is visible i.e.
	/// its display flag is on or it is forced to render regardless
	/// of its display state (when set as forced geometry on the V-Ray ROP)
	static int isNodeVisible(OP_Node &rop, OBJ_Node &node, fpreal t);

	/// Test if the current geometry node is visible i.e.
	/// its display flag is on or it is forced to render regardless
	/// of its display state (when set as forced geometry on the V-Ray ROP)
	int isNodeVisible(OBJ_Node &node) const;

	/// Test if a ligth is enabled i.e. its enabled flag is on,
	/// intensity is > 0 or its a forced light on the V-Ray ROP
	int isLightEnabled(OBJ_Node &objLight) const;

	/// Test if the current geometry node should be rendered
	/// as matte object (when set as matte geometry on the V-Ray ROP)
	int isNodeMatte(OBJ_Node &node) const;

	/// Test if the current geometry node should be rendered
	/// as phantom object (when set as phantom geometry on the V-Ray ROP)
	int isNodePhantom(OBJ_Node &node) const;

	static SubdivInfo getSubdivInfoFromMatNode(OP_Node &matNode);

	static SubdivInfo getSubdivInfoFromVRayMaterialOutput(OP_Node &matNode);

	/// Check if we need to export mesh as subdivision surface (displacement, subdivision).
	/// @param objNode OBJ node to check for subdivision properties.
	/// @param matNode VOP node ("vray_material_output") to check for subdivision properties. May be NULL.
	static SubdivInfo getSubdivInfo(OBJ_Node &objNode, OP_Node *matNode);

	int getPrimKey(const GA_Primitive &prim) const;

	int getPrimPluginFromCache(int primKey, VRay::Plugin &plugin) const;
	void addPrimPluginToCache(int primKey, VRay::Plugin &plugin);

	int getMeshPluginFromCache(int primKey, VRay::Plugin &plugin) const;
	void addMeshPluginToCache(int primKey, VRay::Plugin &plugin);

	int getPluginFromCache(Hash::MHash key, VRay::Plugin &plugin) const;
	void addPluginToCache(Hash::MHash key, VRay::Plugin &plugin);

	int getPluginFromCache(const char *key, VRay::Plugin &plugin) const;
	void addPluginToCache(const char *key, VRay::Plugin &plugin);

	int getPluginFromCache(const QString &key, VRay::Plugin &plugin) const;
	void addPluginToCache(const QString &key, VRay::Plugin &plugin);

	int getPluginFromCache(const OP_Node &opNode, VRay::Plugin &plugin) const;
	void addPluginToCache(OP_Node &opNode, VRay::Plugin &plugin);

	/// Helper function to generate unique id for the packed primitive
	/// this is used as key in m_detailToPluginDesc map to identify
	/// plugins generated for the primitve
	/// @param prim The packed primitive.
	/// @returns unique primitive id.
	int getPrimPackedID(const GU_PrimPacked &prim) const;

	void exportPolyMesh(OBJ_Node &objNode, const GU_Detail &gdp, const GEOPrimList &primList, Hash::MHash styleHash);

	void exportHair(OBJ_Node &objNode, const GU_Detail &gdp, const GEOPrimList &primList);

	VRay::Plugin exportPgYetiRef(OBJ_Node &objNode, const GU_PrimPacked &prim);

	VRay::Plugin exportVRayProxyRef(OBJ_Node &objNode, const GU_PrimPacked &prim);

	VRay::Plugin exportVRaySceneRef(OBJ_Node &objNode, const GU_PrimPacked &prim);

	VRay::Plugin exportGeomPlaneRef(OBJ_Node &objNode, const GU_PrimPacked &prim);

	VRay::Plugin exportAlembicRef(OBJ_Node &objNode, const GU_PrimPacked &prim);

	VRay::Plugin exportPackedDisk(OBJ_Node &objNode, const GU_PrimPacked &prim);

	void exportPackedFragment(OBJ_Node &objNode, const GU_PrimPacked &prim);

	void exportPackedGeometry(OBJ_Node &objNode, const GU_PrimPacked &prim);

	VRay::Plugin exportPrimPacked(OBJ_Node &objNode, const GU_PrimPacked &prim);

	VRay::Plugin exportPrimSphere(OBJ_Node &objNode, const GA_Primitive &prim);

	void exportPrimVolume(OBJ_Node &objNode,  const GA_Primitive &prim);

	void processPrimitives(OBJ_Node &objNode, const GU_Detail &gdp, const GA_Range &primRange=GA_Range());

	VRay::Plugin exportDetailInstancer(OBJ_Node &objNode);

	void exportDetail(OBJ_Node &objNode, const GU_Detail &gdp, const GA_Range &primRange=GA_Range());

	/// Export point particles data.
	/// @param objNode OBJ_Node instance.
	/// @param gdp Detail.
	/// @param pointsMode Point particles rendering mode.
	/// @returns Geometry plugin.
	VRay::Plugin exportPointParticles(OBJ_Node &objNode, const GU_Detail &gdp, VMRenderPoints pointsMode);

	/// Export point particle instancer.
	/// @param objNode OBJ_Node instance.
	/// @param gdp Detail.
	/// @param isInstanceNode Flag indicating we're exporting "instance" node.
	/// @returns Geometry plugin.
	void exportPointInstancer(OBJ_Node &objNode, const GU_Detail &gdp, int isInstanceNode=false);

	/// Returns true if object is a point intancer.
	/// @param gdp Detail.
	static int isPointInstancer(const GU_Detail &gdp);
	static int isInstanceNode(const OP_Node &node);

	/// Returns point particles mode.
	VMRenderPoints getParticlesMode(OBJ_Node &objNode) const;

	/// Instancer works only with Node plugins. This method will wrap geometry into
	/// the Node plugin if needed.
	/// @param primItem Instancer item.
	/// @param cacheEntry Top-level object cache entry.
	/// @returns Node plugin instance.
	VRay::Plugin getNodeForInstancerGeometry(const InstancerItem &primItem, ObjCacheEntry &cacheEntry);

	/// Export object.
	/// @returns Node plugin.
	VRay::Plugin exportObject(OBJ_Node &objNode);

	/// Remove object.
	void removeObject(OBJ_Node &objNode);

	/// Remove object.
	void removeObject(const char *objNode);
 
	/// Adds plugin to a list of plugins generated by a node.
	/// @param key Node intance.
	/// @param plugin V-Ray plugin instance.
	void addGenerated(OP_Node &key, VRay::Plugin plugin);

	/// Remove generated plugins.
	/// @param key Node intance.
	void removeGenerated(OP_Node &key);

	/// Remove generated plugins.
	/// @param key Node full path.
	void removeGenerated(const char *key);

	/// Export geometric object.
	/// @returns Node plugin.
	VRay::Plugin exportNode(OBJ_Node &objNode, SOP_Node *overrideSOP = nullptr);

	/// Export object geometry.
	/// @returns Geometry plugin.
	VRay::Plugin exportGeometry(OBJ_Node &objNode, SOP_Node *overrideSOP = nullptr);

	/// Export object geometry into a set of plugins.
	/// @param objNode OBJ node.
	/// @param items Output geometry list.
	void exportGeometry(OBJ_Node &objNode, InstancerItems &items, SOP_Node *overrideSOP = nullptr);

	/// Returns transform from the primitive context stack.
	VRay::Transform getTm() const { return primContextStack.getTm(); }

	/// Returns velocity from the primitive context stack.
	VRay::Transform getVel() const { return primContextStack.getVel(); }

	/// Returns primitive ID from the primitive context stack.
	exint getDetailID() const { return primContextStack.getDetailID(); }

	/// Returns material from the primitive context stack.
	const PrimMaterial &getPrimMaterial() const { return primContextStack.getPrimMaterial(); }

	/// Returns current level styler.
	STY_Styler getStyler() const { return primContextStack.getStyler(); }

	/// Returns the top-most object that we are exporting.
	const OBJ_Node &getGenerator(const OBJ_Node &currentObj) const { return primContextStack.getGenerator(currentObj); }

private:
	/// Push context frame when exporting nested object.
	void pushContext(const PrimContext &value) { primContextStack.pushContext(value); }

	/// Pop frame when going back to parent context.
	/// @returns Popped context item.
	PrimContext popContext() { return primContextStack.popContext(); }

	/// Export light object.
	/// @returns Light plugin.
	VRay::Plugin exportLight(OBJ_Light &objLight);

	/// Export SOP geometry.
	/// @returns Geometry plugin.
	void exportGeometry(OBJ_Node &objNode, SOP_Node &sopNode);

	/// Plugin exporter.
	VRayExporter &pluginExporter;

	/// Exporting context.
	VRayOpContext &ctx;

	/// Optional partition attribute to add when exporting poly soup primitives as different meshes
	/// If empty or not valid, we will export all poly soup primitives in one detail as one mesh
	UT_String partitionAttribute;

	/// A flag if we should export the actual geometry from the render
	/// detail or only update corresponding Nodes' properties. This is
	/// set by the IPR OBJ callbacks to signal the exporter of whether
	/// to re-export geometry plugins (i.e. something on the actual
	/// geometry has changed). By default this flag is on.
	int doExportGeometry;

	/// NOTE: mutable because HashMapKey doesn't have const methods.
	mutable struct PluginCaches {
		/// OP_Node centric plugin cache.
		PluginNameMap op;

		/// Unique primitive plugin cache.
		PrimPluginCache prim;

		/// Mesh primitive plugin cache.
		PrimPluginCache meshPrim;

		/// Mesh material override cache.
		HashPluginCache polyMaterial;

		/// Mesh map channel overrides cache.
		HashPluginCache polyMapChannels;

		/// Maps OP_Node with generated set of plugins for
		/// non directly instancable object.
		OpPluginGenCache generated;

		/// Plugin cache by data hash.
		HashPluginCache hashCache;
	} pluginCache;

	/// Plugins cache.
	OpCacheMan &cacheMan;

	/// Primitive export context stack.
	/// Used for non-Instancer objects like volumes and lights.
	PrimContextStack primContextStack;

	/// All primitive items for final Instancer for the current OBJ_Node.
	InstancerItems instancerItems;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_GEOM_H



