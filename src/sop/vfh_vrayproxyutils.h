//
// Copyright (c) 2015-2018, Chaos Software Ltd
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

namespace VRayForHoudini {

enum class VRayProxyObjectType {
	none = -1,
	geometry = 0,
	hair,
	particles,
};

VRayProxyObjectType objectInfoIdToObjectType(int channelID);

int objectTypeToObjectInfoId(VRayProxyObjectType objectType);

/// Level of detail types for viewport display of .vrmesh geometry
/// Proxy cache manager will load and cache only the data needed
/// to display the corresponding level of detail
enum class VRayProxyPreviewType {
	bbox = 0, ///< Load and display bbox of the geometry.
	preview, ///< Load and display geometry in the preview voxel.
	full, ///< Load and display full geometry.
};

/// Proxy cache key.
struct VRayProxyRefKey {
	VUtils::CharString filePath;
	VUtils::CharString objectPath;
	VRayProxyObjectType objectType = VRayProxyObjectType::none;
	int objectID;
	VRayProxyPreviewType lod = VRayProxyPreviewType::preview;
	fpreal f = 0;
	int animType = 0;
	fpreal64 animOffset = 0;
	fpreal64 animSpeed = 1.0;
	bool animOverride = false;
	int animStart = 0;
	int animLength = 100;
	int previewFaces = 10000;

	uint32 hash() const;

	bool operator <(const VRayProxyRefKey &other) const {
		return hash() < other.hash();
	}

	/// Checks for difference between two instances,
	/// without taking frame into account
	bool isDifferent(const VRayProxyRefKey &other) const {
		return hash() != other.hash();
	}
};

/// VRayProxy cache item.
struct VRayProxyRefItem {
	/// Preview detail.
	GU_DetailHandle gdp;

	/// Bounding box.
	UT_BoundingBox bbox;
};

/// Get detail handle for a proxy packed primitive.
/// @note internally this will go through the proxy cache manager
/// @param[in] options - UT_Options for proxy packed primitive
/// @retval the detail handle for the primitive (based on file,
///         frame, LOD in options)
VRayProxyRefItem getVRayProxyDetail(const VRayProxyRefKey &options);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAYPROXYCACHE_H
