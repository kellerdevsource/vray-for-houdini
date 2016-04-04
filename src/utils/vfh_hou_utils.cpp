//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_hou_utils.h"


int VRayForHoudini::HOU::isApprentice()
{
	return (HOM().licenseCategory() == HOM_licenseCategoryType::Apprentice);
}


int VRayForHoudini::HOU::isIndie()
{
	return (HOM().licenseCategory() == HOM_licenseCategoryType::Indie);
}
