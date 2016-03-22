//
// Copyright (c) 2015, Chaos Software Ltd
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

#include <unordered_set>


namespace VRayForHoudini {


struct SHOPHasher
{
	typedef int   result_type;

	static result_type getSHOPId(const char *shopPath)
	{
		return (UTisstring(shopPath))? UT_StringHolder(shopPath).hash() : 0;
	}

	result_type operator()(const SHOP_Node *shopNode) const
	{
		return (NOT(shopNode))? 0 : shopNode->getUniqueId();
		// there is a problem with using shop path hash as material id
		// TexUserScalar reads material id from "user_attributes "as float
		// and then casts it to int which results in a different id
		// TODO: need to fix that
		// return (NOT(shopNode))? 0 : getSHOPId(shopNode->getFullPath());
	}

	result_type operator()(const char *shopPath) const
	{
		return getSHOPId(shopPath);
	}

	result_type operator()(const std::string &shopPath) const
	{
		return getSHOPId(shopPath.c_str());
	}
};


typedef std::unordered_set< UT_String , SHOPHasher > SHOPList;


} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H
