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

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_BASE_H
#define VRAY_FOR_HOUDINI_SOP_NODE_BASE_H

#include "vfh_defines.h"
#include "vfh_includes.h"
#include "vfh_class_utils.h"
#include "vfh_prm_json.h"

#include "op/op_node_base.h"

#include <SOP/SOP_Node.h>

#include "vfh_vray.h"


namespace VRayForHoudini {
namespace SOP {

class NodeBase:
		public OP::VRayNode,
		public SOP_Node
{
public:
	NodeBase(OP_Network *parent, const char *name, OP_Operator *entry):SOP_Node(parent, name, entry) {}
	virtual              ~NodeBase() {}

protected:
	virtual OP_ERROR      cookMySop(OP_Context &context)=0;

};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_BASE_H
