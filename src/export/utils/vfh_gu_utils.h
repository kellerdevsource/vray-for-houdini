//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_GA_UTILS_H
#define VRAY_FOR_HOUDINI_GA_UTILS_H

#include <GU/GU_Detail.h>

namespace VRayForHoudini {
namespace GU {

/// Checks if geometry is hair.
/// @param gdp Geometry detail.
int isHairGdp(const GU_Detail &gdp);

} // namespace GU
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_GA_UTILS_H
