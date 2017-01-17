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


#include "vfh_defines.h"

#include <SHOP/SHOP_Node.h>
#include <OP/OP_Director.h>

#include <unordered_set>


namespace VRayForHoudini {


/// SHOPHasher is a helper structure to generate material ids when
/// combining several V-Ray materials into one with MtlMulti
struct SHOPHasher
{
	typedef int   result_type;

	/// Generate mtl id by hashing shop path
	static result_type getSHOPId(const char *shopPath)
	{
		return (UTisstring(shopPath))? UT_StringHolder(shopPath).hash() : 0;
	}

	/// Generate mtl id for a shop node
	/// @param shopNode - pointer to the shop node
	result_type operator()(const SHOP_Node *shopNode) const
	{
		// NOTE: there was a problem with using shop path hash as material id
		// with TexUserScalar as it reads material id from "user_attributes" as
		// floating point number and casts it to int which might result in
		// different id due to precision errors for larger numbers.
		// Currently node unique id is used as identifier however it will be
		// different across different Houdini sessions.
		// TODO: it will be best to use TexUserInt(now available) instead of
		// TexUserScalar in order to use shop path hash as id and make it persistent
		// across Houdini sessions:
		// return (NOT(shopNode))? 0 : getSHOPId(shopNode->getFullPath());
		return (NOT(shopNode))? 0 : shopNode->getUniqueId();
	}

	/// Generate mtl id from shop path
	/// @param shopPath - path to existing shop node
	result_type operator()(const char *shopPath) const
	{
		// return getSHOPId(shopPath);
		SHOP_Node *shopNode = OPgetDirector()->findSHOPNode(shopPath);
		return (NOT(shopNode))? 0 : shopNode->getUniqueId();
	}

	/// Generate mtl id from shop path
	/// @param shopPath - path to existing shop node
	result_type operator()(const std::string &shopPath) const
	{
		// return getSHOPId(shopPath.c_str());
		SHOP_Node *shopNode = OPgetDirector()->findSHOPNode(shopPath.c_str());
		return (NOT(shopNode))? 0 : shopNode->getUniqueId();
	}
};


/// Set of V-Ray shop materials to be combined into a single MtlMulti
typedef std::unordered_set< UT_String , SHOPHasher > SHOPList;


} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H
