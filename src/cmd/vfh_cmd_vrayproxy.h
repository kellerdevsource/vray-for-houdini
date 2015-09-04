//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
//

#ifndef VRAY_FOR_HOUDINI_CMD_VRAYPROXY_H
#define VRAY_FOR_HOUDINI_CMD_VRAYPROXY_H

#include <CMD/CMD_Args.h>

#include <systemstuff.h>



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
