//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <boost/format.hpp>

#include "vop_TexLayered.h"

using namespace VRayForHoudini;

// NOTE: Sockets order:
//
//   - Auto. sockets from description
//   - Base Texture
//   - texture_1
//   - ...
//   - texture_<tex_count>
//

void VOP::TexLayered::setPluginType()
{
	pluginType = VRayPluginType::TEXTURE;
	pluginID   = "TexLayered";
}

const char* VOP::TexLayered::inputLabel(unsigned idx) const
{
	const int numBaseInputs = VOP::NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		return VOP::NodeBase::inputLabel(idx);
	}

	const int socketIndex = idx - numBaseInputs + 1;
	const std::string &label =boost::str(boost::format("Texture %i") % socketIndex);

	return label.c_str();
}


int VOP::TexLayered::getInputFromName(const UT_String &in) const
{
	return getInputFromNameSubclass(in);
}


void VOP::TexLayered::getInputNameSubclass(UT_String &in, int idx) const
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


int VOP::TexLayered::getInputFromNameSubclass(const UT_String &in) const
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


void VOP::TexLayered::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	const int numBaseInputs = VOP::NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		VOP::NodeBase::getInputTypeInfoSubclass(type_info, idx);
	}
	else {
		type_info.setType(VOP_TYPE_COLOR);
	}
}


void VOP::TexLayered::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos)
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


int VOP::TexLayered::customInputsCount() const
{
	// One socket per texture
	int numCustomInputs = evalInt("textures_count", 0, 0.0);

	return numCustomInputs;
}


unsigned VOP::TexLayered::getNumVisibleInputs() const
{
	return orderedInputs();
}


unsigned VOP::TexLayered::orderedInputs() const
{
	int orderedInputs = VOP::NodeBase::orderedInputs();
	orderedInputs += customInputsCount();

	return orderedInputs;
}


void VOP::TexLayered::getCode(UT_String &codestr, const VOP_CodeGenContext &)
{
}


OP::VRayNode::PluginResult VOP::TexLayered::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	const fpreal &t = exporter.getContext().getTime();

	const int tex_count = evalInt("textures_count", 0, 0.0);

	VRay::ValueList textures;
	VRay::IntList   blend_modes;

	for (int i = 1; i <= tex_count; ++i) {
		const std::string &paramPrefix = boost::str(boost::format("@%i") % i);

		const std::string &texSockName = boost::str(boost::format("tex_%i") % i);

		OP_Node *tex_node = VRayExporter::getConnectedNode(this, texSockName);
		if (NOT(tex_node)) {
			Log::getLog().warning("Node \"%s\": Texture node is not connected to \"%s\", ignoring...",
					   getName().buffer(), texSockName.c_str());
		}
		else {
			VRay::Plugin tex_plugin = exporter.exportVop(tex_node, parentContext);
			if (NOT(tex_plugin)) {
				Log::getLog().error("Node \"%s\": Failed to export texture node connected to \"%s\", ignoring...",
							getName().buffer(), texSockName.c_str());
			}
			else {
				const int   blend_mode   = evalIntInst("tex#blend_mode", &i, 0, t);
				const float blend_amount = evalFloatInst("tex#blend_amount", &i, 0, t);

				if (blend_amount != 1.0f) {
					Attrs::PluginDesc blendDesc(VRayExporter::getPluginName(this, paramPrefix), "TexAColorOp");
					blendDesc.add(Attrs::PluginAttr("mode", 0)); // Mode: "result_a"
					blendDesc.add(Attrs::PluginAttr("color_a", tex_plugin));
					blendDesc.add(Attrs::PluginAttr("mult_a", 1.0f));
					blendDesc.add(Attrs::PluginAttr("result_alpha", blend_amount));

					tex_plugin = exporter.exportPlugin(blendDesc);
				}

				textures.push_back(VRay::Value(tex_plugin));
				blend_modes.push_back(blend_mode);
			}
		}
	}

	if (NOT(textures.size())) {
		return PluginResult::PluginResultError;
	}

	// TODO: Check if this is need for this UI
#if 0
	std::reverse(textures.begin(), textures.end());
	std::reverse(blend_modes.begin(), blend_modes.end());
#endif

	pluginDesc.add(Attrs::PluginAttr("textures", textures));
	pluginDesc.add(Attrs::PluginAttr("blend_modes", blend_modes));

	return PluginResult::PluginResultContinue;
}
