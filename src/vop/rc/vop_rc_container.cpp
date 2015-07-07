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

#include <boost/format.hpp>

#include "vop_rc_container.h"


using namespace VRayForHoudini;


void VOP::RenderChannelsContainer::setPluginType()
{
	pluginType = "SETTINGS";
	pluginID   = "SettingsRenderChannels";
}


const char* VOP::RenderChannelsContainer::inputLabel(unsigned idx) const
{
	const std::string &label =boost::str(boost::format("Channel %i") % (idx+1));

	return label.c_str();
}


int VOP::RenderChannelsContainer::getInputFromName(const UT_String &in) const
{
	return getInputFromNameSubclass(in);
}


void VOP::RenderChannelsContainer::getInputNameSubclass(UT_String &in, int idx) const
{
	in = boost::str(boost::format("chan_%i") % (idx+1));
}


int VOP::RenderChannelsContainer::getInputFromNameSubclass(const UT_String &in) const
{
	int inIdx = -1;

	if (in.startsWith("chan_")) {
		sscanf(in.buffer(), "chan_%i", &inIdx);
	}

	return inIdx-1;
}


void VOP::RenderChannelsContainer::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	type_info.setType(VOP_TYPE_VOID);
}


void VOP::RenderChannelsContainer::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos)
{
	VOP_TypeInfo type_info(VOP_TYPE_VOID);
	type_infos.append(type_info);
}


unsigned VOP::RenderChannelsContainer::getNumVisibleInputs() const
{
	int max = nInputs();

	// Make sure there is always exactly one unconnected unordered input
	// visible.
	if (max < orderedInputs()) {
		max = orderedInputs();
	}

	return max + 1;
}


unsigned VOP::RenderChannelsContainer::orderedInputs() const
{
	return 1;
}


OP::VRayNode::PluginResult VOP::RenderChannelsContainer::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = "RenderChannels";

	// Last is always not connected
	const int channels_count = nInputs();

	for (int i = 1; i <= channels_count; ++i) {
		const std::string &chanSockName = boost::str(boost::format("chan_%i") % i);

		OP_Node *chan_node = VRayExporter::getConnectedNode(this, chanSockName);
		if (NOT(chan_node)) {
			PRINT_WARN("Node \"%s\": Render channel node is not connected to \"%s\", ignoring...",
					   getName().buffer(), chanSockName.c_str());
		}
		else {
			VRay::Plugin chan_plugin = exporter->exportVop(chan_node);
			if (NOT(chan_plugin)) {
				PRINT_ERROR("Node \"%s\": Failed to export render channel node connected to \"%s\", ignoring...",
							getName().buffer(), chanSockName.c_str());
			}
		}
	}

	return PluginResult::PluginResultContinue;
}
