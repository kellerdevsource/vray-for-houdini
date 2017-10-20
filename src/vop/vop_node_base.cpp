//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_node_base.h"

using namespace VRayForHoudini;
using namespace Parm;


VOP::NodeBase::NodeBase(OP_Network *parent, const char *name, OP_Operator *entry)
	: VOP_Node(parent, name, entry)
	, OP::VRayNode()
{}

VOP_Type VOP::NodeBase::getShaderType() const
{
	switch (pluginType)	{
		case VRayPluginType::BRDF: return VOP_TYPE_BSDF;
		case VRayPluginType::MATERIAL: return VOP_SURFACE_SHADER;
		case VRayPluginType::TEXTURE: return VOP_GENERIC_SHADER;
		default: return VOP_TYPE_UNDEF;
	}
}

bool VOP::NodeBase::hasPluginInfo() const
{
	return !(pluginInfo == nullptr);
}


bool VOP::NodeBase::updateParmsFlags()
{
	bool changed = VOP_Node::updateParmsFlags();
	return changed;
}


unsigned VOP::NodeBase::orderedInputs() const
{
	// printf("%s::orderedInputs()\n", getName().buffer());

	if (hasPluginInfo()) {
		return pluginInfo->inputs.size();
	}

	return 0;
}


unsigned VOP::NodeBase::getNumVisibleInputs() const
{
	// printf("%s::getNumVisibleInputs()\n", getName().buffer());
	return orderedInputs();
}


const char* VOP::NodeBase::inputLabel(unsigned idx) const
{
	// printf("%s::inputLabel(%i)\n", getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->inputs.size())) {
		return pluginInfo->inputs[idx].name.getLabel();
	}

	return nullptr;
}


void VOP::NodeBase::getInputNameSubclass(UT_String &in, int idx) const
{
	// printf("%s::getInputNameSubclass(%i)\n", getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->inputs.size())) {
		in = pluginInfo->inputs[idx].name.getToken();
	}
}


int VOP::NodeBase::getInputFromNameSubclass(const UT_String &in) const
{
	// printf("%s::getInputFromNameSubclass(%s)\n", getName().buffer(), in.buffer());

	if (hasPluginInfo()) {
		for (int i = 0; i < pluginInfo->inputs.size(); ++i) {
			if (in == pluginInfo->inputs[i].name.getToken()) {
				return i;
			}
		}
	}

	return -1;
}


void VOP::NodeBase::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	// printf("%s::getInputTypeInfoSubclass(%i)\n", getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->inputs.size())) {
		const SocketDesc &socketTypeInfo = pluginInfo->inputs[idx];
		type_info.setType(socketTypeInfo.vopType);
	}
}


void VOP::NodeBase::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos)
{
	// printf("%s::getAllowedInputTypeInfosSubclass(%i)\n", getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->inputs.size())) {
		const SocketDesc &socketTypeInfo = pluginInfo->inputs[idx];

		type_infos.clear();

		const VOP_Type vopType = socketTypeInfo.vopType;
		type_infos.append(VOP_TypeInfo(vopType));

		if (vopType == VOP_SURFACE_SHADER) {
			type_infos.append(VOP_TypeInfo(VOP_TYPE_BSDF));
		}
		if (vopType == VOP_TYPE_BSDF) {
			type_infos.append(VOP_TypeInfo(VOP_SURFACE_SHADER));
		}
		if (vopType == VOP_TYPE_COLOR) {
			type_infos.append(VOP_TypeInfo(VOP_TYPE_FLOAT));
		}
		if (vopType == VOP_TYPE_FLOAT) {
			type_infos.append(VOP_TypeInfo(VOP_TYPE_COLOR));
		}
	}
}

void VOP::NodeBase::getAllowedInputTypesSubclass(unsigned idx, VOP_VopTypeArray &type_infos)
{
	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->inputs.size())) {
		const SocketDesc &socketTypeInfo = pluginInfo->inputs[idx];

		type_infos.clear();

		const VOP_Type vopType = socketTypeInfo.vopType;
		type_infos.append(vopType);

		if (vopType == VOP_SURFACE_SHADER) {
			type_infos.append(VOP_TYPE_BSDF);
		}
		if (vopType == VOP_TYPE_BSDF) {
			type_infos.append(VOP_SURFACE_SHADER);
		}
		if (vopType == VOP_TYPE_COLOR) {
			type_infos.append(VOP_TYPE_FLOAT);
		}
		if (vopType == VOP_TYPE_FLOAT) {
			type_infos.append(VOP_TYPE_COLOR);
		}
	}
}

bool VOP::NodeBase::willAutoconvertInputType(int idx)
{
	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->inputs.size())) {
		const SocketDesc &socketTypeInfo = pluginInfo->inputs[idx];

		const VOP_Type vopType = socketTypeInfo.vopType;
		if (vopType == VOP_SURFACE_SHADER ||
		    vopType == VOP_TYPE_BSDF ||
		    vopType == VOP_TYPE_COLOR ||
		    vopType == VOP_TYPE_FLOAT)
		{
			return true;
		}
	}

	return false;
}

unsigned VOP::NodeBase::getNumVisibleOutputs() const
{
	// printf("%s::getNumVisibleOutputs()\n", getName().buffer());

	return maxOutputs();
}


unsigned VOP::NodeBase::maxOutputs() const
{
	// printf("%s::maxOutputs()\n", getName().buffer());

	if (hasPluginInfo()) {
		return pluginInfo->outputs.size();
	}

	return 0;
}


const char* VOP::NodeBase::outputLabel(unsigned idx) const
{
	// printf("%s::outputLabel(%i)\n", getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->outputs.size())) {
		return pluginInfo->outputs[idx].name.getLabel();
	}

	return nullptr;
}


void VOP::NodeBase::getOutputNameSubclass(UT_String &name, int idx) const
{
	// printf("%s::getOutputNameSubclass(%i)\n", getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->outputs.size())) {
		name = pluginInfo->outputs[idx].name.getToken();
	}
}


int VOP::NodeBase::getOutputFromName(const UT_String &out) const
{
	// printf("%s::getOutputFromName(%s)\n", getName().buffer(), out.buffer());

	if (hasPluginInfo()) {
		for (int i = 0; i < pluginInfo->outputs.size(); ++i) {
			if (out == pluginInfo->outputs[i].name.getToken()) {
				return i;
			}
		}
	}

	return -1;
}


void VOP::NodeBase::getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	// printf("%s::getOutputTypeInfoSubclass(%i)", getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->outputs.size())) {
		const SocketDesc &socketTypeInfo = pluginInfo->outputs[idx];
		type_info.setType(socketTypeInfo.vopType);
	}
}
