//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_GA_UTILS_H
#define VRAY_FOR_HOUDINI_GA_UTILS_H

#include "vop_node_base.h"

#include <GA/GA_Attribute.h>


namespace VRayForHoudini {
namespace GA {

const char *getGaAttributeName(const GA_Attribute &gaAttr);

} // namespace GA
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_GA_UTILS_H
