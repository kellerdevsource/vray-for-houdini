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

using namespace VRayForHoudini;

// NOTE: Sockets order:
//
//   - Auto. sockets from description
//   - brdf_1
//   - weight_1
//   - ...
//   - brdf_<brdf_count>
//   - weight_<brdf_count>
//

void VOP::BRDFLayered::setPluginType()
{
	pluginType = VRayPluginType::BRDF;
	pluginID = SL("BRDFLayered");
}

const char* VOP::BRDFLayered::inputLabel(unsigned idx) const
{
	const int numBaseInputs = VOP::NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		return VOP::NodeBase::inputLabel(idx);
	}

	const int socketIndex = idx - numBaseInputs;
	const int inputHumanIndex = socketIndex / 2 + 1;

	if (socketIndex % 2) {
		return getCreateSocketLabel(socketIndex, "Weight %i", inputHumanIndex);
	}

	return getCreateSocketLabel(socketIndex, "BRDF %i", inputHumanIndex);
}


const char* VOP::BRDFLayered::outputLabel(unsigned idx) const
{
	return "BRDF";
}


void VOP::BRDFLayered::getOutputNameSubclass(UT_String &name, int idx) const
{
	name = "BRDF";
}


void VOP::BRDFLayered::getInputNameSubclass(UT_String &in, int idx) const
{
	const int numBaseInputs = VOP::NodeBase::orderedInputs();
	if (idx < numBaseInputs) {
		VOP::NodeBase::getInputNameSubclass(in, idx);
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


int VOP::BRDFLayered::getInputFromName(const UT_String &in) const
{
	return getInputFromNameSubclass(in);
}


int VOP::BRDFLayered::getInputFromNameSubclass(const UT_String &in) const
{
	const int numInputs = VOP::NodeBase::orderedInputs();

	int inIdx;
	if (in.startsWith("brdf_") && in != "brdf_count") {
		int idx;
		sscanf(in.buffer(), "brdf_%i", &idx);
		inIdx = numInputs + ((idx - 1) * 2);
	}
	else if (in.startsWith("weight_")) {
		int idx;
		sscanf(in.buffer(), "weight_%i", &idx);
		inIdx = numInputs + ((idx * 2) - 1);
	}
	else {
		inIdx = VOP::NodeBase::getInputFromNameSubclass(in);
	}

	return inIdx;
}


void VOP::BRDFLayered::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	const int numBaseInputs = VOP::NodeBase::orderedInputs();
	if (idx < numBaseInputs) {
		VOP::NodeBase::getInputTypeInfoSubclass(type_info, idx);
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


void VOP::BRDFLayered::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos)
{
	if (idx < VOP::NodeBase::orderedInputs()) {
		VOP::NodeBase::getAllowedInputTypeInfosSubclass(idx, type_infos);
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

void VOP::BRDFLayered::getAllowedInputTypesSubclass(unsigned idx, VOP_VopTypeArray &voptypes)
{
	if (idx < VOP::NodeBase::orderedInputs()) {
		VOP::NodeBase::getAllowedInputTypesSubclass(idx, voptypes);
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


bool VOP::BRDFLayered::willAutoconvertInputType(int idx)
{
	if (idx < VOP::NodeBase::orderedInputs()) {
		return VOP::NodeBase::willAutoconvertInputType(idx);
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


int VOP::BRDFLayered::customInputsCount() const
{
	// One socket for BRDF and one for Weight
	const int numCustomInputs = evalInt("brdf_count", 0, 0.0) * 2;
	return numCustomInputs;
}


void VOP::BRDFLayered::getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	type_info.setType(VOP_TYPE_BSDF);
}


unsigned VOP::BRDFLayered::getNumVisibleInputs() const
{
	return orderedInputs();
}


unsigned VOP::BRDFLayered::orderedInputs() const
{
	int orderedInputs = VOP::NodeBase::orderedInputs();
	orderedInputs += customInputsCount();

	return orderedInputs;
}


void VOP::BRDFLayered::getCode(UT_String&, const VOP_CodeGenContext &)
{}

OP::VRayNode::PluginResult VOP::BRDFLayered::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	const fpreal t = exporter.getContext().getTime();

	const int brdfCount = evalInt("brdf_count", 0, 0.0);

	Attrs::QValueList brdfs(brdfCount);
	Attrs::QValueList weights(brdfCount);

	for (int i = 1; i <= brdfCount; ++i) {
		const QString &paramPrefix = SL("@%1").arg(QString::number(i));
		const QString &brdfSockName = SL("brdf_%1").arg(QString::number(i));

		OP_Node *brdf_node = VRayExporter::getConnectedNode(this, brdfSockName);
		if (!brdf_node) {
			Log::getLog().warning("Node \"%s\": BRDF node is not connected to \"%s\", ignoring...",
			                      getName().buffer(), qPrintable(brdfSockName));
		}
		else {
			const QString weightSockName = SL("weight_%1").arg(QString::number(i));

			VRay::Plugin brdf_plugin = exporter.exportVop(brdf_node, parentContext);
			if (brdf_plugin.isEmpty()) {
				Log::getLog().error("Node \"%s\": Failed to export BRDF node connected to \"%s\", ignoring...",
				                    getName().buffer(), qPrintable(brdfSockName));
			}
			else {
				VRay::Plugin weight_plugin = VRay::Plugin();

				OP_Node *weight_node = VRayExporter::getConnectedNode(this, weightSockName);

				if (weight_node) {
					weight_plugin = exporter.exportVop(weight_node, parentContext);
				}
				else {
					const fpreal weight_value = evalFloatInst("brdf#weight", &i, 0, t);

					Attrs::PluginDesc weight_tex(VRayExporter::getPluginName(*this, paramPrefix),
					                             SL("TexAColor"));
					weight_tex.add(Attrs::PluginAttr(SL("texture"), weight_value, weight_value, weight_value, 1.0f));

					weight_plugin = exporter.exportPlugin(weight_tex);
				}

				if (weight_plugin.isEmpty()) {
					Log::getLog().error("Node \"%s\": Failed to export BRDF weight node connected to \"%s\", ignoring...",
								getName().buffer(), qPrintable(brdfSockName));
				}
				else {
					// Convert weight plugin.
					exporter.convertInputPlugin(weight_plugin, pluginDesc, this, VOP_TYPE_FLOAT, weightSockName);
					weights.append(VRay::VUtils::Value(weight_plugin));

					// Convert BRDF plugin.
					exporter.convertInputPlugin(brdf_plugin, pluginDesc, this, VOP_TYPE_BSDF, brdfSockName);
					weights.append(VRay::VUtils::Value(brdf_plugin));
				}
			}
		}
	}

	if (brdfs.empty())
		return PluginResultError;

	pluginDesc.add(SL("brdfs"), brdfs);
	pluginDesc.add(SL("weights"), weights);

	return PluginResultContinue;
}
