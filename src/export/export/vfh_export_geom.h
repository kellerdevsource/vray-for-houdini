//
// Copyright (c) 2015-2016, Chaos Software Ltd
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
#include "vfh_export_primitive.h"
#include "vfh_geoutils.h"

#include <QStack>

#include <GU/GU_PrimPacked.h>
#include <OBJ/OBJ_Light.h>

namespace VRayForHoudini {

enum VMRenderPoints {
	vmRenderPointsNone = 0, ///< Don't render points separately from primitives.
	vmRenderPointsAll, ///< Render only points.
	vmRenderPointsUnconnected, ///< Render unconnected points.
};

enum VMRenderPointsAs {
	vmRenderPointsAsSphere = 0, ///< Render points as spheres. Maps to GeomParticleSystem "render_type" 7 (Spheres).
	vmRenderPointsAsCirle, ///< Render points as circles. Maps GeomParticleSystem "render_type" 6 (Points).
};

/// Primitive export context item.
/// Used for non-Instancer objects like volumes and lights.
struct PrimContext {
	explicit PrimContext(OP_Node *generator=nullptr,
						 VRay::Transform tm=VRay::Transform(1),
						 exint detailID=0,
						 PrimMaterial primMaterial=PrimMaterial())
		: generator(generator)
		, tm(tm)
		, detailID(detailID)
		, primMaterial(primMaterial)
	{}

	OP_Node *generator;

	/// Base transform.
	VRay::Transform tm;

	/// Primitive ID.
	exint detailID;

	/// Material overrides.
	PrimMaterial primMaterial;
};

/// Primitive export context stack.
typedef QStack<PrimContext> PrimContextStack;

/// Primitive export context stack iterator.
typedef QVectorIterator<PrimContext> PrimContextIt;

class VRayExporter;
class VRayRendererNode;
class ObjectExporter
{
	typedef VUtils::HashMapKey<OP_Node*, PluginSet> OpPluginGenCache;
	typedef VUtils::HashMapKey<OP_Node*, VRay::Plugin> OpPluginCache;
	typedef VUtils::HashMapKey<int, VRay::Plugin> PrimPluginCache;
	typedef VUtils::HashMap<VRay::Plugin> GeomNodeCache;

public:
	explicit ObjectExporter(VRayExporter &pluginExporter);

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

	/// Test if the current geometry node is visible i.e.
	/// its display flag is on or it is forced to render regardless
	/// of its display state (when set as forced geometry on the V-Ray ROP)
	static int isNodeVisible(VRayRendererNode &rop, OBJ_Node &node);

	/// Test if a ligth is enabled i.e. its enabled flag is on,
	/// intensity is > 0 or its a forced light on the V-Ray ROP
	int isLightEnabled(OBJ_Node &objLight) const;

	/// Test if the current geometry node is visible i.e.
	/// its display flag is on or it is forced to render regardless
	/// of its display state (when set as forced geometry on the V-Ray ROP)
	int isNodeVisible(OBJ_Node &node) const;

	/// Test if the current geometry node should be rendered
	/// as matte object (when set as matte geometry on the V-Ray ROP)
	int isNodeMatte(OBJ_Node &node) const;

	/// Test if the current geometry node should be rendered
	/// as phantom object (when set as phantom geometry on the V-Ray ROP)
	int isNodePhantom(OBJ_Node &node) const;

	/// Test if subdivision has been applied to the node at render time
	/// @note This will affect the export of vertex attributes on mesh
	///       geometry. Vertex attribute values that have the same value
	///       will be welded into single one
	bool hasSubdivApplied(OBJ_Node &objNode) const;

	int getPrimKey(const GA_Primitive &prim);

	int getPrimPluginFromCache(int primKey, VRay::Plugin &plugin);

	/// It's ok to add invalid plugins to cache here,
	/// because if we've failed to export plugin once we should not retry.
	void addPrimPluginToCache(int primKey, VRay::Plugin &plugin);

	/// Helper function to generate unique id for the packed primitive
	/// this is used as key in m_detailToPluginDesc map to identify
	/// plugins generated for the primitve
	/// @param prim The packed primitive.
	/// @returns unique primitive id.
	int getPrimPackedID(const GU_PrimPacked &prim);

	void exportPolyMesh(OBJ_Node &objNode, const GU_Detail &gdp, const GEOPrimList &primList, PrimitiveItems &instancerItems);

	void exportHair(OBJ_Node &objNode, const GU_Detail &gdp, const GEOPrimList &primList, PrimitiveItems &instancerItems);

	/// A helper function to export geometry from a custom V-Ray SOP node.
	/// @param sop V-Ray SOP node.
	/// @returns V-Ray plugin instance.
	VRay::Plugin exportVRaySOP(OBJ_Node &objNode, SOP_Node &sop);

