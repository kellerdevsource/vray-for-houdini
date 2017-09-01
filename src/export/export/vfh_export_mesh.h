//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
#include "vfh_plugin_attrs.h"
#include "vfh_exporter.h"
#include "vfh_geoutils.h"
#include "vfh_export_primitive.h"

namespace VRayForHoudini {

enum SkipMapChannel {
	skipMapChannelNone = 0, ///< Export all channels.
	skipMapChannelUV, ///< Skip UV channels.
	skipMapChannelNonUV, ///< Skip non-UV channels.
};

class VRayExporter;
class ObjectExporter;
/// Exports closed poly primitives and polysoups from a geometry detail
/// as single V-Ray mesh geometry plugin. The translator caches internally data
/// that has been processed once one of the get<Foo>() methods is called
/// so subsequent calls will be much faster. You should call init() to initialize
/// the translator with a geometry detail before using any of the get<Foo>()
/// methods
class MeshExporter
	: public PrimitiveExporter
{
public:
	MeshExporter(OBJ_Node &obj, const GU_Detail &gdp, OP_Context &ctx, VRayExporter &exp, ObjectExporter &objectExporter, const GEOPrimList &primList);
	~MeshExporter() {}

	/// Reset the translator as unintilized i.e.
	/// clear any cached data for current gdp (if any)
	/// and reset gdp to none
	void reset();

	/// Test if there is any valid mesh geometry to export
	bool hasData() const VRAY_OVERRIDE { return primList.size(); }

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

	/// Generate mesh plugin description from all supported primitives in the
	/// GU_Detail provided
	/// @note this will init the translator internally with the passed gdp
	/// @param gdp[in] - the detail to traverse
	/// @param pluginDesc[out] - the mesh plugin description
	/// @retval true if mesh primitives were found in gdp
	///         and pluginDesc is modified
	bool asPluginDesc(const GU_Detail &gdp, Attrs::PluginDesc &pluginDesc) VRAY_OVERRIDE;

	/// Build material taking overrides into account.
	VRay::Plugin getMaterial();
	VRay::Plugin getExtMapChannels();

private:
	/// Helper funtion to digest point attibutes into map channels
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	/// @param mapChannels[out] - collects generated map channels
	///        from point attributes
	/// @retval number of channels added to mapChannels
	int getPointAttrs(MapChannels &mapChannels, SkipMapChannel skipChannels=skipMapChannelNone);

	/// Helper funtion to digest vertex attibutes into map channels
	/// @note check if the translator has been properly initialized
	///       with hasPolyGeometry() before using this
	/// @param mapChannels[out] - collects generated map channels
	///        from vertex attributes
	/// @retval number of channels added to mapChannels
	int getVertexAttrs(MapChannels &mapChannels, SkipMapChannel skipChannels=skipMapChannelNone);

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
	void getMtlOverrides(MapChannels &mapChannels) const;

	VRay::Plugin exportExtMapChannels(const MapChannels &mapChannelOverrides) const;

	/// A list of poly primitives that can be handled by this translator.
	const GEOPrimList &primList;

	/// Current geometry detail
	const GU_Detail &gdp;

	ObjectExporter &objectExporter;

	bool                        m_hasSubdivApplied; ///< if we have subdivision applied to geometry at render time
	int                         numFaces; ///< number of mesh faces

	// Plugin parameters
	VRay::VUtils::IntRefList    faces; ///< list of mesh faces (triangles)
	VRay::VUtils::IntRefList    edge_visibility; ///< list of visible edges (1 bit per edge, 3 bits per face)
	VRay::VUtils::VectorRefList vertices; ///< list of mesh vertices
	VRay::VUtils::VectorRefList normals; ///< list of vertex normals
	VRay::VUtils::IntRefList    m_faceNormals; ///< list of faces for vertex normals
	VRay::VUtils::VectorRefList velocities; ///< list of vertex velocities

	/// Mesh mapping channels (UV only).
	MapChannels map_channels_data;
};


} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_MESH_H
