//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H
#define VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H

#include "vfh_defines.h"
#include "vfh_prm_def.h"


namespace VRayForHoudini {
namespace Parm {

/// Descriptor for a single VRay plugin
struct VRayPluginInfo {
	VRayPluginInfo() {}

	/// Plugin category type
	PluginType              pluginType;

	/// Attribute description list
	AttributeDescs          attributes;

	/// Node sockets
	SocketsDesc             inputs;
	SocketsDesc             outputs;

	VfhDisableCopy(VRayPluginInfo)
};

typedef std::map<std::string, VRayPluginInfo*> VRayPluginsInfo;

/// Get plugin info for a given pluginID
/// @pluginID - string with the plugin ID
/// @return pointer - to VRayPluginInfo describing the requested plugin
///         nullptr - provided plugin ID was not found
const VRayPluginInfo* GetVRayPluginInfo(const std::string &pluginID);

/// Create new VRayPluginInfo for given pluginID and save it
/// @pluginID - string with the plugin ID
/// @return pointer - to newly created VRayPluginInfo or one previously loaded
VRayPluginInfo* NewVRayPluginInfo(const std::string &pluginID);

} // namespace Parm
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H
