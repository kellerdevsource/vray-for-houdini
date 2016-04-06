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
#include "vfh_hashes.h"
#include "vfh_defines.h"
#include "vfh_plugin_attrs.h"
#include "vfh_exporter.h"
#include "vfh_material_override.h"

#include <SOP/SOP_Node.h>
#include <SHOP/SHOP_Node.h>

#include <unordered_set>
#include <unordered_map>


namespace VRayForHoudini {

class VRayExporter;


struct MapVertex {
	MapVertex():
		index(0)
	{ }

	MapVertex(const UT_Vector3 &vec):
		index(0)
	{
		v[0] = vec[0];
		v[1] = vec[1];
		v[2] = vec[2];
	}

	bool operator == (const MapVertex &_v) const
	{
		return (v[0] == _v.v[0]) && (v[1] == _v.v[1]) && (v[2] == _v.v[2]);
	}

	float        v[3];
	mutable int  index;
};


struct MapVertexHash
{
	std::size_t operator () (const MapVertex &_v) const
	{
		VRayForHoudini::Hash::MHash hash;
		VRayForHoudini::Hash::MurmurHash3_x86_32(_v.v, 3 * sizeof(float), 42, &hash);
		return (std::size_t)hash;
	}
};


struct MapChannel
{
	typedef std::unordered_set<MapVertex, MapVertexHash> VertexSet;

	std::string                 name;
	VRay::VUtils::VectorRefList vertices;
	VRay::VUtils::IntRefList    faces;
	VertexSet                   verticesSet;
};


typedef std::unordered_map<std::string, MapChannel> MapChannels;


class MeshExporter
{
public:
	static bool isPrimPoly(const GA_Primitive &prim);
	static bool getDataFromAttribute(const GA_Attribute *attr, VRay::VUtils::VectorRefList &data);

public:
	MeshExporter(const GU_Detail &gdp, VRayExporter &pluginExporter);
	~MeshExporter() { }

	MeshExporter& setSOPContext(SOP_Node *sop) { m_sopNode = sop; return *this; }
	MeshExporter& setSubdivApplied(bool val) { m_hasSubdivApplied = val; return *this; }

	bool                         hasPolyGeometry() const;
	bool                         hasSubdivApplied() const { return m_hasSubdivApplied; }
	int                          getNumVertices() { return getVertices().size(); }
	int                          getNumNormals() { return getNormals().size(); }
	int                          getNumFaces()  { if (numFaces <= 0 ) numFaces = countFaces(); return numFaces; }
	int                          getNumMtlIDs() { if (numMtlIDs <= 0 ) getFaceMtlIDs(); return numMtlIDs; }
	int                          getNumMapChannels() { return getMapChannels().size(); }
	VRay::VUtils::VectorRefList& getVertices();
	VRay::VUtils::VectorRefList& getNormals();
	VRay::VUtils::IntRefList&    getFaces();
	VRay::VUtils::IntRefList&    getFaceNormals() { return getFaces(); }
	VRay::VUtils::IntRefList&    getEdgeVisibility();
	VRay::VUtils::IntRefList&    getFaceMtlIDs();
	MapChannels&                 getMapChannels();

	int                          getSHOPList(SHOPList &shopList) const;

	std::string                  getVRayPluginType() const { return "GEOMETRY"; }
	std::string                  getVRayPluginID() const   { return "GeomStaticMesh"; }
	std::string                  getVRayPluginName() const;
	bool                         asPluginDesc(Attrs::PluginDesc &pluginDesc);

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
	GA_Size countFaces() const;
	int     getMeshFaces(VRay::VUtils::IntRefList &faces, VRay::VUtils::IntRefList &edge_visibility);
	int     getMtlIds(VRay::VUtils::IntRefList &face_mtlIDs);
	int     getPointAttrs(MapChannels &mapChannels);
	int     getVertexAttrs(MapChannels &mapChannels);
	void    getVertexAttrAsMapChannel(const GA_Attribute &attr, MapChannel &mapChannel);
	int     getMtlOverrides(MapChannels &mapChannels);
	int     getPerPrimMtlOverrides(std::unordered_set< std::string > &o_mapChannelOverrides, std::vector< PrimOverride > &o_primOverrides) const;

private:
	const GU_Detail  &m_gdp;
	OP_Context       &m_context;
	VRayExporter     &m_pluginExporter;
	SOP_Node         *m_sopNode;

	bool              m_hasSubdivApplied;
	int               numFaces;
	int               numMtlIDs;
	VRay::VUtils::VectorRefList vertices;
	VRay::VUtils::VectorRefList normals;
	VRay::VUtils::IntRefList    faces;
	VRay::VUtils::IntRefList    edge_visibility;
	VRay::VUtils::IntRefList    face_mtlIDs;
	MapChannels                 map_channels_data;
};


} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_MESH_H
