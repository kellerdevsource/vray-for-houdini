#include "cmd/vfh_cmd_register.h"
#include "cmd/vfh_cmd_vrayproxy.h"

using namespace VRayForHoudini;

void CMD::RegisterCommands(CMD_Manager *cman)
{
	cman->installCommand("vrayproxy", CMD::vrayproxy_format, CMD::vrayproxy);
}
