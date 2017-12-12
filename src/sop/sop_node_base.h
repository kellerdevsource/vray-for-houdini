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

#include "op/op_node_base.h"

#include <SOP/SOP_Node.h>
#include <OP/OP_Options.h>

class GU_PrimPacked;

namespace VRayForHoudini {
namespace SOP {

/// Base class for custom SOP nodes.
class NodeBase
	: public OP::VRayNode
	, public SOP_Node
{
public:
	NodeBase(OP_Network *parent, const char *name, OP_Operator *entry)
		: SOP_Node(parent, name, entry)
	{}

protected:
	/// Set node time dependent flag based on UI settings.
	virtual void setTimeDependent();
};

/// Base class for custom SOP nodes that generate packed primitives.
class NodePackedBase
	: public NodeBase
{
public:
	NodePackedBase(const char *primType, OP_Network *parent, const char *name, OP_Operator *entry)
		: NodeBase(parent, name, entry)
		, m_primType(primType)
	{}

	// From SOP_Node.
	OP_ERROR cookMySop(OP_Context &context) VRAY_OVERRIDE;

protected:
	/// Setup / update primitive data based.
	/// @param context Cooking context.
	virtual void updatePrimitive(const OP_Context &context);

	/// Update primitive with new options.
	/// @param options New options.
	virtual void updatePrimitiveFromOptions(const OP_Options &options);

	/// Packed primitive type.
	const UT_String m_primType;

	/// Packed primitive instance.
	GU_PrimPacked *m_primPacked = nullptr;

	/// Current options set.
	OP_Options m_primOptions;
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_BASE_H
