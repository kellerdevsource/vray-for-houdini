//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "cmd/vfh_cmd_register.h"
#include "cmd/vfh_cmd_vrayproxy.h"

using namespace VRayForHoudini;

void CMD::RegisterCommands(CMD_Manager *cman)
{
	cman->installCommand("vrayproxy", CMD::vrayproxy_format, CMD::vrayproxy);
}
