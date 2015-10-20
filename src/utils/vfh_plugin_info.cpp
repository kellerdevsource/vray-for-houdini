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
#include "vfh_prm_json.h"

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
	VRayPluginInfo *pInfo = nullptr;

	if (pluginsInfo.count(pluginID)) {
		pInfo = pluginsInfo[pluginID];
	}
	else {
		VRayPluginInfo *pluginInfo = Parm::generatePluginInfo(pluginID);
		pluginsInfo[pluginID] = pluginInfo;
		pInfo = pluginInfo;
	}

	return pInfo;
}
