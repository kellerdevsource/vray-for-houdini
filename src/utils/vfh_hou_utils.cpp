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
#include "vfh_includes.h"

#include <HOM/HOM_Module.h>
#include <HOM/HOM_EnumModules.h>
#include <HOM/HOM_ui.h>
#include <RE/RE_Window.h>

int VRayForHoudini::HOU::isApprentice()
{
	return HOM().licenseCategory() == HOM_licenseCategoryType::Apprentice;
}

int VRayForHoudini::HOU::isIndie()
{
	return HOM().licenseCategory() == HOM_licenseCategoryType::Indie;
}

int VRayForHoudini::HOU::isUIAvailable()
{
	return HOM().isUIAvailable();
}

QWidget* VRayForHoudini::HOU::getMainQtWindow()
{
	return RE_Window::mainQtWindow();
}

float VRayForHoudini::HOU::getUiScaling()
{
#if HDK_16_5
	return HOM().ui().globalScaleFactor();
#else
	return 1.0f;
#endif
}
