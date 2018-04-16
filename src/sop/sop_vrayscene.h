//
// Copyright (c) 2015-2018, Chaos Software Ltd
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

struct PrimWithOptions
{
	GU_PrimPacked *prim{nullptr};
	OP_Options options;
};

typedef UT_ValArray<PrimWithOptions> PrimWithOptionsList;

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
	void getCreatePrimitive() VRAY_OVERRIDE;
	void updatePrimitiveFromOptions(const OP_Options &options) VRAY_OVERRIDE;
	void updatePrimitive(const OP_Context &context) VRAY_OVERRIDE;

	/// Packed primitives list.
	PrimWithOptionsList prims;
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // CGR_HAS_VRAYSCENE
#endif // VRAY_FOR_HOUDINI_SOP_NODE_VRAYSCENE_H
