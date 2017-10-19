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

bool VOP::NodeBase::updateParmsFlags()
{
	bool changed = VOP_Node::updateParmsFlags();
	return changed;
}

unsigned VOP::NodeBase::orderedInputs() const
{
	return pluginInfo->inputs.count();
}

unsigned VOP::NodeBase::getNumVisibleInputs() const
{
	return orderedInputs();
}

const char* VOP::NodeBase::inputLabel(unsigned idx) const
{
	if (idx >= pluginInfo->inputs.count())
		return nullptr;
	return pluginInfo->inputs[idx].socketLabel.ptr();
}

void VOP::NodeBase::getInputNameSubclass(UT_String &name, int idx) const
{
	if (idx < 0 || idx >= pluginInfo->inputs.count())
		return;
	name = pluginInfo->inputs[idx].socketLabel.ptr();
}

int VOP::NodeBase::getInputFromNameSubclass(const UT_String &name) const
{
	for (int i = 0; i < pluginInfo->inputs.count(); ++i) {
		if (name.equal(pluginInfo->inputs[i].socketLabel.ptr())) {
			return i;
		}
	}
	return -1;
}

void VOP::NodeBase::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	if (idx < 0 || idx >= pluginInfo->inputs.count())
		return;
	type_info.setType(pluginInfo->inputs[idx].socketType);
}

void VOP::NodeBase::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos)
{
	type_infos.clear();

	if (idx >= pluginInfo->inputs.count())
		return;

	const SocketDesc &socketTypeInfo = pluginInfo->inputs[idx];

	const VOP_Type vopType = socketTypeInfo.socketType;
	type_infos.append(VOP_TypeInfo(vopType));

	if (vopType == VOP_SURFACE_SHADER) {
		type_infos.append(VOP_TypeInfo(VOP_TYPE_BSDF));
	}
	if (vopType == VOP_TYPE_BSDF) {
		type_infos.append(VOP_TypeInfo(VOP_SURFACE_SHADER));
	}
}

void VOP::NodeBase::getAllowedInputTypesSubclass(unsigned idx, VOP_VopTypeArray &type_infos)
{
	type_infos.clear();

	if (idx >= pluginInfo->inputs.count())
		return;

	const SocketDesc &socketTypeInfo = pluginInfo->inputs[idx];

	const VOP_Type vopType = socketTypeInfo.socketType;
	type_infos.append(vopType);

	if (vopType == VOP_SURFACE_SHADER) {
		type_infos.append(VOP_TYPE_BSDF);
	}
	if (vopType == VOP_TYPE_BSDF) {
		type_infos.append(VOP_SURFACE_SHADER);
	}
}

bool VOP::NodeBase::willAutoconvertInputType(int idx)
{
	if (idx < pluginInfo->inputs.count()) {
		const SocketDesc &socketTypeInfo = pluginInfo->inputs[idx];

		const VOP_Type vopType = socketTypeInfo.socketType;
		if (vopType == VOP_SURFACE_SHADER ||
			vopType == VOP_TYPE_BSDF)
		{
			return true;
		}
	}

	return false;
}

unsigned VOP::NodeBase::getNumVisibleOutputs() const
{
	return maxOutputs();
}

unsigned VOP::NodeBase::maxOutputs() const
{
	return pluginInfo->outputs.count();
}

const char* VOP::NodeBase::outputLabel(unsigned idx) const
{
	if (idx >= pluginInfo->outputs.count())
		return nullptr;

	return pluginInfo->outputs[idx].socketLabel.ptr();
}

void VOP::NodeBase::getOutputNameSubclass(UT_String &name, int idx) const
{
	if (idx < 0 || idx >= pluginInfo->outputs.count())
		return;
	name = pluginInfo->outputs[idx].socketLabel.ptr();
}

int VOP::NodeBase::getOutputFromName(const UT_String &name) const
{
	for (int i = 0; i < pluginInfo->outputs.count(); ++i) {
		if (name.equal(pluginInfo->outputs[i].socketLabel.ptr())) {
			return i;
		}
	}
	return -1;
}

void VOP::NodeBase::getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	if (idx < 0 || idx >= pluginInfo->outputs.count())
		return;
	type_info.setType(pluginInfo->outputs[idx].socketType);
}
