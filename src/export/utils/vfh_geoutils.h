//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_GEOUTILS_H
#define VRAY_FOR_HOUDINI_GEOUTILS_H

#include "vfh_vray.h"
#include "vfh_hashes.h"

#include <GEO/GEO_Primitive.h>

#include <unordered_set>
#include <unordered_map>


namespace VRayForHoudini {

struct MapVertex {
	MapVertex():
		index(0)
	{ }
	MapVertex(const UT_Vector3 &vec):
		index(0)
	{
		v[0] = vec[0]; v[1] = vec[1]; v[2] = vec[2];
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
	inline std::size_t operator () (const MapVertex &_v) const
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


typedef UT_ValArray< const GEO_Primitive* > GEOPrimList;
typedef UT_Array< const GA_Attribute * >    GEOAttribList;

GA_AttributeFilter& GEOgetV3AttribFilter();

bool GEOgetDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
							 VRay::VUtils::IntRefList &data);

bool GEOgetDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
							 VRay::VUtils::FloatRefList &data);

bool GEOgetDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
							 VRay::VUtils::VectorRefList &data);

bool GEOgetDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
							 VRay::VUtils::ColorRefList &data);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_GEOUTILS_H
