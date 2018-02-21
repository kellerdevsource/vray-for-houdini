

//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_BASE_H
#define VRAY_FOR_HOUDINI_VOP_NODE_BASE_H

#include "op/op_node_base.h"

#include <OP/OP_Input.h>
#include <VOP/VOP_Node.h>

namespace VRayForHoudini {
namespace VOP {

class NodeBase
	: public VOP_Node
	, public OP::VRayNode
{
public:
	NodeBase(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual ~NodeBase() {}

	/// Disable our parameters based on which inputs are connected.
	bool updateParmsFlags() VRAY_OVERRIDE;

	/// Generate the code for this operator.
	/// NOTE: Not called for me, find out why and if it actually affect viewport
	void getCode(UT_String &codestr, const VOP_CodeGenContext &context) VRAY_OVERRIDE {}

	/// Provides the labels to appear on input and output buttons.
	const char *inputLabel(unsigned idx) const VRAY_OVERRIDE;
	const char *outputLabel(unsigned idx) const VRAY_OVERRIDE;

	/// Number of input connectors that should be visible on a node. This is
	/// only used by VOPs and DOPs. All other OPs return maxInputs() here.
	unsigned getNumVisibleInputs() const VRAY_OVERRIDE;

	/// Number of output connections that should be visible on a node tile.
	/// Only used by VOPs and DOPs.
	unsigned getNumVisibleOutputs() const VRAY_OVERRIDE;

	/// Returns the index of the first "unordered" input, meaning that inputs
	/// before this one should not be collapsed when they are disconnected.
	unsigned orderedInputs() const VRAY_OVERRIDE;

	unsigned maxOutputs() const VRAY_OVERRIDE;

	VOP_Type getShaderType() const VRAY_OVERRIDE;

protected:
	/// Returns the internal name of the supplied input. This input name is
	/// used when the code generator substitutes variables return by getCode.
	void getInputNameSubclass(UT_String &in, int idx) const VRAY_OVERRIDE;

	/// Reverse mapping of internal input names to an input index.
	int getInputFromNameSubclass(const UT_String &in) const VRAY_OVERRIDE;

	/// Fills in the info about the vop type connected to the idx-th input.
	void getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;

	/// Returns the internal name of the supplied output. This name is used
	/// when the code generator substitutes variables return by getCode.
	void getOutputNameSubclass(UT_String &out, int idx) const VRAY_OVERRIDE;
	int getOutputFromName(const UT_String &out) const VRAY_OVERRIDE;

	/// Fills out the info about the data type of each output connector.
	void getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;

	/// This methods should be overridden in most VOP nodes. It should fill
	/// in voptypes vector with vop types that are allowed to be connected
	/// to this node at the input at index idx. Note that
	/// this method should return valid vop types even when nothing is
	/// connected to the corresponding input.
	void getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos) VRAY_OVERRIDE;
	void getAllowedInputTypesSubclass(unsigned idx, VOP_VopTypeArray &voptypes) VRAY_OVERRIDE;
	bool willAutoconvertInputType(int input_idx) VRAY_OVERRIDE;

	/// Returns labed for the soket.
	/// @param socketIndex 0 based socket index.
	/// @param nameFormat Label format.
	const char *getCreateSocketLabel(int socketIndex, const char *nameFormat) const;

private:
	/// Storage for dynamically generated socket labels.
	mutable UT_StringArray socketLabels;

	bool                      hasPluginInfo() const;

}; // NodeBase


#define NODE_BASE_DEF(OpPluginType, OpPluginID) \
class OpPluginID: \
		public NodeBase \
{ \
public: \
	OpPluginID(OP_Network *parent, const char *name, OP_Operator *entry) \
		: NodeBase(parent, name, entry) \
	{} \
	virtual ~OpPluginID() {} \
protected: \
	void setPluginType() VRAY_OVERRIDE { \
		pluginType = VRayPluginType::OpPluginType; \
		pluginID = STRINGIZE(OpPluginID); \
		if (pluginType == VRayPluginType::BRDF || \
			pluginType == VRayPluginType::MATERIAL) \
		{ \
			setMaterialFlag(true); \
		} \
	} \
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_BASE_H
