//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_TexLayeredMax.h"

using namespace VRayForHoudini;

void VOP::TexLayeredMax::setPluginType()
{
	pluginType = VRayPluginType::TEXTURE;
	pluginID   = "TexLayeredMax";
}

const char* VOP::TexLayeredMax::inputLabel(unsigned idx) const
{
	const unsigned numBaseInputs = NodeBase::orderedInputs();
	if (idx < numBaseInputs)
		return NodeBase::inputLabel(idx);

	const int socketIndex = idx - numBaseInputs + 1;
	if (socketIndex >= 0) {
		return getCreateSocketLabel(socketIndex, "Texture %i");
	}

	return NULL;
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
		in.sprintf("tex_%i", socketIndex);
	}
}

int VOP::TexLayeredMax::getInputFromNameSubclass(const UT_String &in) const
{
	int inIdx = -1;

	if (in.startsWith("tex_")) {
		const int numBaseInputs = VOP::NodeBase::orderedInputs();

		int idx = -1;
		if (sscanf(in.buffer(), "tex_%i", &idx) == 1) {
			inIdx = numBaseInputs + idx - 1;
		}
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
	const unsigned numBaseInputs = VOP::NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		VOP::NodeBase::getAllowedInputTypeInfosSubclass(idx, type_infos);
	}
	else {
		type_infos.append(VOP_TypeInfo(VOP_TYPE_COLOR));
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
	const fpreal t = exporter.getContext().getTime();

	const int texCount = evalInt("textures_count", 0, 0.0);

	Attrs::QValueList textures(texCount);
	Attrs::QIntList   blend_modes(texCount);
	Attrs::QFloatList opacities(texCount);

	for (int i = 1; i <= texCount; ++i) {
		const QString texSockName(SL("tex_%1").arg(i));

		OP_Node *texNode = VRayExporter::getConnectedNode(this, texSockName);
		if (!texNode) { 
			Log::getLog().warning("Node \"%s\": Texture node is not connected to \"%s\", ignoring...",
					   getName().buffer(), qPrintable(texSockName));
		}
		else {
			const VRay::Plugin texPlugin = exporter.exportVop(texNode, parentContext);
			if (texPlugin.isEmpty()) {
				Log::getLog().error("Node \"%s\": Failed to export texture node connected to \"%s\", ignoring...",
							getName().buffer(), qPrintable(texSockName));
			}
			else {
				const int blendMode = evalIntInst("tex#blend_mode", &i, 0, t);
				const float blendAmount = evalFloatInst("tex#blend_amount", &i, 0, t);

				textures.append(VRay::VUtils::Value(texPlugin));
				blend_modes.append(blendMode);
				opacities.append(blendAmount);
			}
		}
	}

	if (textures.empty())
		return PluginResultError;

	std::reverse(textures.begin(), textures.end());
	std::reverse(blend_modes.begin(), blend_modes.end());
	std::reverse(opacities.begin(), opacities.end());

	pluginDesc.add(SL("textures"), textures);
	pluginDesc.add(SL("blend_modes"), blend_modes);
	pluginDesc.add(SL("opacities"), opacities);

	return PluginResultContinue;
}
