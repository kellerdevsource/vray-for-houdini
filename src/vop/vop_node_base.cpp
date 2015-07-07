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

#include "vop_node_base.h"


using namespace VRayForHoudini;
using namespace Parm;


VOP::NodeBase::NodeBase(OP_Network *parent, const char *name, OP_Operator *entry):
	OP::VRayNode(),
	VOP_Node(parent, name, entry)
{
}


bool VOP::NodeBase::hasPluginInfo() const
{
	return !(pluginInfo == nullptr);
}


bool VOP::NodeBase::updateParmsFlags()
{
	// PRINT_INFO("VOP::NodeBase::updateParmsFlags()");
	// TODO: Hide / disable color attribute if texture is set
	// NOTE: Input connections doesn't call this

	bool changed = VOP_Node::updateParmsFlags();

	UI::ActiveStateDeps::activateElements(pluginID, this, changed);

	return changed;
}


unsigned VOP::NodeBase::orderedInputs() const
{
	DEBUG_SOCKET("%s::orderedInputs()",
				 getName().buffer());

	if (hasPluginInfo()) {
		return pluginInfo->inputs.size();
	}

	return 0;
}


unsigned VOP::NodeBase::getNumVisibleInputs() const
{
	DEBUG_SOCKET("%s::getNumVisibleInputs()",
				 getName().buffer());

	return orderedInputs();
}


const char* VOP::NodeBase::inputLabel(unsigned idx) const
{
	DEBUG_SOCKET("%s::inputLabel(%i)",
				 getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->inputs.size())) {
		return pluginInfo->inputs[idx].name.getLabel();
	}

	return nullptr;
}


void VOP::NodeBase::getInputNameSubclass(UT_String &in, int idx) const
{
	DEBUG_SOCKET("%s::getInputNameSubclass(%i)",
				 getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->inputs.size())) {
		in = pluginInfo->inputs[idx].name.getToken();
	}
}


int VOP::NodeBase::getInputFromNameSubclass(const UT_String &in) const
{
	DEBUG_SOCKET("%s::getInputFromNameSubclass(%s)",
				 getName().buffer(), in.buffer());

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
	DEBUG_SOCKET("%s::getInputTypeInfoSubclass(%i)",
				 getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->inputs.size())) {
		const SocketDesc &socketTypeInfo = pluginInfo->inputs[idx];
		type_info.setType(socketTypeInfo.vopType);
	}
}


void VOP::NodeBase::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos)
{
	DEBUG_SOCKET("%s::getAllowedInputTypeInfosSubclass(%i)",
				 getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->inputs.size())) {
		const SocketDesc &socketTypeInfo = pluginInfo->inputs[idx];

		VOP_TypeInfo type_info(socketTypeInfo.vopType);
		type_infos.clear();
		type_infos.append(type_info);
	}
}


unsigned VOP::NodeBase::getNumVisibleOutputs() const
{
	DEBUG_SOCKET("%s::getNumVisibleOutputs()",
				 getName().buffer());

	return maxOutputs();
}


unsigned VOP::NodeBase::maxOutputs() const
{
	DEBUG_SOCKET("%s::maxOutputs()",
				 getName().buffer());

	if (hasPluginInfo()) {
		return pluginInfo->outputs.size();
	}

	return 0;
}


const char* VOP::NodeBase::outputLabel(unsigned idx) const
{
	DEBUG_SOCKET("%s::outputLabel(%i)",
				 getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->outputs.size())) {
		return pluginInfo->outputs[idx].name.getLabel();
	}

	return nullptr;
}


void VOP::NodeBase::getOutputNameSubclass(UT_String &name, int idx) const
{
	DEBUG_SOCKET("%s::getOutputNameSubclass(%i)",
				 getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->outputs.size())) {
		name = pluginInfo->outputs[idx].name.getToken();
	}
}


int VOP::NodeBase::getOutputFromName(const UT_String &out) const
{
	DEBUG_SOCKET("%s::getOutputFromName(%s)",
				 getName().buffer(), out.buffer());

	if (hasPluginInfo()) {
		for (int i = 0; i < pluginInfo->outputs.size(); ++i) {
			if (out == pluginInfo->outputs[i].name.getToken()) {
				DEBUG_SOCKET(" %s => %i",
							 out.buffer(), i);
				return i;
			}
		}
	}

	return -1;
}


void VOP::NodeBase::getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	DEBUG_SOCKET("%s::getOutputTypeInfoSubclass(%i)",
				 getName().buffer(), idx);

	if (hasPluginInfo() && (idx >= 0) && (idx < pluginInfo->outputs.size())) {
		const SocketDesc &socketTypeInfo = pluginInfo->outputs[idx];
		type_info.setType(socketTypeInfo.vopType);
	}
}
