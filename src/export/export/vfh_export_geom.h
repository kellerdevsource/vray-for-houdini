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
class GeometryExporter
{
public:
	GeometryExporter(OBJ_Geometry &node, VRayExporter &pluginExporter);
	~GeometryExporter() { }

	/// Test if the current geometry node is visible i.e.
	/// its display flag is on or it is forced to render regardless
	/// of its display state (when set as forced geometry on the V-Ray ROP)
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
	GeometryExporter& setExportGeometry(bool val)
	{ m_exportGeometry = val; return *this; }

	/// Test if subdivision has been applied to the node at render time
	/// @note This will affect the export of vertex attributes on mesh
	///       geometry. Vertex attribute values that have the same value
	///       will be welded into single one
	bool hasSubdivApplied() const;

	/// Export the object's geometry. Different primitive types are handled by
	/// different primitive exporters ususally by exporting separate Node plugins
	/// for each type.
	/// @retval number of plugin descriptions generated for this
	///         OBJ node
	int exportNodes();

	/// Clear data generated from export
	void reset();

	/// Get number of plugin descriptions generated for this OBJ node
	int getNumPluginDesc() const;

	/// Get the plugin description at a given index
	/// @param idx[in] - index in range [0, getNumPluginDesc())
	/// @retval the plugin description at the specified index
	///         throws std::out_of_range if no such exists
	Attrs::PluginDesc& getPluginDescAt(int idx);

private:
	/// Helper function to export geometry from a custom V-Ray SOP node
	/// @param sop[in] - V-Ray custom SOP node (V-Ray plane or V-Ray scene)
	/// @param pluginList[out] - collects plugins generated for the SOP node
	/// @retval number of plugin descriptions added to pluginList
	int exportVRaySOP(SOP_Node &sop, PluginDescList &pluginList);

	/// Helper function to traverse and export primitives from a given gdp
	/// This is called recursively when handling packed geometry prims
	/// @param sop[in] - parent SOP node for the gdp
	/// @param gdl[in] - read lock handle, guarding the actual gdp
	/// @param pluginList[out] - collects plugins generated for the gdp
	/// @retval number of plugin descriptions added to pluginList
	int exportDetail(SOP_Node &sop, GU_DetailHandleAutoReadLock &gdl, PluginDescList &pluginList);

	/// Helper function to export mesh geometry from a given gdp
	/// @param sop[in] - parent SOP node for the gdp
	/// @param gdp[in] - the actual gdp
	/// @param pluginList[out] - collects plugins generated for the gdp
	/// @retval number of plugin descriptions added to pluginList
	int exportPolyMesh(SOP_Node &sop, const GU_Detail &gdp, PluginDescList &pluginList);

	/// Helper function to export a packed primitive. It will check if
	/// the primitive geometry has already been processed
	/// @param sop[in] - parent SOP node for the primitive
	/// @param prim[in] - the packed primitive
	/// @param pluginList[out] - collects plugins generated for the primitive
	/// @retval number of plugin descriptions added to pluginList
	int exportPacked(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);

	/// Helper function to generate unique id for the packed primitive
	/// this is used as key in m_detailToPluginDesc map to identify
	/// plugins generated for the primitve
	/// @param prim[in] - the packed primitive
	/// @retval unique primitive id
	uint getPrimPackedID(const GU_PrimPacked &prim);

	/// Helper function to export a packed primitive
	/// @param sop[in] - parent SOP node for the primitive
	/// @param prim[in] - the packed primitive
	/// @param pluginList[out] - collects plugins generated for the primitive
	/// @retval number of plugin descriptions added to pluginList
	int exportPrimPacked(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);

	/// Helper function to export AlembicRef primitive
	/// @param sop[in] - parent SOP node for the primitive
	/// @param prim[in] - the primitive
	/// @param pluginList[out] - collects plugins generated for the primitive
	/// @retval number of plugin descriptions added to pluginList
	int exportAlembicRef(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);

	/// Helper function to export VRayProxyRef primitive
	/// @param sop[in] - parent SOP node for the primitive
	/// @param prim[in] - the primitive
	/// @param pluginList[out] - collects plugins generated for the primitive
	/// @retval number of plugin descriptions added to pluginList
	int exportVRayProxyRef(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);

	/// Helper function to export PackedDisk primitive (bgeo, bclassic)
	/// @param sop[in] - parent SOP node for the primitive
	/// @param prim[in] - the primitive
	/// @param pluginList[out] - collects plugins generated for the primitive
	/// @retval number of plugin descriptions added to pluginList
	int exportPackedDisk(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);

	/// Helper function to export packed geometry primitive
	/// @param sop[in] - parent SOP node for the primitive
	/// @param prim[in] - the primitive
	/// @param pluginList[out] - collects plugins generated for the primitive
	/// @retval number of plugin descriptions added to pluginList
	int exportPackedGeometry(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);

	/// Export points as particles.
	/// @param gdp Detail.
	/// @param renderPoints Points rendering mode.
	/// @param[out] pluginList Collects plugins generated for the primitive.
	/// @returns Number of plugin descriptions added to pluginList.
	int exportRenderPoints(const GU_Detail &gdp, VMRenderPoints renderPoints, PluginDescList &pluginList);

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

private:
	typedef std::unordered_map< uint, PluginDescList > DetailToPluginDesc;

	/// The OBJ node owner of all details that will be processed
	/// by this exporter
	OBJ_Geometry        &m_objNode;
	/// Current time
	OP_Context          &m_context;
	/// A reference to the main vfh exporter
	VRayExporter        &m_pluginExporter;

	/// A flag if we should export the actual geometry from the render
	/// detail or only update corresponding Nodes' properties. This is
	/// set by the IPR OBJ callbacks to signal the exporter of whether
	/// to re-export geometry plugins (i.e. something on the actual
	/// geometry has changed). By default this flag is on.
	bool                 m_exportGeometry;
	/// Unique id of the main gdp (taken from the render SOP)
	/// This is used as a key in m_detailToPluginDesc map to
	/// identify Node plugins generated for this OBJ node
	uint                 m_myDetailID;
	/// A map of gdp unique id to list of Node plugins generated
	/// for this gdp. This is used to check if geometry has already
	/// been processed and what are the corrsponding V-Ray Node plugins
	/// when exporting packed geometry or delay load prims.
	DetailToPluginDesc   m_detailToPluginDesc;
	/// A list of V-Ray materials that accumulates all the materials that should
	/// be exported for this OBJ node. These are combined into a single MtlMulti
	/// V-Ray plugin and the corresponding material id (generated from SHOPHasher
	/// and properly formatted in Node::user_attributes) is used to index the
	/// correct material for the Node. For example: when dealing with instanced
	/// packed geometry primitives, single geometry could have different materials
	/// assigned. These are accumulated here.
	/// @note the material from OBJ node 'shoppath' parameter is always exported
	///       with id 0. If no such is specified this would be the default material.
	SHOPList             m_shopList;
};


} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_GEOM_H



