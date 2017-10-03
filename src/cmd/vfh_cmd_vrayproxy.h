//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_CMD_VRAYPROXY_H
#define VRAY_FOR_HOUDINI_CMD_VRAYPROXY_H

#include <CMD/CMD_Args.h>


namespace VRayForHoudini {
namespace CMD {

/// Option string containing command line arguments for vrayproxy hscript command
/// format is similar to the option parsing string of the standard C function getopt.
/// @note see vfh_home/help/command.help file or type 'help vrayproxy' in textport
///       for detailed info on vrayproxy usage and command line options.
///       Pls update the help file accordingly if args format changes in future.
const char* const vrayproxyFormat = "n:cfmia::ltv::T:F:H:X:P:";

/// Callback function for creating V-Ray proxy geometry - called when vrayproxy hscript command is called
/// @param[in] args - command arguments in the format above
/// @retval no return val
void vrayproxy(CMD_Args &args);

}
}

#endif // VRAY_FOR_HOUDINI_CMD_VRAYPROXY_H
