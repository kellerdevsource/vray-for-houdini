//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
#include <SYS/SYS_Math.h>

#include <unordered_set>
#include <unordered_map>


namespace VRayForHoudini {

/// Helper structure to hash float3f vertex attribute values
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
		return SYSalmostEqual(v[0],_v.v[0]) && SYSalmostEqual(v[1],_v.v[1]) && SYSalmostEqual(v[2],_v.v[2]);
	}

	float        v[3]; ///< vertex attribute value
	mutable int  index; ///< the index of vertex attribute value
};


/// Helper structure to hash MapVertex
struct MapVertexHash
{
	inline std::size_t operator () (const MapVertex &_v) const
	{
		VRayForHoudini::Hash::MHash hash;
		VRayForHoudini::Hash::MurmurHash3_x86_32(_v.v, 3 * sizeof(float), 42, &hash);
		return (std::size_t)hash;
	}
};


/// Helper structure to wrap relevant map channel properties
struct MapChannel
{
	typedef std::unordered_set<MapVertex, MapVertexHash> VertexSet;

	std::string                 name; ///< map channels name
	VRay::VUtils::VectorRefList vertices; ///< map channel vertex data
	VRay::VUtils::IntRefList    faces; ///< map channel face indices
	VertexSet                   verticesSet; ///< helper to weld vertex attributes
};

typedef std::unordered_map<std::string, MapChannel> MapChannels;

typedef UT_ValArray< const GEO_Primitive* > GEOPrimList;
typedef UT_Array< const GA_Attribute * >    GEOAttribList;

/// Get the commonly used float3f attribute filter
GA_AttributeFilter& GEOgetV3AttribFilter();

/// Copy data from an int attribute to a VRay int list for a given
/// primitive list
/// @param attr[in] - the attribute. It can be either vertex, point
///        or primitive attribute. In case of a point attribute values
///        referenced multiple times will be copied multiple times in
///        the output list
/// @param primList[in] - the primitive list
/// @param data[out] - output data list of integer values
/// @retval true if successful
bool GEOgetDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
							 VRay::VUtils::IntRefList &data);

/// Copy data from an float attribute to a VRay float list for a given
/// primitive list
/// @param attr[in] - the attribute. It can be either vertex, point
///        or primitive attribute. In case of a point attribute values
///        referenced multiple times will be copied multiple times in
///        the output list
/// @param primList[in] - the primitive list
/// @param data[out] - output data list of float values
/// @retval true if successful
bool GEOgetDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
							 VRay::VUtils::FloatRefList &data);

/// Copy data from an float3f attribute to a VRay vector list for a given
/// primitive list
/// @param attr[in] - the attribute. It can be either vertex, point
///        or primitive attribute. In case of a point attribute values
///        referenced multiple times will be copied multiple times in
///        the output list
/// @param primList[in] - the primitive list
/// @param data[out] - output data list of vector values
/// @retval true if successful
bool GEOgetDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
							 VRay::VUtils::VectorRefList &data);

/// Copy data from an float3f attribute to a VRay color list for a given
/// primitive list
/// @param attr[in] - the attribute. It can be either vertex, point
///        or primitive attribute. In case of a point attribute values
///        referenced multiple times will be copied multiple times in
///        the output list
/// @param primList[in] - the primitive list
/// @param data[out] - output data list of color values
/// @retval true if successful
bool GEOgetDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
							 VRay::VUtils::ColorRefList &data);

exint getGEOPrimListHash(const GEOPrimList &primList);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_GEOUTILS_H
