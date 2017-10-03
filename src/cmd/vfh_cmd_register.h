//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_CMD_REGISTER_H
#define VRAY_FOR_HOUDINI_CMD_REGISTER_H

#include <CMD/CMD_Manager.h>

namespace VRayForHoudini{
namespace CMD {

/// Register custom hscript commands
/// @note for more info on how to extend hscript see: http://archive.sidefx.com/docs/hdk15.5/_h_d_k__extend_c_m_d.html
/// @param[in] cman - Houdini command manager
/// @retval no return val
void RegisterCommands(CMD_Manager *cman);

}
}

#endif // VRAY_FOR_HOUDINI_CMD_REGISTER_H
