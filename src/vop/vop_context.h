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

#ifndef VRAY_FOR_HOUDINI_VOP_CONTEXT_H
#define VRAY_FOR_HOUDINI_VOP_CONTEXT_H

#include <vfh_vray.h>

#include <OP/OP_OperatorTable.h>
#include <OP/OP_Network.h>
#include <SHOP/SHOP_Node.h>


namespace VRayForHoudini {
namespace VOP {


class VRayMaterialBuilderOperatorFilter:
		public OP_OperatorFilter
{
public:
	virtual bool allowOperatorAsChild(OP_Operator *op) VRAY_OVERRIDE;
};


class VRayMaterialBuilder:
		public SHOP_Node
{
public:
	static OP_Node            *creator(OP_Network *parent, const char *name, OP_Operator *entry) { return new VRayMaterialBuilder(parent, name, entry); }

public:
/// Allow VOPs only as children
	virtual const char        *getChildType() const VRAY_OVERRIDE { return VOP_OPTYPE_NAME; }
	virtual OP_OpTypeId        getChildTypeID() const VRAY_OVERRIDE { return VOP_OPTYPE_ID; }
	virtual OP_OperatorFilter *getOperatorFilter() VRAY_OVERRIDE{ return &m_opFilter; }

	virtual OP_ERROR           cookMe(OP_Context &context) VRAY_OVERRIDE;

protected:
	VRayMaterialBuilder(OP_Network *parent, const char *name, OP_Operator *entry, SHOP_TYPE shader_type=SHOP_VOP_MATERIAL);
	virtual ~VRayMaterialBuilder() {}

protected:
	VRayMaterialBuilderOperatorFilter m_opFilter;
};


class MaterialContext:
		public OP_Network
{
public:
	static OP_Node            *creator(OP_Network *parent, const char *name, OP_Operator *entry) { return new MaterialContext(parent, name, entry); }

	virtual const char        *getChildType() const VRAY_OVERRIDE;
	virtual OP_OpTypeId        getChildTypeID() const VRAY_OVERRIDE;

	virtual int                isNetwork() const VRAY_OVERRIDE { return true; }
	virtual OP_OpTypeId        getOpTypeID() const VRAY_OVERRIDE;
	virtual const char        *getOpType() const VRAY_OVERRIDE;
	virtual OP_DataType        getCookedDataType() const VRAY_OVERRIDE;
	virtual void               deleteCookedData() VRAY_OVERRIDE {}
	virtual int                saveCookedData(std::ostream &os, OP_Context &, int binary=0) VRAY_OVERRIDE;
	virtual int                saveCookedData(const char *filename, OP_Context &) VRAY_OVERRIDE;
	virtual OP_ERROR           cookMe(OP_Context &context) VRAY_OVERRIDE;
	virtual OP_ERROR           bypassMe(OP_Context &context, int &copied_input) VRAY_OVERRIDE;
	virtual const char        *getFileExtension(int binary) const VRAY_OVERRIDE;

protected:
	MaterialContext(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual                   ~MaterialContext() {}

public:
	static void                register_operator(OP_OperatorTable *table);
	static void                register_shop_operator(OP_OperatorTable *table);

};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_CONTEXT_H
