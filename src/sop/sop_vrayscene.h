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

namespace VRayForHoudini {
namespace SOP {

class VRayScene
	: public NodePackedBase
{
public:
	VRayScene(OP_Network *parent, const char *name, OP_Operator *entry);

protected:
	// From VRayNode.
	void setPluginType() VRAY_OVERRIDE;

	// From NodePackedBase.
	void setTimeDependent() VRAY_OVERRIDE;
	void updatePrimitive(const OP_Context &context) VRAY_OVERRIDE;
	/// Set time dependency of vray scene
	/// @param state[in] - Desired state of time dependency
	void setTimeDependent(bool state);

};

} // namespace SOP
} // namespace VRayForHoudini

#endif // CGR_HAS_VRAYSCENE
#endif // VRAY_FOR_HOUDINI_SOP_NODE_VRAYSCENE_H
