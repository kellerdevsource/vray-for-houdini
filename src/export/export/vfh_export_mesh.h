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

/// Exports closed poly primitives and polysoups as V-Ray mesh geometry
/// from geometry detail
class MeshExporter:
		public PrimitiveExporter
{
public:
	/// Test if a primitive is polygonal primitive i.e
	/// closed poly or polysoup
	/// @param prim[in] - the primitive to test
	static bool isPrimPoly(const GEO_Primitive *prim);

	/// Fast test if the detail contains mesh primitives
	/// @note this will test the gdp only for primitives of certail type
	///       but not the actual primitives i.e if the detail contains
	///       poly prims all of which are open this function will return
	///       a false positive
	/// @param gdp[in] - detail to test
	static bool containsPolyPrimitives(const GU_Detail &gdp);

public:
	MeshExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp);
	~MeshExporter() { }

	bool init(const GU_Detail &gdp);
	void reset();

	bool hasPolyGeometry()
	{ return m_gdp != nullptr && containsPolyPrimitives(*m_gdp) && getNumFaces() > 0; }

	void setSubdivApplied(bool val) { m_hasSubdivApplied = val; }
	bool hasSubdivApplied() const { return m_hasSubdivApplied; }

	int getNumVertices() { return getVertices().size(); }
	VRay::VUtils::VectorRefList& getVertices();

	int getNumFaces();
	VRay::VUtils::IntRefList& getFaces();
	VRay::VUtils::IntRefList& getEdgeVisibility();

	int getNumNormals() { return getNormals().size(); }
	VRay::VUtils::VectorRefList& getNormals();
	VRay::VUtils::IntRefList getFaceNormals();

	int getNumVelocities() { return getVelocities().size(); }
	VRay::VUtils::VectorRefList& getVelocities();

	int getNumMapChannels() { return getMapChannels().size(); }
	MapChannels& getMapChannels();

	int getNumMtlIDs() { return getFaceMtlIDs().size(); }
	VRay::VUtils::IntRefList& getFaceMtlIDs();

	int getSHOPList(SHOPList &shopList);

	/// Generate mesh plugin description from all supported primitives in the
	/// GU_Detail provided
	/// @param gdp[in] - the detail to traverse
	/// @param pluginDesc[out] - the mesh plugin description
	/// @retval true if mesh primitives were found in gdp
	///         and pluginDesc is modified
	virtual bool asPluginDesc(const GU_Detail &gdp, Attrs::PluginDesc &pluginDesc) VRAY_OVERRIDE;

	/// Export mesh geometry plugin and generate Node plugin description
	/// for all supported primitives in the GU_Detail provided
	/// @param gdp[in] - the detail to traverse
	/// @plugins[out] - the list of plugins generated for this detail
	virtual void exportPrimitives(const GU_Detail &gdp, PluginDescList &plugins) VRAY_OVERRIDE;

private:
	struct PrimOverride
	{
		typedef std::unordered_map< std::string, VRay::Vector > MtlOverrides;

		PrimOverride(SHOP_Node *shopNode = nullptr):
			shopNode(shopNode)
		{}

		SHOP_Node *shopNode;
		MtlOverrides mtlOverrides;
	};

private:
	const GEOPrimList& getPrimList();
	int     getPointAttrs(MapChannels &mapChannels);
	int     getVertexAttrs(MapChannels &mapChannels);
	void    getVertexAttrAsMapChannel(const GA_Attribute &attr, MapChannel &mapChannel);
	int     getMtlOverrides(MapChannels &mapChannels);
	int     getPerPrimMtlOverrides(std::unordered_set< std::string > &o_mapChannelOverrides, std::vector< PrimOverride > &o_primOverrides);

private:
	const GU_Detail            *m_gdp;
	bool                        m_hasSubdivApplied;
	GEOPrimList                 m_primList;
	int                         numFaces;
	VRay::VUtils::IntRefList    faces;
	VRay::VUtils::IntRefList    edge_visibility;
	VRay::VUtils::VectorRefList vertices;
	VRay::VUtils::VectorRefList normals;
	VRay::VUtils::IntRefList    m_faceNormals;
	VRay::VUtils::VectorRefList velocities;
	VRay::VUtils::IntRefList    face_mtlIDs;
	MapChannels                 map_channels_data;
};


} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_MESH_H
