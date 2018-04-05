//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_TexMulti.h"

using namespace VRayForHoudini;

void VOP::TexMulti::setPluginType()
{
	pluginType = VRayPluginType::TEXTURE;
	pluginID   = "TexMulti";
}

const char* VOP::TexMulti::inputLabel(unsigned idx) const
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

int VOP::TexMulti::getInputFromName(const UT_String &in) const
{
	return getInputFromNameSubclass(in);
}

void VOP::TexMulti::getInputNameSubclass(UT_String &in, int idx) const
{
	const int numBaseInputs = NodeBase::orderedInputs();
	if (idx < numBaseInputs) {
		NodeBase::getInputNameSubclass(in, idx);
	}
	else {
		const int socketIndex = idx - numBaseInputs + 1;
		in.sprintf("tex_%i", socketIndex);
	}
}

int VOP::TexMulti::getInputFromNameSubclass(const UT_String &in) const
{
	int inIdx = -1;;

	if (in.startsWith("tex_")) {
		const int numBaseInputs = NodeBase::orderedInputs();

		int idx = -1;
		if (sscanf(in.buffer(), "tex_%i", &idx) == 1) {
			inIdx = numBaseInputs + idx - 1;
		}
	}
	else {
		inIdx = NodeBase::getInputFromNameSubclass(in);
	}

	return inIdx;
}

void VOP::TexMulti::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	const int numBaseInputs = NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		NodeBase::getInputTypeInfoSubclass(type_info, idx);
	}
	else {
		type_info.setType(VOP_TYPE_COLOR);
	}
}

void VOP::TexMulti::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos)
{
	const unsigned numBaseInputs = NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		NodeBase::getAllowedInputTypeInfosSubclass(idx, type_infos);
	}
	else {
		type_infos.append(VOP_TypeInfo(VOP_TYPE_COLOR));
	}
}

int VOP::TexMulti::customInputsCount() const
{
	return evalInt("tex_count", 0, 0.0);
}

unsigned VOP::TexMulti::getNumVisibleInputs() const
{
	return orderedInputs();
}

unsigned VOP::TexMulti::orderedInputs() const
{
	int orderedInputs = NodeBase::orderedInputs();
	orderedInputs += customInputsCount();

	return orderedInputs;
}

void VOP::TexMulti::getCode(UT_String&, const VOP_CodeGenContext &)
{}

OP::VRayNode::PluginResult VOP::TexMulti::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	const int numTextures = evalInt("tex_count", 0, 0.0);

	VRay::ValueList textures;
	VRay::IntList textureIds;

	for (int i = 1; i <= numTextures; ++i) {
		QString texSockName = QString("tex_%1").arg(i);

		OP_Node *texNode = VRayExporter::getConnectedNode(this, texSockName.toStdString());
		if (!texNode) {
			Log::getLog().warning("Node \"%s\": Texture node is not connected to \"%s\", ignoring...",
								  getName().buffer(), _toChar(texSockName));
		}
		else {
			VRay::Plugin texPlugin = exporter.exportVop(texNode, parentContext);
			if (texPlugin.isEmpty()) {
				Log::getLog().error("Node \"%s\": Failed to export texture node connected to \"%s\", ignoring...",
									getName().buffer(), _toChar(texSockName));
			}
			else {
				exporter.convertInputPlugin(texPlugin, pluginDesc, texNode, VOP_TYPE_COLOR, texSockName.toStdString().c_str());

				textures.push_back(VRay::Value(texPlugin));
				textureIds.push_back(i-1);
			}
		}
	}

	if (textures.empty())
		return PluginResultError;

	pluginDesc.addAttribute(Attrs::PluginAttr("textures_list", textures));
	pluginDesc.addAttribute(Attrs::PluginAttr("ids_list", textureIds));

	return PluginResultContinue;
}
