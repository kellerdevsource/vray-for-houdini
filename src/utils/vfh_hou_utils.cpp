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
#include <UT/UT_Version.h>


#if UT_MAJOR_VERSION_INT >= 16
#include <RE/RE_Window.h>
#else
#include <RE/RE_QtWindow.h>
#endif


int VRayForHoudini::HOU::isApprentice()
{
	return (HOM().licenseCategory() == HOM_licenseCategoryType::Apprentice);
}


int VRayForHoudini::HOU::isIndie()
{
	return (HOM().licenseCategory() == HOM_licenseCategoryType::Indie);
}


int VRayForHoudini::HOU::isUIAvailable()
{
	return (HOM().isUIAvailable());
}


QWidget* VRayForHoudini::HOU::getMainQtWindow()
{
#if UT_MAJOR_VERSION_INT >= 16
	return RE_Window::mainQtWindow();
#else
	return RE_QtWindow::mainQtWindow();
#endif
}
