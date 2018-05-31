//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_BRDFLayered.h"
#include "vfh_prm_templates.h"

using namespace VRayForHoudini;
using namespace VOP;

// NOTE: Sockets order:
//
//   - Auto. sockets from description
//   - brdf_1
//   - weight_1
//   - ...
//   - brdf_<brdf_count>
//   - weight_<brdf_count>
//

void BRDFLayered::setPluginType()
{
	pluginType = VRayPluginType::BRDF;
	pluginID = SL("BRDFLayered");
}

const char *BRDFLayered::inputLabel(unsigned idx) const
{
	const unsigned numBaseInputs = NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		return NodeBase::inputLabel(idx);
	}

	const int socketIndex = idx - numBaseInputs;
	const int inputHumanIndex = socketIndex / 2 + 1;

	if (socketIndex % 2) {
		return getCreateSocketLabel(socketIndex, "Weight %i", inputHumanIndex);
	}

	return getCreateSocketLabel(socketIndex, "BRDF %i", inputHumanIndex);
}

const char *BRDFLayered::outputLabel(unsigned idx) const
{
	return "BRDF";
}

void BRDFLayered::getOutputNameSubclass(UT_String &name, int idx) const
{
	name = "BRDF";
}

void BRDFLayered::getInputNameSubclass(UT_String &in, int idx) const
{
	const int numBaseInputs = NodeBase::orderedInputs();
	if (idx < numBaseInputs) {
		NodeBase::getInputNameSubclass(in, idx);
	}
	else {
		const int socketIndex = idx - numBaseInputs;
		const int inputHumanIndex = ((socketIndex) / 2) + 1;

		if (socketIndex % 2) {
			in = getCreateSocketToken(socketIndex, "weight_%i", inputHumanIndex);
		}
		else {
			in = getCreateSocketToken(socketIndex, "brdf_%i", inputHumanIndex);
		}
	}
}

int BRDFLayered::getInputFromName(const UT_String &in) const
{
	return getInputFromNameSubclass(in);
}

int BRDFLayered::getInputFromNameSubclass(const UT_String &in) const
{
	const int numInputs = NodeBase::orderedInputs();

	int inIdx = -1;

	if (in.startsWith("brdf_") && in != "brdf_count") {
		int idx;
		if (sscanf(in.buffer(), "brdf_%i", &idx) == 1) {
			inIdx = numInputs + (idx - 1) * 2;
		}
	}
	else if (in.startsWith("weight_")) {
		int idx;
		if (sscanf(in.buffer(), "weight_%i", &idx)) {
			inIdx = numInputs + (idx * 2 - 1);
		}
	}
	else {
		inIdx = NodeBase::getInputFromNameSubclass(in);
	}

	return inIdx;
}

void BRDFLayered::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	const int numBaseInputs = NodeBase::orderedInputs();
	if (idx < numBaseInputs) {
		NodeBase::getInputTypeInfoSubclass(type_info, idx);
	}
	else {
		const int socketIndex = idx - numBaseInputs;
		if (socketIndex % 2) {
			type_info.setType(VOP_TYPE_FLOAT);
		}
		else {
			type_info.setType(VOP_TYPE_BSDF);
		}
	}
}

void BRDFLayered::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos)
{
	if (idx < NodeBase::orderedInputs()) {
		NodeBase::getAllowedInputTypeInfosSubclass(idx, type_infos);
	}
	else {
		VOP_TypeInfo vopTypeInfo;
		getInputTypeInfoSubclass(vopTypeInfo, idx);
		if (vopTypeInfo == VOP_TypeInfo(VOP_TYPE_BSDF)) {
			type_infos.append(VOP_TypeInfo(VOP_SURFACE_SHADER));
		}
		if (vopTypeInfo == VOP_TypeInfo(VOP_TYPE_FLOAT)) {
			type_infos.append(VOP_TypeInfo(VOP_TYPE_COLOR));
		}
	}
}

