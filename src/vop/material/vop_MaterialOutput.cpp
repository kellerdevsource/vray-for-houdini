//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
#include <UT/UT_Version.h>

#include <boost/format.hpp>
#include <unordered_set>

using namespace VRayForHoudini;
using namespace VOP;

enum MaterialOutputInputs {
	materialOutputInputMaterial = 0,
	materialOutputInputSurface,
	materialOutputInputSimulation,
	materialOutputInputMax,
};

struct MaterialOutputInputSocket {
	const char *label;
	const VOP_TypeInfo typeInfo;
};

static MaterialOutputInputSocket materialOutputInputSockets[] = {
	{ "Material",   VOP_TypeInfo(VOP_SURFACE_SHADER)    },
	{ "Surface",    VOP_TypeInfo(VOP_GEOMETRY_SHADER)   },
	{ "Simulation", VOP_TypeInfo(VOP_ATMOSPHERE_SHADER) },
} ;

static PRM_Template templates[] = {
	PRM_Template()
};

void MaterialOutput::register_operator(OP_OperatorTable *table)
{
	VOP_Operator *op = new VOP_Operator("vray_material_output", "V-Ray Material Output",
										MaterialOutput::creator, templates,
#if UT_MAJOR_VERSION_INT >= 16
										nullptr,
#endif
										3, 3,
										"VRay",
										nullptr,
										OP_FLAG_UNORDERED | OP_FLAG_OUTPUT,
										0 );

	// Set icon
	op->setIconName("ROP_vray");
	table->addOperator(op);
}

OP_Node* MaterialOutput::creator(OP_Network *parent, const char *name, OP_Operator *entry)
{
	return new MaterialOutput(parent, name, entry);
}

MaterialOutput::MaterialOutput(OP_Network *parent, const char *name, OP_Operator *entry)
	: VOP_Node(parent, name, entry)
{}

MaterialOutput::~MaterialOutput()
{}

unsigned MaterialOutput::orderedInputs() const
{
	return materialOutputInputMax;
}

unsigned MaterialOutput::getNumVisibleInputs() const
{
	return orderedInputs();
}

bool MaterialOutput::willAutoconvertInputType(int idx)
{
	return true;
}

const char* MaterialOutput::inputLabel(unsigned idx) const
{
	if (idx >= materialOutputInputMax)
		return nullptr;
	return materialOutputInputSockets[idx].label;
}

void MaterialOutput::getInputNameSubclass(UT_String &in, int idx) const
{
	if (idx >= materialOutputInputMax)
		in = "Unknown";
	else
		in = inputLabel(idx);
}

int MaterialOutput::getInputFromNameSubclass(const UT_String &in) const
{
	for (int idx = 0; idx < materialOutputInputMax; ++idx) {
		if (in.equal(materialOutputInputSockets[idx].label)) {
			return idx;
		}
	}
	return -1;
}

void MaterialOutput::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	if (idx >= materialOutputInputMax)
		type_info = VOP_TypeInfo(VOP_TYPE_ERROR);
	else
		type_info = materialOutputInputSockets[idx].typeInfo;
}

void MaterialOutput::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos)
{
	switch (idx) {
		case materialOutputInputMaterial: {
			type_infos.append(VOP_TypeInfo(VOP_TYPE_BSDF));
			type_infos.append(VOP_TypeInfo(VOP_SURFACE_SHADER));
			break;
		}
		case materialOutputInputSurface: {
			type_infos.append(VOP_TypeInfo(VOP_GEOMETRY_SHADER));
			break;
		}
		case materialOutputInputSimulation: {
			type_infos.append(VOP_TypeInfo(VOP_ATMOSPHERE_SHADER));
		}
		default: {
			type_infos.append(VOP_TypeInfo(VOP_TYPE_ERROR));
			break;
		}
	}
}
