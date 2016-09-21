//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_hou_utils.h"

#include <HOM/HOM_Module.h>
#include <HOM/HOM_EnumModules.h>


int VRayForHoudini::HOU::isApprentice()
{
	return false;
	return (HOM().licenseCategory() == HOM_licenseCategoryType::Apprentice);
}


int VRayForHoudini::HOU::isIndie()
{
	return false;
	return (HOM().licenseCategory() == HOM_licenseCategoryType::Indie);
}


int VRayForHoudini::HOU::isUIAvailable()
{
	return (HOM().isUIAvailable());
}

