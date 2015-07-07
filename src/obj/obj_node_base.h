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

#ifndef VRAY_FOR_HOUDINI_OBJ_NODE_BASE_H
#define VRAY_FOR_HOUDINI_OBJ_NODE_BASE_H

#include "vfh_defines.h"
#include "vfh_includes.h"
#include "vfh_class_utils.h"
#include "vfh_prm_json.h"

#include "op/op_node_base.h"

#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_Light.h>

#include "vfh_vray.h"


namespace VRayForHoudini {
namespace OBJ {

class LightNodeBase:
		public OP::VRayNode,
		public OBJ_Light
{
public:
	LightNodeBase(OP_Network *parent, const char *name, OP_Operator *entry):OBJ_Light(parent, name, entry) {}
	virtual              ~LightNodeBase() {}

};

} // namespace OBJ
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_BASE_H
