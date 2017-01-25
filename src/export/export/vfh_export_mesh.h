//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORT_MESH_H
#define VRAY_FOR_HOUDINI_EXPORT_MESH_H

#include "vfh_vray.h"
#include "vfh_defines.h"
#include "vfh_plugin_attrs.h"
#include "vfh_exporter.h"
#include "vfh_geoutils.h"
#include "vfh_material_override.h"
#include "vfh_export_primitive.h"

#include <SOP/SOP_Node.h>
#include <SHOP/SHOP_Node.h>

#include <unordered_set>
#include <unordered_map>


namespace VRayForHoudini {

class VRayExporter;

/// Exports closed poly primitives and polysoups from a geometry detail
/// as single V-Ray mesh geometry plugin. The translator caches internally data
/// that has been processed once one of the get<Foo>() methods is called
/// so subsequent calls will be much faster. You should call init() to initialize
/// the translator with a geometry detail before using any of the get<Foo>()
/// methods.
class MeshExporter:
		public PrimitiveExporter
{
public:
	/// Test if a primitive can be handled by this translator i.e
	/// closed poly or polysoup
	/// @param prim[in] - the primitive to test
	static bool isPrimPoly(const GEO_Primitive *prim);

	/// Fast test if the detail contains primitives that can be handled
	/// by this translator
	/// @note this will test the gdp only for primitives of certail type
	///       but not the actual primitives i.e. if the detail contains
	///       poly prims all of which are open this function will return
	///       a false positive
	/// @param gdp[in] - detail to test
	static bool containsPolyPrimitives(const GU_Detail &gdp);

public:
	MeshExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp);
	~MeshExporter() { }

	/// Intilize the translator with the passed gdp.
	/// This will clear any cached data for current gdp (if any)
	/// and init the translator with the new one.
	/// @note you should call init() to initialize the translator
	///       with a geometry detail before using any of the get<Foo>()
	/// @param gdp[in] - geometry detail
	/// @retval true if the translator is properly initilized
	///         with the new gdp, or false on error or if the
	///         gdp passed is the same as the current one (comaprison
	///         is based on detail unique id) in which case cached data
	///         will remain.
	bool init(const GU_Detail &gdp);

	/// Reset the translator as unintilized i.e.
	/// clear any cached data for current gdp (if any)
	/// and reset gdp to none
	void reset();

	/// Test if there is any valid mesh geometry to export
	bool hasPolyGeometry()
	{ return m_gdp != nullptr && containsPolyPrimitives(*m_gdp) && getNumFaces() > 0; }

	/// Test if we have subdivision applied to geometry at render time
	bool hasSubdivApplied() const { return m_hasSubdivApplied; }

	/// Set if we have subdivision applied to geometry at render time
	/// If true this will weld vertex attribute values when exporting
	/// map channels from uvs and other custom vertex attributes
	void setSubdivApplied(bool val) { m_hasSubdivApplied = val; }

	/// Get the number of vertices
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	int getNumVertices() { return getVertices().size(); }

	/// Get the list vertices (cached internally)
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	VRay::VUtils::VectorRefList& getVertices();

	/// Get the number of faces (cached internally)
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	int getNumFaces();

	/// Get the list of faces (cached internally)
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	VRay::VUtils::IntRefList& getFaces();

	/// Get the list of visible edges (cached internally)
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	VRay::VUtils::IntRefList& getEdgeVisibility();

	/// Get the number of vertex normals
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	int getNumNormals() { return getNormals().size(); }

	/// Get the list of vertex normals (cached internally)
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	VRay::VUtils::VectorRefList& getNormals();

	/// Get the list of faces for the normals (cached internally)
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	VRay::VUtils::IntRefList getFaceNormals();

	/// Get the number of vertex velocities
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	int getNumVelocities() { return getVelocities().size(); }

	/// Get the vertex velocities (cached internally)
	/// For V-Ray velocity makes sense only when assigned on points, so
	/// faces is used to index velocity as well
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	VRay::VUtils::VectorRefList& getVelocities();

	/// Get the number of map channels
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	int getNumMapChannels() { return getMapChannels().size(); }

	/// Get the list of map channels (cached internally)
	/// uvs as well as additional point and vertex attributes
	/// are passed as map channels to V-Ray
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	MapChannels& getMapChannels();

	/// Get the number of matrial ids
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	int getNumMtlIDs() { return getFaceMtlIDs().size(); }

