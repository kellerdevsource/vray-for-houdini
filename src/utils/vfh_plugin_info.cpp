//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_plugin_info.h"


using namespace VRayForHoudini;
using namespace VRayForHoudini::Parm;


static VRayPluginsInfo pluginsInfo;


VRayPluginInfo* VRayForHoudini::Parm::NewVRayPluginInfo(const std::string &pluginID)
{
	VRayPluginInfo *pluginInfo = new VRayPluginInfo();
	pluginsInfo[pluginID] = pluginInfo;
	return pluginInfo;
}


VRayPluginInfo* VRayForHoudini::Parm::GetVRayPluginInfo(const std::string &pluginID)
{
	if (pluginsInfo.count(pluginID)) {
		return pluginsInfo[pluginID];
	}
	return nullptr;
}


const AttrDesc *VRayPluginInfo::getAttributeDesc(const std::string &attrName) const
{
	for (const auto &aIt : attributes) {
		if (attrName == aIt.first) {
			return &aIt.second;
		}
	}
	return nullptr;
}
