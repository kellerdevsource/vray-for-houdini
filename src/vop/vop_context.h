//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_CONTEXT_H
#define VRAY_FOR_HOUDINI_VOP_CONTEXT_H

#include "vfh_vray.h"

#include <OP/OP_OperatorTable.h>
#include <OP/OP_Network.h>
#include <SHOP/SHOP_Node.h>
#include <VOPNET/VOPNET_Node.h>

namespace VRayForHoudini {
namespace VOP {

struct VRayVOPContextOPFilter
	: OP_OperatorFilter
{
	bool allowOperatorAsChild(OP_Operator *op) VRAY_OVERRIDE;
};

class VRayMaterialBuilder
	: public SHOP_Node
{
public:
	static void register_shop_operator(OP_OperatorTable *table);

	static OP_Node *creator(OP_Network *parent, const char *name, OP_Operator *entry) {
		return new VRayMaterialBuilder(parent, name, entry);
	}

	/// Allow VOPs only as children
	const char *getChildType() const VRAY_OVERRIDE { return VOP_OPTYPE_NAME; }
	OP_OpTypeId getChildTypeID() const VRAY_OVERRIDE { return VOP_OPTYPE_ID; }
	OP_OperatorFilter *getOperatorFilter() VRAY_OVERRIDE { return &m_opFilter; }
	OP_ERROR cookMe(OP_Context &context) VRAY_OVERRIDE;

	VOP_CodeGenerator *getVopCodeGenerator() VRAY_OVERRIDE { return &m_codeGen; }
	bool evalVariableValue(UT_String &value, int index, int thread) VRAY_OVERRIDE;
	bool hasVexShaderParameter(const char *parm_name) VRAY_OVERRIDE;
	void opChanged(OP_EventType reason, void *data) VRAY_OVERRIDE;

protected:
	VRayMaterialBuilder(OP_Network *parent, const char *name, OP_Operator *entry,
	                    SHOP_TYPE shader_type = SHOP_VOP_MATERIAL);
	~VRayMaterialBuilder() {}

	void addNode(OP_Node *node, int notify, int explicitly) VRAY_OVERRIDE;
	void finishedLoadingNetwork(bool is_child_call) VRAY_OVERRIDE;
	void ensureSpareParmsAreUpdatedSubclass() VRAY_OVERRIDE;

	VRayVOPContextOPFilter m_opFilter;
	VOP_CodeGenerator m_codeGen;
};

class VRayVOPContext :
	public VOPNET_Node
{
public:
	static void register_operator_vrayenvcontext(OP_OperatorTable *table);
	static void register_operator_vrayrccontext(OP_OperatorTable *table);

	static OP_Node *creator(OP_Network *parent, const char *name, OP_Operator *entry) {
		return new VRayVOPContext(parent, name, entry);
	}

	OP_OperatorFilter *getOperatorFilter() VRAY_OVERRIDE { return &m_opFilter; }
	VOP_CodeGenerator *getVopCodeGenerator() VRAY_OVERRIDE { return &m_codeGen; }
	bool evalVariableValue(UT_String &value, int index, int thread) VRAY_OVERRIDE;
	bool hasVexShaderParameter(const char *parm_name) VRAY_OVERRIDE;
	void opChanged(OP_EventType reason, void *data) VRAY_OVERRIDE;

protected:
	VRayVOPContext(OP_Network *parent, const char *name, OP_Operator *entry);
	~VRayVOPContext() {}

	void addNode(OP_Node *node, int notify, int explicitly) VRAY_OVERRIDE;
	void finishedLoadingNetwork(bool is_child_call) VRAY_OVERRIDE;
	void ensureSpareParmsAreUpdatedSubclass() VRAY_OVERRIDE;

	VRayVOPContextOPFilter m_opFilter;
	VOP_CodeGenerator m_codeGen;
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_CONTEXT_H
