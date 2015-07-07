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

#include "op_node_base.h"


using namespace VRayForHoudini;
using namespace Parm;


void OP::VRayNode::init()
{
	setPluginType();

	if (NOT(pluginID.empty())) {
		pluginInfo = Parm::GetVRayPluginInfo(pluginID);
	}
}
