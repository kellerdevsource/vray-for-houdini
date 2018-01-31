//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAYPROXYCACHE_H
#define VRAY_FOR_HOUDINI_VRAYPROXYCACHE_H

#include <GU/GU_DetailHandle.h>
#include <UT/UT_BoundingBox.h>
#include <UT/UT_String.h>

#include <charstring.h>
#include <vfh_defines.h>
#include "mesh_file.h"

namespace VRayForHoudini {

/// Level of detail types for viewport display of .vrmesh geometry
/// Proxy cache manager will load and cache only the data needed
/// to display the corresponding level of detail
enum LOD {
	LOD_BBOX = 0, ///< Load and display bbox of the geometry
	LOD_PREVIEW, ///< Load and display geometry in the preview voxel
	LOD_FULL, ///< Load and display full geometry
};

/// Proxy cache key.
struct VRayProxyRefKey {
	VUtils::CharString filePath;
	LOD lod = LOD_PREVIEW;
	fpreal f = 0;
	int animType = 0;
	fpreal64 animOffset = 0;
	fpreal64 animSpeed = 1.0;
	bool animOverride = false;
	int animStart = 0;
	int animLength = 100;

	bool operator== (const VRayProxyRefKey & other) const {
		return (filePath == other.filePath &&
			lod == other.lod &&
			MemberFloatEq(f, other.f) &&
			animType == other.animType &&
			MemberFloatEq(animOffset, other.animOffset) &&
			MemberFloatEq(animSpeed, other.animSpeed) &&
			animOverride == other.animOverride &&
			animStart == other.animStart &&
			animLength == other.animLength);
	}

	bool operator!=(const VRayProxyRefKey &other) const {
		return !(*this == other);
	}

	/// Checks for difference between two instances,
	/// without taking frame into account
	bool differingSettings(const VRayProxyRefKey &other) const {
		return !(filePath == other.filePath &&
			lod == other.lod &&
			animType == other.animType &&
			MemberFloatEq(animOffset, other.animOffset) &&
			MemberFloatEq(animSpeed, other.animSpeed) &&
			animOverride == other.animOverride &&
			animStart == other.animStart &&
			animLength == other.animLength);
	}
};

/// Get detail handle for a proxy packed primitive.
/// @note internally this will go through the proxy cache manager
/// @param[in] options - UT_Options for proxy packed primitive
/// @retval the detail handle for the primitive (based on file,
///         frame, LOD in options)
GU_DetailHandle getVRayProxyDetail(const VRayProxyRefKey &options);

/// Get the bounding box for a proxy packed primitive.
/// @note internally this will go through the proxy cache manager
///       bbox is cached on first read access for detail handle
/// @param[in] options - UT_Options for proxy packed primitive
/// @param[out] box - the bounding box for all geometry
/// @retval true if successful i.e this data is found cached
bool getVRayProxyBoundingBox(const VRayProxyRefKey &options, UT_BoundingBox &box);

/// Clear cached data for a given file.
/// @param filepath File path.
/// @retval true if cached file is deleted.
bool clearVRayProxyCache(const char *filepath);
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAYPROXYCACHE_H
