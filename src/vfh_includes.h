//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_INCLUDES_H
#define VRAY_FOR_HOUDINI_INCLUDES_H

#include <UT/UT_Version.h>
#include <UT/UT_Interrupt.h>

#if UT_MAJOR_VERSION_INT >= 16 && UT_MINOR_VERSION_INT >= 0 && UT_BUILD_VERSION_INT >= 633
#define HDK_16_0_633
#endif

#if UT_MAJOR_VERSION_INT >= 16 && UT_MINOR_VERSION_INT >= 5
#define HDK_16_5
#endif

#endif // VRAY_FOR_HOUDINI_INCLUDES_H
