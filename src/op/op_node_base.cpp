//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "op_node_base.h"

using namespace VRayForHoudini;
using namespace Parm;

void OP::VRayNode::init()
{
	setPluginType();

	pluginInfo = Parm::getVRayPluginInfo(pluginID.c_str());
}
