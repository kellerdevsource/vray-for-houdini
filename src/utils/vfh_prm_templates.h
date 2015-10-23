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

#include "vfh_plugin_info.h"
#include "vfh_prm_defaults.h"

#include <PRM/PRM_Parm.h>


class OP_Node;

namespace VRayForHoudini {
namespace Parm {

int isParmExist(const OP_Node &node, const std::string &attrName);
int isParmSwitcher(const OP_Node &node, const int index);

const PRM_Parm *getParm(const OP_Node &node, const int index);
const PRM_Parm *getParm(const OP_Node &node, const std::string &attrName);

int    getParmInt(const OP_Node &node, const std::string &attrName, fpreal t=0.0);
float  getParmFloat(const OP_Node &node, const std::string &attrName, fpreal t=0.0);

int    getParmEnumExt(const OP_Node &node, const AttrDesc &attrDesc, const std::string &attrName, fpreal t=0.0);

} // namespace Parm
} // namespace VRayForHoudini

#endif
