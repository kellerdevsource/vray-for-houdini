//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Mila Grigorova <mila.grigorova@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
//

#include "vop_MaterialOutput.h"

#include <VOP/VOP_Operator.h>
#include <VOP/VOP_OperatorInfo.h>
#include <OP/OP_Input.h>

#include <boost/format.hpp>

using namespace VRayForHoudini;


static PRM_Template templates[] = {
	PRM_Template()
};


void VOP::MaterialOutput::register_operator(OP_OperatorTable *table)
{
	VOP_Operator *op = new VOP_Operator("vray_material_output",
									   "V-Ray Material Output",
									   VOP::MaterialOutput::creator,
									   templates,
									   0,
									   VOP_VARIABLE_INOUT_MAX,
									   "VRay",
									   0,
									   OP_FLAG_UNORDERED,
									   0);

	// Set icon
	op->setIconName("ROP_vray");
	table->addOperator(op);
}


void VOP::MaterialOutput::getCode(UT_String &, const VOP_CodeGenContext &)
{
}


void VOP::MaterialOutput::getInputNameSubclass(UT_String &in, int idx) const
{
	const std::string &label = boost::str(boost::format("shader%i") % (idx + 1));
	in = label.c_str();
}


int VOP::MaterialOutput::getInputFromNameSubclass(const UT_String &in) const
{
	auto name = static_cast<const tchar *>(in);
	const tchar *baseName = "shader";
	const int baseLen = vutils_strlen(baseName);
	if (vutils_strcmp_n(name, baseName, baseLen)) {
		return (vutils_atoi(name + baseLen) - 1);
	}
	return -1;
}


void VOP::MaterialOutput::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	getInputTypeInfoFromInputNode(type_info, idx, false);
	if (idx < nConnectedInputs()) {
		VOP_VopTypeInfoArray allowedTypes;
		getAllowedInputTypeInfos(idx, allowedTypes);
		for (auto allowed : allowedTypes) {
			if (type_info == allowed) {
				return;
			}
		}

		type_info.setTypeInfoFromType(VOP_TYPE_ERROR);
	}
}


void VOP::MaterialOutput::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos)
{
	VOP_TypeInfo typeSurf(static_cast<VOP_Type>(MTL_SURFACE));
	type_infos.append(typeSurf);
	VOP_TypeInfo typeDispl(static_cast<VOP_Type>(MTL_DISPLACEMENT));
	type_infos.append(typeDispl);
	VOP_TypeInfo typeGeom(static_cast<VOP_Type>(MTL_GEOMETRY));
	type_infos.append(typeGeom);
	VOP_TypeInfo typeMisc(static_cast<VOP_Type>(MTL_MISC));
	type_infos.append(typeMisc);

}
