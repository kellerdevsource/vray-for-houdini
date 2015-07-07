//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
//

#ifndef VRAY_FOR_HOUDINI_MESH_UTILS_H
#define VRAY_FOR_HOUDINI_MESH_UTILS_H

#include <boost/unordered_set.hpp>
#include <UT/UT_Vector3.h>

#include "vfh_vray.h"
#include "vfh_hashes.h"


#define CGR_USE_LIST_RAW_TYPES 0


namespace VRayForHoudini {
namespace Mesh {


struct MapVertex {
	MapVertex() {
		index = 0;
	}

	MapVertex(const UT_Vector3 &vec) {
		MapVertex();

		v[0] = vec[0];
		v[1] = vec[1];
		v[2] = vec[2];
	}

	bool operator == (const MapVertex &_v) const {
		return (v[0] == _v.v[0]) && (v[1] == _v.v[1]) && (v[2] == _v.v[2]);
	}

	float        v[3];
	mutable int  index;
};


struct MapVertexHash
{
	std::size_t operator () (const MapVertex &_v) const {
		VRayForHoudini::Hash::MHash hash;
		VRayForHoudini::Hash::MurmurHash3_x86_32(_v.v, 3 * sizeof(float), 42, &hash);
		return (std::size_t)hash;
	}
};


typedef boost::unordered_set<MapVertex, MapVertexHash> VertexSet;


struct MapChannel {
	std::string           name;
	VertexSet             verticesSet;
#if CGR_USE_LIST_RAW_TYPES
	VUtils::VectorRefList vertices;
	VUtils::IntRefList    faces;
#else
	VRay::VectorList      vertices;
	VRay::IntList         faces;
#endif
};
typedef std::map<std::string, MapChannel> MapChannels;


} // namespace Mesh
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_MESH_UTILS_H
