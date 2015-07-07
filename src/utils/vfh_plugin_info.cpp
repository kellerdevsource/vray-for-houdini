//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
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
