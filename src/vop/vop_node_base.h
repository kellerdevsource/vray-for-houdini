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

#include "vfh_defines.h"
#include "vfh_class_utils.h"

#include "op/op_node_base.h"

#include <OP/OP_Input.h>
#include <OP/OP_OperatorTable.h>

#include <VOP/VOP_Node.h>
#include <VOP/VOP_Operator.h>

#include <UT/UT_NTStreamUtil.h>


namespace VRayForHoudini {
namespace VOP {

class NodeBase:
		public VOP_Node,
		public OP::VRayNode
{
public:
	NodeBase(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual                  ~NodeBase() {}

	/// Disable our parameters based on which inputs are connected.
	virtual bool              updateParmsFlags() VRAY_OVERRIDE;

	/// Generate the code for this operator.
	/// NOTE: Not called for me, find out why and if it actually affect viewport
	virtual void              getCode(UT_String &codestr, const VOP_CodeGenContext &context) VRAY_OVERRIDE {}

	/// Provides the labels to appear on input and output buttons.
	virtual const char       *inputLabel(unsigned idx) const VRAY_OVERRIDE;
	virtual const char       *outputLabel(unsigned idx) const VRAY_OVERRIDE;

	/// Number of input connectors that should be visible on a node. This is
	/// only used by VOPs and DOPs. All other OPs return maxInputs() here.
	virtual unsigned          getNumVisibleInputs() const VRAY_OVERRIDE;

	/// Number of output connections that should be visible on a node tile.
	/// Only used by VOPs and DOPs.
	virtual unsigned          getNumVisibleOutputs() const VRAY_OVERRIDE;

	/// Returns the index of the first "unordered" input, meaning that inputs
	/// before this one should not be collapsed when they are disconnected.
	virtual unsigned          orderedInputs() const VRAY_OVERRIDE;

	virtual unsigned          maxOutputs() const VRAY_OVERRIDE;

	VOP_Type getShaderType() const VRAY_OVERRIDE;

protected:
	/// Returns the internal name of the supplied input. This input name is
	/// used when the code generator substitutes variables return by getCode.
	virtual void              getInputNameSubclass(UT_String &in, int idx) const VRAY_OVERRIDE;

	/// Reverse mapping of internal input names to an input index.
	virtual int               getInputFromNameSubclass(const UT_String &in) const VRAY_OVERRIDE;

	/// Fills in the info about the vop type connected to the idx-th input.
	virtual void              getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;

	/// Returns the internal name of the supplied output. This name is used
	/// when the code generator substitutes variables return by getCode.
	virtual void              getOutputNameSubclass(UT_String &out, int idx) const VRAY_OVERRIDE;
	virtual int               getOutputFromName(const UT_String &out) const VRAY_OVERRIDE;

	/// Fills out the info about the data type of each output connector.
	virtual void              getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;

	/// This methods should be overridden in most VOP nodes. It should fill
	/// in voptypes vector with vop types that are allowed to be connected
	/// to this node at the input at index idx. Note that
	/// this method should return valid vop types even when nothing is
	/// connected to the corresponding input.
	virtual void              getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos) VRAY_OVERRIDE;
	void getAllowedInputTypesSubclass(unsigned idx, VOP_VopTypeArray &voptypes) VRAY_OVERRIDE;
	bool willAutoconvertInputType(int input_idx) VRAY_OVERRIDE;

private:
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
	virtual void setPluginType() VRAY_OVERRIDE { \
		pluginType = VRayPluginType::OpPluginType; \
		pluginID = STRINGIZE(OpPluginID); \
	} \
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_BASE_H
