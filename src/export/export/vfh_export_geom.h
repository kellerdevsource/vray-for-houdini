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
#include "vfh_exporter.h"
#include "vfh_material_override.h"
#include "vfh_export_primitive.h"

#include <OBJ/OBJ_Geometry.h>
#include <GU/GU_PrimPacked.h>

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

/// Wraps the export of geometry from a OBJ_Geometry node.
/// In Houdini single gdp can contain multiple primitive types:
/// polygons, curves, volumes, packed geometry, delay load prims (bgeo,
/// alembic, V-Ray proxy), etc.
/// GeometryExporter will take up an OBJ node and handle the export of
/// different primitive types by calling the corresponding primitive exporter.
/// Usually this would result in generating separate Node plugin for each
/// primitive type: f: OBJ_Geometry -> set of Node plugins
class ObjectExporter;
class GeometryExporter
{
public:
	GeometryExporter(ObjectExporter &objExporter, OBJ_Node &objNode, VRayExporter &pluginExporter);
	~GeometryExporter() { }

	/// Test if the current geometry node is visible i.e.
	/// its display flag is on or it is forced to render regardless
	/// of its display state (when set as forced geometry on the V-Ray ROP)
	static int isNodeVisible(VRayRendererNode &rop, OBJ_Node &node);
	int isNodeVisible() const;

	/// Test if the current geometry node should be rendered
	/// as matte object (when set as matte geometry on the V-Ray ROP)
	int isNodeMatte() const;

	/// Test if the current geometry node should be rendered
	/// as phantom object (when set as phantom geometry on the V-Ray ROP)
	int isNodePhantom() const;

	/// This is set by the IPR OBJ callbacks to signal the exporter of
	/// whether to re-export the actual geometry (i.e. something on the
	/// geometry plugins has changed) or only update corresponding Nodes'
	/// properties.
	/// @param val[in] - a flag whether to re-export actual geometry
	void setExportGeometry(int value) { m_exportGeometry = value; }

	/// Test if subdivision has been applied to the node at render time
	/// @note This will affect the export of vertex attributes on mesh
	///       geometry. Vertex attribute values that have the same value
	///       will be welded into single one
	bool hasSubdivApplied() const;

private:
	/// Helper function to export geometry from a custom V-Ray SOP node
	/// @param sop V-Ray SOP node.
	/// @returns V-Ray plugin instance.
	VRay::Plugin exportVRaySOP(SOP_Node &vraySop);

	/// Helper function to traverse and export primitives from a given gdp
	/// This is called recursively when handling packed geometry prims
	/// @param sop[in] - parent SOP node for the gdp
	/// @param gdl[in] - read lock handle, guarding the actual gdp
	/// @param pluginList[out] - collects plugins generated for the gdp
	/// @retval number of plugin descriptions added to pluginList
	VRay::Plugin exportDetail(const GU_Detail &gdp);

	/// Helper function to export mesh geometry from a given gdp
	/// @param sop[in] - parent SOP node for the gdp
	/// @param gdp[in] - the actual gdp
	/// @param pluginList[out] - collects plugins generated for the gdp
	/// @retval number of plugin descriptions added to pluginList
	VRay::Plugin exportPolyMesh(const GU_Detail &gdp);

	/// Helper function to export a packed primitive. It will check if
	/// the primitive geometry has already been processed
	/// @param sop[in] - parent SOP node for the primitive
	/// @param prim[in] - the packed primitive
	/// @param pluginList[out] - collects plugins generated for the primitive
	/// @retval number of plugin descriptions added to pluginList
	VRay::Plugin exportPacked(const GU_PrimPacked &prim);

	/// Helper function to generate unique id for the packed primitive
	/// this is used as key in m_detailToPluginDesc map to identify
	/// plugins generated for the primitve
	/// @param prim[in] - the packed primitive
	/// @retval unique primitive id
	int getPrimPackedID(const GU_PrimPacked &prim);

	/// Helper function to export a packed primitive
	/// @param sop[in] - parent SOP node for the primitive
	/// @param prim[in] - the packed primitive
	/// @param pluginList[out] - collects plugins generated for the primitive
	/// @retval number of plugin descriptions added to pluginList
	VRay::Plugin exportPrimPacked(const GU_PrimPacked &prim);

	/// Helper function to export AlembicRef primitive
	/// @param sop[in] - parent SOP node for the primitive
	/// @param prim[in] - the primitive
	/// @param pluginList[out] - collects plugins generated for the primitive
	/// @retval number of plugin descriptions added to pluginList
	VRay::Plugin exportAlembicRef(const GU_PrimPacked &prim);

