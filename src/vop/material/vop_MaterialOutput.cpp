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
	static boost::format FmtShader("shader%i");

	const std::string &label = boost::str(FmtShader % (idx + 1));
	in = label.c_str();
}


int VOP::MaterialOutput::getInputFromNameSubclass(const UT_String &in) const
{
	int inputIndex = -1;
	int inputTmp = -1;

	if (sscanf(in.buffer(), "shader%i", &inputTmp) == 1) {
		inputIndex = inputTmp - 1;
	}

	return inputIndex;
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
	type_infos.append(VOP_TypeInfo(VOP_TYPE_BSDF));
	type_infos.append(VOP_TypeInfo(VOP_SURFACE_SHADER));
	type_infos.append(VOP_TypeInfo(VOP_DISPLACEMENT_SHADER));
	type_infos.append(VOP_TypeInfo(VOP_GEOMETRY_SHADER));
	type_infos.append(VOP_TypeInfo(VOP_TYPE_VOID));
}