	/// Get the list of per face matrial ids (cached internally)
	/// These are ued by MtlMulti plugin to index the correct material
	/// for the face
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	VRay::VUtils::IntRefList& getFaceMtlIDs();

	/// Get the list of valid materials assigned per face
	/// based on "shop_materialpath" attribute. These are exported as
	/// MtlMulti to allow different materials per face
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	/// @param shopList[out]
	/// @retval number of valid materials found
	int getSHOPList(SHOPList &shopList);

	/// Generate mesh plugin description from all supported primitives in the
	/// GU_Detail provided
	/// @note this will init the translator internally with the passed gdp
	/// @param gdp[in] - the detail to traverse
	/// @param pluginDesc[out] - the mesh plugin description
	/// @retval true if mesh primitives were found in gdp
	///         and pluginDesc is modified
	virtual bool asPluginDesc(const GU_Detail &gdp, Attrs::PluginDesc &pluginDesc) VRAY_OVERRIDE;

	/// Export mesh geometry plugin and generate Node plugin description
	/// for all supported primitives in the GU_Detail provided
	/// @note calls asPluginDesc() to export the geometry
	/// @param gdp[in] - the detail to traverse
	/// @param plugins[out] - collects the Node plugins generated for this detail
	virtual void exportPrimitives(const GU_Detail &gdp, PluginDescList &plugins) VRAY_OVERRIDE;

private:
	/// Helper structure used when digesting material overrides into map channels
	struct PrimOverride
	{
		typedef std::unordered_map< std::string, VRay::Vector > MtlOverrides;

		PrimOverride(SHOP_Node *shopNode = nullptr):
			shopNode(shopNode)
		{}

		SHOP_Node    *shopNode; ///< the material assigned to the primitive with "shop_materialpath" attribute
		MtlOverrides mtlOverrides; ///< map of map channel name to override value for the face
	};

private:
	/// Get the list of primitives that can be handled by MeshExporter(cached internally)
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	const GEOPrimList& getPrimList();

	/// Helper funtion to digest point attibutes into map channels
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	/// @param mapChannels[out] - collects generated map channels
	///        from point attributes
	/// @retval number of channels added to mapChannels
	int getPointAttrs(MapChannels &mapChannels);


	/// Helper funtion to digest vertex attibutes into map channels
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	/// @param mapChannels[out] - collects generated map channels
	///        from vertex attributes
	/// @retval number of channels added to mapChannels
	int getVertexAttrs(MapChannels &mapChannels);

	/// Helper funtion to digest single vertex attibute into a map channel
	/// @note called from getVertexAttrs() when adding new map channels
	///       check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	/// @param attr[in] - vertex attribute to digest
	/// @param mapChannel[out] - map channel to populate
	void getVertexAttrAsMapChannel(const GA_Attribute &attr, MapChannel &mapChannel);

	/// Helper funtion to digest per primitive material overrides into map channels
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	/// @param mapChannels[out] - collects generated map channels
	///        from material override attribute
	/// @retval number of channels added to mapChannels
	int getMtlOverrides(MapChannels &mapChannels);

	/// Helper funtion to digest per primitive material overrides into map channels
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	/// @param o_mapChannelOverrides[out] - map channel names that will be generated
	///       from material overrides
	/// @param o_primOverrides[out] - list of per primitive overrides
	/// @retval number of map channels to be generated from material overrides
	int getPerPrimMtlOverrides(std::unordered_set< std::string > &o_mapChannelOverrides,
							   std::vector< PrimOverride > &o_primOverrides);

private:
	const GU_Detail            *m_gdp; ///< current geometry detail
	bool                        m_hasSubdivApplied; ///< if we have subdivision applied to geometry at render time
	GEOPrimList                 m_primList; ///< list of primitives that can be handled by this translator
	int                         numFaces; ///< number of mesh faces
	VRay::VUtils::IntRefList    faces; ///< list of mesh faces (triangles)
	VRay::VUtils::IntRefList    edge_visibility; ///< list of visible edges (1 bit per edge, 3 bits per face)
	VRay::VUtils::VectorRefList vertices; ///< list of mesh vertices
	VRay::VUtils::VectorRefList normals; ///< list of vertex normals
	VRay::VUtils::IntRefList    m_faceNormals; ///< list of faces for vertex normals
	VRay::VUtils::VectorRefList velocities; ///< list of vertex velocities
	VRay::VUtils::IntRefList    face_mtlIDs; ///< list of material ids used to index the correct material
	MapChannels                 map_channels_data; /// mesh map channels
};


} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_MESH_H
