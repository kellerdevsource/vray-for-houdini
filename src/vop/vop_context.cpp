//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_context.h"
#include "vfh_defines.h"

#include <SHOP/SHOP_Operator.h>
#include <VOP/VOP_Operator.h>
#include <VOP/VOP_OperatorInfo.h>
#include <VOP/VOP_LanguageContextTypeList.h>
#include <VOP/VOP_ExportedParmsManager.h>


using namespace VRayForHoudini;


static PRM_Template templates[] = {
	PRM_Template()
};


VOP::VRayVOPContextOPFilter::VRayVOPContextOPFilter()
{
	m_allowedVOPs.insert("parameter");
	m_allowedVOPs.insert("switch");
	m_allowedVOPs.insert("null");
	m_allowedVOPs.insert("makexform");
}


bool VOP::VRayVOPContextOPFilter::allowOperatorAsChild(OP_Operator *op)
{
	bool res = false;
	auto info = static_cast<const VOP_OperatorInfo *>(op->getOpSpecificData());
	res |= ((info)? info->getVopnetMask().startsWith("VRay") : false);
	res |= m_allowedVOPs.count(op->getName().buffer());
	return res;
}


VOP::VRayMaterialBuilder::VRayMaterialBuilder(OP_Network *parent, const char *name, OP_Operator *entry, SHOP_TYPE shader_type)
	: SHOP_Node(parent, name, entry, shader_type)
	, m_codeGen(this, new VOP_LanguageContextTypeList(VOP_LANGUAGE_VEX, VOPconvertToContextType(VEX_SURFACE_CONTEXT)), 1, 1)
{
	setOperatorTable(getOperatorTable(VOP_TABLE_NAME));

	auto info = static_cast<SHOP_OperatorInfo *>(entry->getOpSpecificData());
	info->setShaderType(shader_type);
	info->setNumOutputs(0);
}


OP_ERROR VOP::VRayMaterialBuilder::cookMe(OP_Context &context)
{
	return error();
}


bool VOP::VRayMaterialBuilder::evalVariableValue(UT_String &value, int index, int thread)
{
	if (m_codeGen.getVariableString(index, value)) {
		return true;
	}

	return SHOP_Node::evalVariableValue(value, index, thread);
}


bool VOP::VRayMaterialBuilder::hasVexShaderParameter(const char *parm_name)
{
	return m_codeGen.hasShaderParameter(parm_name);
}


void VOP::VRayMaterialBuilder::opChanged(OP_EventType reason, void *data)
{
	const int updateId = m_codeGen.beginUpdate();

	SHOP_Node::opChanged(reason, data);

	m_codeGen.ownerChanged(reason, data);
	m_codeGen.endUpdate(updateId);
}


void VOP::VRayMaterialBuilder::finishedLoadingNetwork(bool is_child_call)
{
	m_codeGen.ownerFinishedLoadingNetwork();
	SHOP_Node::finishedLoadingNetwork(is_child_call);
}


void VOP::VRayMaterialBuilder::addNode(OP_Node *node, int notify, int explicitly)
{
	m_codeGen.beforeAddNode(node);
	SHOP_Node::addNode(node, notify, explicitly);
	m_codeGen.afterAddNode(node);
}


void VOP::VRayMaterialBuilder::ensureSpareParmsAreUpdatedSubclass()
{
	// Check if the spare parameter templates
	// are out-of-date.
	if (getVopCodeGenerator()
		&& eventMicroNode(OP_SPAREPARM_MODIFIED)
			.requiresUpdate(0.0))
	{
		// Call into the code generator to update
		// the spare parameter templates.
		getVopCodeGenerator()
			->exportedParmsManager()
			->updateOwnerSpareParmLayout();
	}
}


void VOP::VRayMaterialBuilder::register_shop_operator(OP_OperatorTable *table)
{
	SHOP_Operator *op = new SHOP_Operator("vray_material",
										  "V-Ray Material",
										  VOP::VRayMaterialBuilder::creator,
										  templates,
										  0,
										  0,
										  VOP_CodeGenerator::theLocalVariables,
										  OP_FLAG_GENERATOR,
										  SHOP_AUTOADD_NONE);

	// Set icon
	op->setIconName("ROP_vray");

	table->addOperator(op);
}


VOP::VRayVOPContext::VRayVOPContext(OP_Network *parent, const char *name, OP_Operator *entry):
	OP_Network(parent, name, entry)
	, m_codeGen(this, new VOP_LanguageContextTypeList(VOP_LANGUAGE_VEX, VOPconvertToContextType(VEX_CVEX_CONTEXT)), 1, 1)
{
	setOperatorTable(getOperatorTable(VOP_TABLE_NAME));
}


