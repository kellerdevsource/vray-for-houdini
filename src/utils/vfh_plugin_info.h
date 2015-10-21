//
// Copyright (c) 2015, Chaos Software Ltd
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

struct VRayPluginInfo {
	VRayPluginInfo() {}

	// Plugin category type
	PluginType              pluginType;

	// Attribute description list
	AttributeDescs          attributes;

	// Node sockets
	SocketsDesc             inputs;
	SocketsDesc             outputs;

	VfhDisableCopy(VRayPluginInfo)
};

typedef std::map<std::string, VRayPluginInfo*> VRayPluginsInfo;

VRayPluginInfo* GetVRayPluginInfo(const std::string &pluginID);
VRayPluginInfo* NewVRayPluginInfo(const std::string &pluginID);

} // namespace Parm
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H
