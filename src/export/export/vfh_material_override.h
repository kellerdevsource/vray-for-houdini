//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H
#define VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H

#include "vfh_attr_utils.h"

#include <unordered_set>

namespace VRayForHoudini {

/// SHOPHasher is a helper structure to generate material IDs when
/// combining several V-Ray materials into MtlMulti.
struct SHOPHasher
{
	typedef int result_type;

	/// Generate mtl id by hashing shop path
	static result_type getSHOPId(const char *shopPath) {
		return (UTisstring(shopPath)) ? UT_StringHolder(shopPath).hash() : 0;
	}

	/// Generate mtl id for a shop node
	/// @param opNode OP_Node instance.
	result_type operator()(const OP_Node *opNode) const {
		// NOTE: there was a problem with using shop path hash as material id
		// with TexUserScalar as it reads material id from "user_attributes" as
		// floating point number and casts it to int which might result in
		// different id due to precision errors for larger numbers.
		// Currently node unique id is used as identifier however it will be
		// different across different Houdini sessions.
		// TODO: it will be best to use TexUserInt(now available) instead of
		// TexUserScalar in order to use shop path hash as id and make it persistent
		// across Houdini sessions.
		return opNode ? opNode->getUniqueId() : 0;
	}

	/// Generate material ID from path.
	/// @param path Node path.
	result_type operator()(const char *path) const {
		if (UTisstring(path)) {
			UT_String opPath(path);
			const OP_Node *opNode = getOpNodeFromPath(opPath);
			if (opNode) {
				return opNode->getUniqueId();
			}
		}
		return 0;
	}

	/// Generate material ID from path.
	/// @param path Node path.
	result_type operator()(const std::string &path) const {
		UT_String opPath(path);
		const OP_Node *opNode = getOpNodeFromPath(opPath);
		return opNode ? opNode->getUniqueId() : 0;
	}

	/// Generate material ID from path.
	/// @param path Node path.
	result_type operator()(const UT_String &path) const {
		const OP_Node *opNode = getOpNodeFromPath(path);
		return opNode ? opNode->getUniqueId() : 0;
	}
};

/// Set of V-Ray shop materials to be combined into a single MtlMulti
typedef std::unordered_set<UT_String , SHOPHasher> SHOPList;

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H
