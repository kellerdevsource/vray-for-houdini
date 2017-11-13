//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_SOP_PGYETI_H
#define VRAY_FOR_HOUDINI_SOP_PGYETI_H

#include "sop_node_base.h"

#include <OP/OP_Options.h>

namespace VRayForHoudini {
namespace SOP {

class VRayPgYeti
	: public NodeBase
{
public:
	VRayPgYeti(OP_Network *parent, const char *name, OP_Operator *entry)
		: NodeBase(parent, name, entry)
	{}

	// From SOP_Node.
	OP_ERROR cookMySop(OP_Context &context) VRAY_OVERRIDE;

protected:
	// From VRayNode.
	void setPluginType() VRAY_OVERRIDE;

private:
	/// Set node time dependent flag based on UI settings.
	void setTimeDependent();

	/// Setup / update primitive data based.
	/// @param context Cooking context.
	void updatePrimitive(const OP_Context &context);

	/// Packed primitive with the preview data.
	GU_PrimPacked *m_primPacked{nullptr};

	/// Current options set.
	OP_Options m_primOptions;

	/// Show preview mesh animation.
	int previewMeshAnimated{0};
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_PGYETI_H
