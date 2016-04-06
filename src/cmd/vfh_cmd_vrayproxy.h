//
// Copyright (c) 2015-2016, Chaos Software Ltd
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

#include "vfh_vray.h" // For proper "systemstuff.h" inclusion

namespace VRayForHoudini {
namespace CMD {

const tchar* const vrayproxy_format = "m:d:n:tfla::v::P:F:H:T:cix:AN:b";
/*
 * vrayproxy

 * callback function for creating vray proxy geometry
 */
void vrayproxy(CMD_Args &args);

}
}

#endif // VRAY_FOR_HOUDINI_CMD_VRAYPROXY_H
