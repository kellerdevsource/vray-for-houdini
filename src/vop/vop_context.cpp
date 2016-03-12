//
// Copyright (c) 2015, Chaos Software Ltd
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


VOP::MaterialContext::MaterialContext(OP_Network *parent, const char *name, OP_Operator *entry):
	OP_Network(parent, name, entry)
{
	setOperatorTable(getOperatorTable(VOP_TABLE_NAME));
}


const char* VOP::MaterialContext::getChildType() const
{
	return VOP_OPTYPE_NAME;
}


OP_OpTypeId VOP::MaterialContext::getChildTypeID() const
{
	return VOP_OPTYPE_ID;
}


const char *VOP::MaterialContext::getOpType() const
{
	return VOPNET_OPTYPE_NAME;
}


OP_OpTypeId VOP::MaterialContext::getOpTypeID() const
{
	return VOPNET_OPTYPE_ID;
}


OP_DataType VOP::MaterialContext::getCookedDataType() const
{
	return OP_NO_DATA;
}


int VOP::MaterialContext::saveCookedData(std::ostream &os, OP_Context &, int binary)
{
	return 0;
}


int VOP::MaterialContext::saveCookedData(const char *filename, OP_Context &)
{
	return 0;
}


OP_ERROR VOP::MaterialContext::cookMe(OP_Context &context)
{
	return error();
}


OP_ERROR VOP::MaterialContext::bypassMe(OP_Context &context, int &copied_input)
{
	return error();
}


const char *VOP::MaterialContext::getFileExtension(int binary) const
{
	return binary ? ".bhip" : ".hip";
}


void VOP::MaterialContext::register_operator(OP_OperatorTable *table)
{
	OP_Operator *op = new OP_Operator("vray_material_context",
									  "V-Ray Material Context",
									  VOP::MaterialContext::creator,
									  templates,
									  0);

	// Set icon
	op->setIconName("ROP_vray");

	table->addOperator(op);
}


void VOP::MaterialContext::register_shop_operator(OP_OperatorTable *table)
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
