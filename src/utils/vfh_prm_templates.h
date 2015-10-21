//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_PRM_TEMPLATES_H
#define VRAY_FOR_HOUDINI_PRM_TEMPLATES_H

#include "vfh_prm_defaults.h"

class OP_Node;

namespace VRayForHoudini {
namespace Parm {

int isParmExist(OP_Node &node, const std::string &attrName);
int isParmSwitcher(OP_Node &node, const int index);

const PRM_Parm *getParm(OP_Node &node, const int index);

} // namespace Parm
} // namespace VRayForHoudini

#endif
