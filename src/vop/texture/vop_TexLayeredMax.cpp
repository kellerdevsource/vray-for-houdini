//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_TexLayeredMax.h"

#include <boost/format.hpp>

using namespace VRayForHoudini;

void VOP::TexLayeredMax::setPluginType()
{
	pluginType = VRayPluginType::TEXTURE;
	pluginID   = "TexLayeredMax";
}

const char* VOP::TexLayeredMax::inputLabel(unsigned idx) const
{
	const int numBaseInputs = VOP::NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		return VOP::NodeBase::inputLabel(idx);
	}

	const int socketIndex = idx - numBaseInputs + 1;
	const std::string &label =boost::str(boost::format("Texture %i") % socketIndex);

	return label.c_str();
}

int VOP::TexLayeredMax::getInputFromName(const UT_String &in) const
{
	return getInputFromNameSubclass(in);
}

void VOP::TexLayeredMax::getInputNameSubclass(UT_String &in, int idx) const
{
	const int numBaseInputs = VOP::NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		VOP::NodeBase::getInputNameSubclass(in, idx);
	}
	else {
		const int socketIndex = idx - numBaseInputs + 1;
		in = boost::str(boost::format("tex_%i") % socketIndex);
	}
}

int VOP::TexLayeredMax::getInputFromNameSubclass(const UT_String &in) const
{
	int inIdx = -1;

	if (in.startsWith("tex_")) {
		const int numBaseInputs = VOP::NodeBase::orderedInputs();

		int idx = -1;
		sscanf(in.buffer(), "tex_%i", &idx);

		inIdx = numBaseInputs + idx - 1;
	}
	else {
		inIdx = VOP::NodeBase::getInputFromNameSubclass(in);
	}

	return inIdx;
}

void VOP::TexLayeredMax::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	const int numBaseInputs = VOP::NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		VOP::NodeBase::getInputTypeInfoSubclass(type_info, idx);
	}
	else {
		type_info.setType(VOP_TYPE_COLOR);
	}
}

void VOP::TexLayeredMax::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos)
{
	const int numBaseInputs = VOP::NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		VOP::NodeBase::getAllowedInputTypeInfosSubclass(idx, type_infos);
	}
	else {
		VOP_TypeInfo type_info(VOP_TYPE_COLOR);
		type_infos.append(type_info);
	}
}

int VOP::TexLayeredMax::customInputsCount() const
{
	return evalInt("textures_count", 0, 0.0);
}

unsigned VOP::TexLayeredMax::getNumVisibleInputs() const
{
	return orderedInputs();
}

unsigned VOP::TexLayeredMax::orderedInputs() const
{
	int orderedInputs = VOP::NodeBase::orderedInputs();
	orderedInputs += customInputsCount();

	return orderedInputs;
}

OP::VRayNode::PluginResult VOP::TexLayeredMax::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	const fpreal &t = exporter.getContext().getTime();

	const int tex_count = evalInt("textures_count", 0, 0.0);

	VRay::ValueList textures;
	VRay::IntList   blend_modes;
	VRay::FloatList opacities;

	for (int i = 1; i <= tex_count; ++i) {
		const std::string &texSockName = boost::str(boost::format("tex_%i") % i);

		OP_Node *tex_node = VRayExporter::getConnectedNode(this, texSockName);
		if (!tex_node) { 
			Log::getLog().warning("Node \"%s\": Texture node is not connected to \"%s\", ignoring...",
					   getName().buffer(), texSockName.c_str());
		}
		else {
			VRay::Plugin tex_plugin = exporter.exportVop(tex_node, parentContext);
			if (!tex_plugin) {
				Log::getLog().error("Node \"%s\": Failed to export texture node connected to \"%s\", ignoring...",
							getName().buffer(), texSockName.c_str());
			}
			else {
				const int blend_mode   = evalIntInst("tex#blend_mode", &i, 0, t);
				const float blend_amount = evalFloatInst("tex#blend_amount", &i, 0, t);

				textures.push_back(VRay::Value(tex_plugin));
				blend_modes.push_back(blend_mode);
				opacities.push_back(blend_amount);
			}
		}
	}

	if (!textures.size()) {
		return PluginResult::PluginResultError;
	}

	std::reverse(textures.begin(), textures.end());
	std::reverse(blend_modes.begin(), blend_modes.end());
	std::reverse(opacities.begin(), opacities.end());

	pluginDesc.add(Attrs::PluginAttr("textures", textures));
	pluginDesc.add(Attrs::PluginAttr("blend_modes", blend_modes));
	pluginDesc.add(Attrs::PluginAttr("opacities", opacities));

	return PluginResult::PluginResultContinue;
}
