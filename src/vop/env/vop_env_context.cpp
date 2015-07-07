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

#include "vop_env_context.h"

#include <SHOP/SHOP_Operator.h>


using namespace VRayForHoudini;


static PRM_Template templates[] = {
	PRM_Template()
};


VOP::EnvironmentContext::EnvironmentContext(OP_Network *parent, const char *name, OP_Operator *entry):
	OP_Network(parent, name, entry)
{
	setOperatorTable(getOperatorTable(VOP_TABLE_NAME));
}


const char* VOP::EnvironmentContext::getChildType() const
{
	return VOP_OPTYPE_NAME;
}


OP_OpTypeId VOP::EnvironmentContext::getChildTypeID() const
{
	return VOP_OPTYPE_ID;
}


const char *VOP::EnvironmentContext::getOpType() const
{
	return VOPNET_OPTYPE_NAME;
}


OP_OpTypeId VOP::EnvironmentContext::getOpTypeID() const
{
	return VOPNET_OPTYPE_ID;
}


OP_DataType VOP::EnvironmentContext::getCookedDataType() const
{
	return OP_NO_DATA;
}


int VOP::EnvironmentContext::saveCookedData(std::ostream &os, OP_Context &, int binary)
{
	return 0;
}


int VOP::EnvironmentContext::saveCookedData(const char *filename, OP_Context &)
{
	return 0;
}


OP_ERROR VOP::EnvironmentContext::cookMe(OP_Context &context)
{
	return error();
}


OP_ERROR VOP::EnvironmentContext::bypassMe(OP_Context &context, int &copied_input)
{
	return error();
}


const char *VOP::EnvironmentContext::getFileExtension(int binary) const
{
	return binary ? ".bhip" : ".hip";
}


void VOP::EnvironmentContext::register_operator(OP_OperatorTable *table)
{
	OP_Operator *op = new OP_Operator("vray_environment",
									  "V-Ray Environment",
									  VOP::EnvironmentContext::creator,
									  templates,
									  0);

	// Set icon
	op->setIconName("ROP_vray");

	table->addOperator(op);
}


void VOP::EnvironmentContext::register_shop_operator(OP_OperatorTable *table)
{
	SHOP_Operator *op = new SHOP_Operator("vray_environment",
										  "V-Ray Environment",
										  VOP::EnvironmentContext::creator,
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