void BRDFLayered::getAllowedInputTypesSubclass(unsigned idx, VOP_VopTypeArray &voptypes)
{
	if (idx < NodeBase::orderedInputs()) {
		NodeBase::getAllowedInputTypesSubclass(idx, voptypes);
	}
	else {
		VOP_TypeInfo vopTypeInfo;
		getInputTypeInfoSubclass(vopTypeInfo, idx);
		if (vopTypeInfo == VOP_TypeInfo(VOP_TYPE_BSDF)) {
			voptypes.append(VOP_SURFACE_SHADER);
		}
		if (vopTypeInfo == VOP_TypeInfo(VOP_TYPE_FLOAT)) {
			voptypes.append(VOP_TYPE_COLOR);
		}
	}
}

bool BRDFLayered::willAutoconvertInputType(int idx)
{
	if (idx < int(NodeBase::orderedInputs())) {
		return NodeBase::willAutoconvertInputType(idx);
	}

	VOP_TypeInfo vopTypeInfo;
	getInputTypeInfoSubclass(vopTypeInfo, idx);

	if (vopTypeInfo == VOP_TypeInfo(VOP_TYPE_BSDF)) {
		return true;
	}
	if (vopTypeInfo == VOP_TypeInfo(VOP_TYPE_FLOAT)) {
		return true;
	}

	return false;
}

int BRDFLayered::customInputsCount() const
{
	// One socket for BRDF and one for Weight
	const int numCustomInputs = evalInt("brdf_count", 0, 0.0) * 2;
	return numCustomInputs;
}

void BRDFLayered::getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	type_info.setType(VOP_TYPE_BSDF);
}

unsigned BRDFLayered::getNumVisibleInputs() const
{
	return orderedInputs();
}

unsigned BRDFLayered::orderedInputs() const
{
	int orderedInputs = NodeBase::orderedInputs();
	orderedInputs += customInputsCount();

	return orderedInputs;
}

void BRDFLayered::getCode(UT_String &, const VOP_CodeGenContext &)
{}

OP::VRayNode::PluginResult BRDFLayered::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter)
{
	const fpreal t = exporter.getContext().getTime();

	const int brdfCount = evalInt("brdf_count", 0, 0.0);

	Attrs::QValueList brdfs(brdfCount);
	Attrs::QValueList weights(brdfCount);

	ShaderExporter &shaderExporter = exporter.getShaderExporter();

	UT_String brdfSockName;
	UT_String weightSockName;

	for (int i = 1; i <= brdfCount; ++i) {
		brdfSockName.sprintf("brdf_%i", i);
		weightSockName.sprintf("weight_%i", i);

		const VRay::PluginRef brdfPlugin = shaderExporter.exportConnectedSocket(*this, brdfSockName);
		if (brdfPlugin.isEmpty()) {
			Log::getLog().error("Node \"%s\": Failed to export BRDF node connected to \"%s\", ignoring...",
			                    getName().buffer(), qPrintable(brdfSockName));
		}
		else {
			VRay::PluginRef weightPlugin = shaderExporter.exportConnectedSocket(*this, weightSockName);
			if (weightPlugin.isEmpty()) {
				const fpreal weightValue = evalFloatInst("brdf#weight", &i, 0, t);
				const QString paramPrefix = SL("|weight|%1").arg(QString::number(i));

				Attrs::PluginDesc weightTex(VRayExporter::getPluginName(*this, SL(""), paramPrefix),
				                            SL("TexAColor"));
				weightTex.add(SL("texture"), weightValue, weightValue, weightValue, 1.0f);

				weightPlugin = exporter.exportPlugin(weightTex);
			}

			if (weightPlugin.isEmpty()) {
				Log::getLog().error("Node \"%s\": Failed to export BRDF weight node connected to \"%s\", ignoring...",
				                    getName().buffer(), qPrintable(brdfSockName));
			}
			else {
				brdfs.append(VRay::VUtils::Value(brdfPlugin));
				weights.append(VRay::VUtils::Value(weightPlugin));
			}
		}
	}

	if (brdfs.empty() || weights.empty())
		return PluginResultError;

	pluginDesc.add(SL("brdfs"), brdfs);
	pluginDesc.add(SL("weights"), weights);

	return PluginResultContinue;
}
