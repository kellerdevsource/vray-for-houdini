//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_ga_utils.h"

#include <UT/UT_Version.h>


const char *VRayForHoudini::GA::getGaAttributeName(const GA_Attribute &gaAttr)
{
#if UT_MAJOR_VERSION_INT >= 15
	return gaAttr.getName().buffer();
#else
	return gaAttr.getName();
#endif
}
