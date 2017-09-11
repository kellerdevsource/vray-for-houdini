//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
	VRayPluginsInfo::iterator pInfo = pluginsInfo.find(pluginID);
	if (pInfo != pluginsInfo.end()) {
		return pInfo->second;
	}
	VRayPluginInfo *pluginInfo = new VRayPluginInfo();
	pluginsInfo[pluginID] = pluginInfo;
	return pluginInfo;
}


const VRayPluginInfo* VRayForHoudini::Parm::GetVRayPluginInfo(const std::string &pluginID)
{
	VRayPluginInfo *pInfo = nullptr;

	VRayPluginsInfo::iterator pluginInfoIter = pluginsInfo.find(pluginID);
	if (pluginInfoIter != pluginsInfo.end()) {
		pInfo = pluginInfoIter->second;
	}
	else {
		VRayPluginInfo *pluginInfo = Parm::generatePluginInfo(pluginID);
		// don't save nullptrs inside, we have no use for them
		if (pluginInfo) {
			pluginsInfo[pluginID] = pluginInfo;
			pInfo = pluginInfo;
		}
	}

	return pInfo;
}
