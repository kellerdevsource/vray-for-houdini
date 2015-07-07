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

#ifndef VRAY_FOR_HOUDINI_PRM_JSON_H
#define VRAY_FOR_HOUDINI_PRM_JSON_H

#include <PRM/PRM_Template.h>

namespace VRayForHoudini {
namespace Parm {

PRM_Template *GeneratePrmTemplate(const std::string &pluginType, const std::string &pluginID, const bool &useWidget=false, const bool &prefix=false, const std::string &pluginPrefix="");

} // namespace Parm
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PRM_JSON_H
