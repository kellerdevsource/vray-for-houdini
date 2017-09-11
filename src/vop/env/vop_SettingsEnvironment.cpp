//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_SettingsEnvironment.h"


using namespace VRayForHoudini;


void VOP::SettingsEnvironment::setPluginType()
{
	pluginType = VRayPluginType::SETTINGS;
	pluginID   = "SettingsEnvironment";
}


OP::VRayNode::PluginResult VOP::SettingsEnvironment::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	// TODO: Effects
	//
	return PluginResult::PluginResultNA;
}
