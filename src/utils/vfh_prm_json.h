//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_PRM_JSON_H
#define VRAY_FOR_HOUDINI_PRM_JSON_H

#include "vfh_plugin_info.h"

namespace VRayForHoudini {
namespace Parm {

/// Allocate new VRayPluginInfo and fill it based ib plugin info for pluginID
/// @pluginID - the desired ID
/// @return pointer - to newly allocated VRayPluginInfo with filled info for properties
///         nullptr - some fail with loading data or invalid pluginID
VRayPluginInfo *generatePluginInfo(const std::string &pluginID);

} // namespace Parm
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PRM_JSON_H
