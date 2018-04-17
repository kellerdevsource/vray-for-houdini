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

#define EXT_MAPCHANNEL_STRING_CHANNEL_SUPPORT 0

namespace VRayForHoudini {

/// Helper structure to hash float3f vertex attribute values
struct MapVertex {
	MapVertex() = default;

	explicit MapVertex(const UT_Vector3 &vec) {
		v[0] = vec[0];
		v[1] = vec[1];
		v[2] = vec[2];
	}

	bool operator == (const MapVertex &other) const {
		return SYSalmostEqual(v[0],other.v[0]) && SYSalmostEqual(v[1],other.v[1]) && SYSalmostEqual(v[2],other.v[2]);
	}

	float v[3] = { 0.0f }; ///< vertex attribute value
	mutable int index = 0; ///< the index of vertex attribute value
};

/// Helper structure to hash MapVertex
struct MapVertexHash {
	std::size_t operator()(const MapVertex &v) const {
		Hash::MHash hash;
		Hash::MurmurHash3_x86_32(v.v, 3 * sizeof(float), 42, &hash);
		return hash;
	}
};

typedef std::unordered_set<MapVertex, MapVertexHash> VertexSet;

struct CharStringTable
	: QStringList
{
	VRay::VUtils::CharStringRefList toRefList() const;
};

/// Helper structure to wrap relevant map channel properties
struct MapChannel {
	/// Maps string with its index in the strings table.
	typedef QMap<QString, int> StringToTableIndex;

	enum MapChannelType {
		mapChannelTypeVertex = 0,
#if EXT_MAPCHANNEL_STRING_CHANNEL_SUPPORT
		mapChannelTypeString,
#endif
	};

	MapChannelType type = mapChannelTypeVertex;

	/// Vertex array.
	VRay::VUtils::VectorRefList vertices;

	/// Face indices or an array of indices into @c strings array per-face.
	VRay::VUtils::IntRefList faces;

	/// Helper structure to weld vertex attributes.
	VertexSet verticesSet;

	/// String data array.
	CharStringTable strings;

	/// A hash for mapping string with its index in the strings table.
	StringToTableIndex stringToTableIndex;
};

typedef QMap<QString, MapChannel> MapChannels;

typedef UT_ValArray<const GEO_Primitive*> GEOPrimList;
typedef UT_Array<const GA_Attribute*>     GEOAttribList;

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
