//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_MaterialOutput.h"

#include <VOP/VOP_Operator.h>
#include <VOP/VOP_OperatorInfo.h>
#include <OP/OP_Input.h>

#include <boost/format.hpp>
#include <unordered_set>

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
									   2,
									   2,
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


bool VOP::MaterialOutput::generateErrorsSubclass()
{
	bool hasErr = VOP_Node::generateErrorsSubclass();
	if (hasErr) {
		return hasErr;
	}

	std::unordered_set<std::string> inpTypes;
	for (int idx = 0; idx < nConnectedInputs(); ++idx) {
		UT_String inpName;
		getInputName(inpName, idx);

		if (inpTypes.count(inpName.buffer())) {
			addError(VOP_INPUT_TYPE_COLLISION);
			hasErr = true;
			break;
		}
		inpTypes.insert(inpName.buffer());
	}

	return hasErr;
}

void VOP::MaterialOutput::getInputNameSubclass(UT_String &in, int idx) const
{
	static boost::format FmtShader("Input%i");

	switch (getInputType(idx)) {
		case VOP_TYPE_BSDF:
		case VOP_SURFACE_SHADER:
			in = "Material";
			break;
		case VOP_GEOMETRY_SHADER:
			in = "Geometry";
			break;
		default:
			const std::string &label = boost::str(FmtShader % (idx + 1));
			in = label.c_str();
	}
}


int VOP::MaterialOutput::getInputFromNameSubclass(const UT_String &in) const
{
	int idx = -1;
	if (sscanf(in.buffer(), "Input%i", &idx) == 1) {
		return idx - 1;
	}

	for (idx = 0; idx < nConnectedInputs(); ++idx) {
		UT_String inpName;
		getInputName(inpName, idx);
		if (in.equal(inpName)) {
			return idx;
		}
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
	type_infos.append(VOP_TypeInfo(VOP_TYPE_BSDF));
	type_infos.append(VOP_TypeInfo(VOP_SURFACE_SHADER));
	type_infos.append(VOP_TypeInfo(VOP_GEOMETRY_SHADER));
}
