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

#include <vfh_vray.h>

#include <OP/OP_OperatorTable.h>
#include <OP/OP_Network.h>
#include <SHOP/SHOP_Node.h>
#include <VOPNET/VOPNET_Node.h>

#include <unordered_set>

namespace VRayForHoudini {
namespace VOP {


class VRayVOPContextOPFilter:
		public OP_OperatorFilter
{
public:
	VRayVOPContextOPFilter();
	virtual bool allowOperatorAsChild(OP_Operator *op) VRAY_OVERRIDE;

private:
	std::unordered_set<std::string> m_allowedVOPs;
};


class VRayMaterialBuilder:
		public SHOP_Node
{
public:
	static void                register_shop_operator(OP_OperatorTable *table);
	static OP_Node            *creator(OP_Network *parent, const char *name, OP_Operator *entry) { return new VRayMaterialBuilder(parent, name, entry); }

public:
/// Allow VOPs only as children
	virtual const char        *getChildType() const VRAY_OVERRIDE { return VOP_OPTYPE_NAME; }
	virtual OP_OpTypeId        getChildTypeID() const VRAY_OVERRIDE { return VOP_OPTYPE_ID; }
	virtual OP_OperatorFilter *getOperatorFilter() VRAY_OVERRIDE{ return &m_opFilter; }
	virtual OP_ERROR           cookMe(OP_Context &context) VRAY_OVERRIDE;

	virtual VOP_CodeGenerator *getVopCodeGenerator() VRAY_OVERRIDE { return &m_codeGen; }
	virtual bool               evalVariableValue(UT_String &value, int index, int thread) VRAY_OVERRIDE;
	virtual bool               hasVexShaderParameter(const char *parm_name) VRAY_OVERRIDE;
	virtual void               opChanged(OP_EventType reason, void *data) VRAY_OVERRIDE;

protected:
	VRayMaterialBuilder(OP_Network *parent, const char *name, OP_Operator *entry, SHOP_TYPE shader_type=SHOP_VOP_MATERIAL);
	virtual ~VRayMaterialBuilder() {}

	virtual void               addNode(OP_Node *node, int notify, int explicitly) VRAY_OVERRIDE;
	virtual void               finishedLoadingNetwork(bool is_child_call) VRAY_OVERRIDE;
	virtual void               ensureSpareParmsAreUpdatedSubclass() VRAY_OVERRIDE;

protected:
	VRayVOPContextOPFilter m_opFilter;
	VOP_CodeGenerator      m_codeGen;
};


class VRayVOPContext:
		public VOPNET_Node
{
public:
	static void                register_operator_vrayenvcontext(OP_OperatorTable *table);
	static void                register_operator_vrayrccontext(OP_OperatorTable *table);
	static OP_Node            *creator(OP_Network *parent, const char *name, OP_Operator *entry) { return new VRayVOPContext(parent, name, entry); }

public:
	virtual OP_OperatorFilter *getOperatorFilter() VRAY_OVERRIDE { return &m_opFilter; }
	virtual VOP_CodeGenerator *getVopCodeGenerator() VRAY_OVERRIDE { return &m_codeGen; }
	virtual bool               evalVariableValue(UT_String &value, int index, int thread) VRAY_OVERRIDE;
	virtual bool               hasVexShaderParameter(const char *parm_name) VRAY_OVERRIDE;
	virtual void               opChanged(OP_EventType reason, void *data) VRAY_OVERRIDE;

protected:
	VRayVOPContext(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual                   ~VRayVOPContext() {}

	virtual void               addNode(OP_Node *node, int notify, int explicitly) VRAY_OVERRIDE;
	virtual void               finishedLoadingNetwork(bool is_child_call) VRAY_OVERRIDE;
	virtual void               ensureSpareParmsAreUpdatedSubclass() VRAY_OVERRIDE;

protected:
	VRayVOPContextOPFilter m_opFilter;
	VOP_CodeGenerator      m_codeGen;

};


} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_CONTEXT_H
