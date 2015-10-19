//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
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