const char* VOP::VRayVOPContext::getChildType() const
{
	return VOP_OPTYPE_NAME;
}


OP_OpTypeId VOP::VRayVOPContext::getChildTypeID() const
{
	return VOP_OPTYPE_ID;
}


const char *VOP::VRayVOPContext::getOpType() const
{
	return VOPNET_OPTYPE_NAME;
}


OP_OpTypeId VOP::VRayVOPContext::getOpTypeID() const
{
	return VOPNET_OPTYPE_ID;
}


OP_DataType VOP::VRayVOPContext::getCookedDataType() const
{
	return OP_NO_DATA;
}


int VOP::VRayVOPContext::saveCookedData(std::ostream &os, OP_Context &, int binary)
{
	return 0;
}


int VOP::VRayVOPContext::saveCookedData(const char *filename, OP_Context &)
{
	return 0;
}


OP_ERROR VOP::VRayVOPContext::cookMe(OP_Context &context)
{
	return error();
}


OP_ERROR VOP::VRayVOPContext::bypassMe(OP_Context &context, int &copied_input)
{
	return error();
}


const char *VOP::VRayVOPContext::getFileExtension(int binary) const
{
	return binary ? ".bhip" : ".hip";
}


bool VOP::VRayVOPContext::evalVariableValue(UT_String &value, int index, int thread)
{
	if (m_codeGen.getVariableString(index, value)) {
		return true;
	}

	return OP_Network::evalVariableValue(value, index, thread);
}


bool VOP::VRayVOPContext::hasVexShaderParameter(const char *parm_name)
{
	return m_codeGen.hasShaderParameter(parm_name);
}


void VOP::VRayVOPContext::opChanged(OP_EventType reason, void *data)
{
	const int updateId = m_codeGen.beginUpdate();

	OP_Network::opChanged(reason, data);

	m_codeGen.ownerChanged(reason, data);
	m_codeGen.endUpdate(updateId);
}


void VOP::VRayVOPContext::finishedLoadingNetwork(bool is_child_call)
{
	m_codeGen.ownerFinishedLoadingNetwork();
	OP_Network::finishedLoadingNetwork(is_child_call);
}


void VOP::VRayVOPContext::addNode(OP_Node *node, int notify, int explicitly)
{
	m_codeGen.beforeAddNode(node);
	OP_Network::addNode(node, notify, explicitly);
	m_codeGen.afterAddNode(node);
}


void VOP::VRayVOPContext::ensureSpareParmsAreUpdatedSubclass()
{
	// Check if the spare parameter templates
	// are out-of-date.
	if (getVopCodeGenerator()
		&& eventMicroNode(OP_SPAREPARM_MODIFIED)
			.requiresUpdate(0.0))
	{
		// Call into the code generator to update
		// the spare parameter templates.
		getVopCodeGenerator()
			->exportedParmsManager()
			->updateOwnerSpareParmLayout();
	}
}


void VOP::VRayVOPContext::register_operator_vrayenvcontext(OP_OperatorTable *table)
{
	OP_Operator *op = new OP_Operator("vray_environment",
									  "V-Ray Environment",
									  VOP::VRayVOPContext::creator,
									  templates,
									  0,
									  9999,
									  VOP_CodeGenerator::theLocalVariables,
									  OP_FLAG_GENERATOR
									  );

	// Set icon
	op->setIconName("ROP_vray");

	table->addOperator(op);
}


void VOP::VRayVOPContext::register_operator_vrayrccontext(OP_OperatorTable *table)
{
	OP_Operator *op = new OP_Operator("vray_render_channels",
									  "V-Ray Render Channles",
									  VOP::VRayVOPContext::creator,
									  templates,
									  0,
									  9999,
									  VOP_CodeGenerator::theLocalVariables,
									  OP_FLAG_GENERATOR
									  );

	// Set icon
	op->setIconName("ROP_vray");

	table->addOperator(op);
}


void VOP::VRayVOPContext::register_operator(OP_OperatorTable *table)
{
	OP_Operator *op = new OP_Operator("vray_vopcontext",
									  "V-Ray VOP Context",
									  VOP::VRayVOPContext::creator,
									  templates,
									  0);

	// Set icon
	op->setIconName("ROP_vray");

	table->addOperator(op);
}