	/// Helper function to export VRayProxyRef primitive
	/// @param sop[in] - parent SOP node for the primitive
	/// @param prim[in] - the primitive
	/// @param pluginList[out] - collects plugins generated for the primitive
	/// @retval number of plugin descriptions added to pluginList
	VRay::Plugin exportVRayProxyRef(const GU_PrimPacked &prim);

	/// Helper function to export PackedDisk primitive (bgeo, bclassic)
	/// @param sop[in] - parent SOP node for the primitive
	/// @param prim[in] - the primitive
	/// @param pluginList[out] - collects plugins generated for the primitive
	/// @retval number of plugin descriptions added to pluginList
	VRay::Plugin exportPackedDisk(const GU_PrimPacked &prim);

	/// Helper function to export packed geometry primitive
	/// @param sop[in] - parent SOP node for the primitive
	/// @param prim[in] - the primitive
	/// @param pluginList[out] - collects plugins generated for the primitive
	/// @retval number of plugin descriptions added to pluginList
	VRay::Plugin exportPackedGeometry(const GU_PrimPacked &prim);

	/// Export points as particles.
	/// @param gdp Detail.
	/// @param renderPoints Points rendering mode.
	/// @param[out] pluginList Collects plugins generated for the primitive.
	/// @returns Number of plugin descriptions added to pluginList.
	int exportRenderPoints(const GU_Detail &gdp, VMRenderPoints renderPoints);

	/// Helper function to export the material for this OBJ node.
	/// The material from OBJ node 'shoppath' parameter together with
	/// V-Ray materials specified on per primitive basis are collected
	/// during traversal and export of geometry. These are then combined
	/// into single MtlMulti material which is assigned to all the Nodes.
	/// Node::user_attributes is utilized to specify the correct material
	/// id and index the correct material for the specific Node.
	/// @retval the material V-Ray plugin
	VRay::Plugin exportMaterial();

	/// Helper function to format material overrides specified on the object node
	/// as Node::user_attributes
	/// @param userAttrs[out] - string with formatted material overrides
	/// @retval number of overrides we have found
	int getSHOPOverridesAsUserAttributes(UT_String& userAttrs) const;

public:
	/// Export object geometry.
	/// @returns Geometry plugin.
	VRay::Plugin exportGeometry();

	/// Export SOP geometry.
	/// @returns Geometry plugin.
	VRay::Plugin exportGeometry(SOP_Node &sop);

private:
	/// Export point particles data.
	/// @param gdp Detail.
	/// @param pointsMode Point particles rendering mode.
	/// @returns Geometry plugin.
	VRay::Plugin exportPointParticles(const GU_Detail &gdp, VMRenderPoints pointsMode);

	/// Export point particle instancer.
	/// @param gdp Detail.
	/// @returns Geometry plugin.
	VRay::Plugin exportPointInstancer(const GU_Detail &gdp, int isInstanceNode=false);

	/// Export instancer.
	/// @param gdp Detail.
	/// @returns Geometry plugin.
	VRay::Plugin exportInstancer(const GU_Detail &gdp);

	/// Returns true if object is a point intancer.
	/// @param gdp Detail.
	int isPointInstancer(const GU_Detail &gdp) const;

	static int isInstanceNode(const OP_Node &node);

	/// Returns point particles mode.
	VMRenderPoints getParticlesMode() const;

	VRay::Plugin getNodeForInstancerGeometry(VRay::Plugin geometry);

	/// Object exporter for "path" / "instancepath".
	ObjectExporter &objExporter;

	/// The OBJ node owner of all details that will be processed
	/// by this exporter
	OBJ_Node &objNode;

	/// Exporting context.
	OP_Context &ctx;

	/// Plugin exporter.
	VRayExporter &pluginExporter;

	/// A flag if we should export the actual geometry from the render
	/// detail or only update corresponding Nodes' properties. This is
	/// set by the IPR OBJ callbacks to signal the exporter of whether
	/// to re-export geometry plugins (i.e. something on the actual
	/// geometry has changed). By default this flag is on.
	int m_exportGeometry;

	/// Primitive items.
	InstancerItems instancerItems;
};

class ObjectExporter
{
public:
	ObjectExporter(VRayExporter &pluginExporter, OBJ_Node &objNode);
	
	void setExportGeometry(int value) { exportGeometry = value; }

	VRay::Plugin exportNode();

protected:
	/// Plugin exporter.
	VRayExporter &pluginExporter;

	/// Object to export.
	OBJ_Node &objNode;

	/// Primitive items.
	InstancerItems instancerItems;

	/// Export node geometry data or just the node attributes.
	int exportGeometry;
};

/// Clears OBJ plugin cache.
void clearOpPluginCache();

/// Clears primitive plugin cache.
void clearPrimPluginCache();

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_GEOM_H



