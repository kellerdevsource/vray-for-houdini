//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_rc_context.h"

#include <SHOP/SHOP_Operator.h>


using namespace VRayForHoudini;


static PRM_Template templates[] = {
	PRM_Template()
};


VOP::RenderChannelsContext::RenderChannelsContext(OP_Network *parent, const char *name, OP_Operator *entry):
	OP_Network(parent, name, entry)
{
	setOperatorTable(getOperatorTable(VOP_TABLE_NAME));
}


const char* VOP::RenderChannelsContext::getChildType() const
{
	return VOP_OPTYPE_NAME;
}


OP_OpTypeId VOP::RenderChannelsContext::getChildTypeID() const
{
	return VOP_OPTYPE_ID;
}


const char *VOP::RenderChannelsContext::getOpType() const
{
	return VOPNET_OPTYPE_NAME;
}


OP_OpTypeId VOP::RenderChannelsContext::getOpTypeID() const
{
	return VOPNET_OPTYPE_ID;
}


OP_DataType VOP::RenderChannelsContext::getCookedDataType() const
{
	return OP_NO_DATA;
}


int VOP::RenderChannelsContext::saveCookedData(std::ostream &os, OP_Context &, int binary)
{
	return 0;
}


int VOP::RenderChannelsContext::saveCookedData(const char *filename, OP_Context &)
{
	return 0;
}


OP_ERROR VOP::RenderChannelsContext::cookMe(OP_Context &context)
{
	return error();
}


OP_ERROR VOP::RenderChannelsContext::bypassMe(OP_Context &context, int &copied_input)
{
	return error();
}


const char *VOP::RenderChannelsContext::getFileExtension(int binary) const
{
	return binary ? ".bhip" : ".hip";
}


void VOP::RenderChannelsContext::register_operator(OP_OperatorTable *table)
{
	OP_Operator *op = new OP_Operator("vray_render_channels",
									  "V-Ray Render Channles",
									  VOP::RenderChannelsContext::creator,
									  templates,
									  0);

	// Set icon
	op->setIconName("ROP_vray");

	table->addOperator(op);
}


void VOP::RenderChannelsContext::register_shop_operator(OP_OperatorTable *table)
{
	SHOP_Operator *op = new SHOP_Operator("vray_render_channels",
										  "V-Ray Render Channles",
										  VOP::RenderChannelsContext::creator,
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
