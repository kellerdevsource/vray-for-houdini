//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_prm_globals.h"

StringSet  VRayForHoudini::Parm::RenderSettingsPlugins;
StringSet  VRayForHoudini::Parm::RenderGIPlugins;

PRM_Name  VRayForHoudini::Parm::parm_render_net_render_channels("render_network_render_channels", "Render Channels");
PRM_Name  VRayForHoudini::Parm::parm_render_net_environment("render_network_environment", "Environment");

static PRM_Name exporterLogLevelMenuItems[] = {
	PRM_Name("Errors"),
	PRM_Name("Debug"),
	PRM_Name(),
};

PRM_Name       VRayForHoudini::Parm::exporterLogLevelMenuName("exporter_log_level", "Exporter Log Level");
PRM_ChoiceList VRayForHoudini::Parm::exporterLogLevelMenu(PRM_CHOICELIST_SINGLE, exporterLogLevelMenuItems);
