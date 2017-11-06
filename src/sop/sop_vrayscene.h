//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_VRAYSCENE_H
#define VRAY_FOR_HOUDINI_SOP_NODE_VRAYSCENE_H
#ifdef CGR_HAS_VRAYSCENE

#include "sop_node_base.h"

#include <vrscene_preview.h>

namespace VRayForHoudini {
namespace SOP {

class VRayScene
	: public NodeBase
{
public:
	VRayScene(OP_Network *parent, const char *name, OP_Operator *entry)
		: NodeBase(parent, name, entry)
	{}

	/// Houdini callback to cook custom geometry for this node.
	/// @param context Cooking context.
	OP_ERROR cookMySop(OP_Context &context) VRAY_OVERRIDE;

protected:
	/// Set custom plugin id and type for this node.
	void setPluginType() VRAY_OVERRIDE;
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // CGR_HAS_VRAYSCENE
#endif // VRAY_FOR_HOUDINI_SOP_NODE_VRAYSCENE_H