	VRay::Plugin exportVRayProxyRef(OBJ_Node &objNode, const GU_PrimPacked &prim);

	VRay::Plugin exportAlembicRef(OBJ_Node &objNode, const GU_PrimPacked &prim);

	VRay::Plugin exportPackedDisk(OBJ_Node &objNode, const GU_PrimPacked &prim);

	VRay::Plugin exportPackedGeometry(OBJ_Node &objNode, const GU_PrimPacked &prim);

	VRay::Plugin exportPrimPacked(OBJ_Node &objNode, const GU_PrimPacked &prim);

	VRay::Plugin exportPrimSphere(OBJ_Node &objNode, const GA_Primitive &prim);

	void exportPrimVolume(OBJ_Node &objNode, const PrimitiveItem &item);

	void processPrimitives(OBJ_Node &objNode, const GU_Detail &gdp, PrimitiveItems &instancerItems);

	VRay::Plugin exportDetailInstancer(OBJ_Node &objNode, const GU_Detail &gdp, const PrimitiveItems &instancerItems, const char *prefix);

	VRay::Plugin exportDetail(OBJ_Node &objNode, const GU_Detail &gdp);

	/// Export point particles data.
	/// @param gdp Detail.
	/// @param pointsMode Point particles rendering mode.
	/// @returns Geometry plugin.
	VRay::Plugin exportPointParticles(OBJ_Node &objNode, const GU_Detail &gdp, VMRenderPoints pointsMode);

	/// Export point particle instancer.
	/// @param gdp Detail.
	/// @returns Geometry plugin.
	VRay::Plugin exportPointInstancer(OBJ_Node &objNode, const GU_Detail &gdp, int isInstanceNode=false);

	/// Returns true if object is a point intancer.
	/// @param gdp Detail.
	static int isPointInstancer(const GU_Detail &gdp);
	static int isInstanceNode(const OP_Node &node);

	/// Returns point particles mode.
	VMRenderPoints getParticlesMode(OBJ_Node &objNode) const;

	/// Instancer works only with Node plugins. This method will wrap geometry into
	/// the Node plugin if needed.
	/// @param geometry Geometry plugin instance.
	/// @returns Node plugin instance.
	VRay::Plugin getNodeForInstancerGeometry(VRay::Plugin geometry, VRay::Plugin objMaterial);

	/// Export object geometry.
	/// @returns Geometry plugin.
	VRay::Plugin exportGeometry(OBJ_Node &objNode);

	/// Export object.
	/// @returns Node plugin.
	VRay::Plugin exportObject(OBJ_Node &objNode);

	/// Remove object.
	void removeObject(OBJ_Node &objNode);
 
	/// Returns transform from the primitive context stack.
	VRay::Transform getTm() const;

	/// Returns primitive ID from the primitive context stack.
	exint getDetailID() const;

	/// Returns material from the primitive context stack.
	void getPrimMaterial(PrimMaterial &primMaterial) const;

	OP_Node *getGenerator() const;

	void addGenerated(OP_Node &key, VRay::Plugin plugin);

	void removeGenerated(OP_Node &key);

private:
	/// Push context frame when exporting nested object.
	void pushContext(const PrimContext &value) { primContextStack.push(value); }

	/// Pop frame when going back to parent context.
	/// @returns Popped context frame.
	PrimContext popContext() { return primContextStack.pop(); }

	/// Export light object.
	/// @returns Light plugin.
	VRay::Plugin exportLight(OBJ_Light &objLight);

	/// Export geometric object.
	/// @returns Node plugin.
	VRay::Plugin exportNode(OBJ_Node &objNode);

	/// Plugin exporter.
	VRayExporter &pluginExporter;

	/// Exporting context.
	OP_Context &ctx;

	/// A flag if we should export the actual geometry from the render
	/// detail or only update corresponding Nodes' properties. This is
	/// set by the IPR OBJ callbacks to signal the exporter of whether
	/// to re-export geometry plugins (i.e. something on the actual
	/// geometry has changed). By default this flag is on.
	int doExportGeometry;

	struct PluginCaches {
		/// OP_Node centric plugin cache.
		OpPluginCache op;

		/// Unique primitive plugin cache.
		PrimPluginCache prim;

		/// Wrapper nodes cache for Instancer plugin.
		GeomNodeCache instancerNodeWrapper;

		/// Maps OP_Node with generated set of plugins for
		/// non directly instancable object.
		OpPluginGenCache generated;
	} pluginCache;

	/// Primitive export context stack.
	/// Used for non-Instancer objects like volumes and lights.
	PrimContextStack primContextStack;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_GEOM_H



