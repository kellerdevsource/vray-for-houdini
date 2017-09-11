//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_cmd_register.h"
#include "vfh_cmd_vrayproxy.h"

using namespace VRayForHoudini;

void CMD::RegisterCommands(CMD_Manager *cman)
{
	cman->installCommand("vrayproxy", CMD::vrayproxyFormat, CMD::vrayproxy);
}
