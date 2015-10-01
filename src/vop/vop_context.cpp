//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
//

#include "vop_context.h"
#include "vfh_defines.h"

#include <SHOP/SHOP_Operator.h>
#include <VOP/VOP_OperatorInfo.h>

using namespace VRayForHoudini;


static PRM_Template templates[] = {
	PRM_Template()
};


bool VOP::VRayMaterialBuilderOperatorFilter::allowOperatorAsChild(OP_Operator *op)
{
	auto info = static_cast<const VOP_OperatorInfo *>(op->getOpSpecificData());
	return ((info)? info->getVopnetMask().startsWith("VRay") : false);
}


VOP::VRayMaterialBuilder::VRayMaterialBuilder(OP_Network *parent, const char *name, OP_Operator *entry, SHOP_TYPE shader_type):
	SHOP_Node(parent, name, entry, shader_type)
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
										  nullptr,
										  0,
										  SHOP_AUTOADD_NONE);

	// Set icon
	op->setIconName("ROP_vray");

	table->addOperator(op);
}
