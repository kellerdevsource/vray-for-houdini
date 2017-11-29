//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_BASE_H
#define VRAY_FOR_HOUDINI_SOP_NODE_BASE_H

#include "vfh_defines.h"
#include "vfh_includes.h"
#include "vfh_class_utils.h"

#include "op/op_node_base.h"

#include <SOP/SOP_Node.h>

#include "vfh_vray.h"

namespace VRayForHoudini {
namespace SOP {

/// Base class for vfh custom SOP nodes
class NodeBase:
		public OP::VRayNode,
		public SOP_Node
{
public:
	NodeBase(OP_Network *parent, const char *name, OP_Operator *entry):
		SOP_Node(parent, name, entry)
	{ }
	virtual ~NodeBase()
	{ }

protected:

	/// Houdini callback to cook custom geometry for this node
	/// @note all derived classes are forced to implement this
	/// @param context[in] - cook time
	virtual OP_ERROR cookMySop(OP_Context &context)=0;

};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_BASE_H
