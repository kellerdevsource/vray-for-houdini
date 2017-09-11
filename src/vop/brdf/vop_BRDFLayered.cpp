//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <boost/format.hpp>

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
	pluginID   = "BRDFLayered";
}

const char* VOP::BRDFLayered::inputLabel(unsigned idx) const
{
	int numBaseInputs = VOP::NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		return VOP::NodeBase::inputLabel(idx);
	}
	else {
		const int socketIndex = idx - numBaseInputs;
		const int inputHumanIndex = ((socketIndex) / 2) + 1;

		const std::string &label = (socketIndex % 2)
								   ? boost::str(boost::format("Weight %i") % inputHumanIndex)
								   : boost::str(boost::format("BRDF %i")   % inputHumanIndex);

		return label.c_str();
	}
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
	int numBaseInputs = VOP::NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		VOP::NodeBase::getInputNameSubclass(in, idx);
	}
	else {
		const int socketIndex = idx - numBaseInputs;
		const int inputHumanIndex = ((socketIndex) / 2) + 1;

		if (socketIndex % 2) {
			in = boost::str(boost::format("weight_%i") % inputHumanIndex);
		}
		else {
			in = boost::str(boost::format("brdf_%i") % inputHumanIndex);
		}
	}
}


int VOP::BRDFLayered::getInputFromName(const UT_String &in) const
{
	return getInputFromNameSubclass(in);
}


int VOP::BRDFLayered::getInputFromNameSubclass(const UT_String &in) const
{
	int numInputs = VOP::NodeBase::orderedInputs();
	int inIdx = 0;

	if (in.startsWith("brdf_")) {
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
	int numBaseInputs = VOP::NodeBase::orderedInputs();

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
		VOP_TypeInfo type_info(VOP_TYPE_BSDF);
		type_infos.append(type_info);
	}
}


int VOP::BRDFLayered::customInputsCount() const
{
	// One socket for BRDF and one for Weight
	int numCustomInputs = evalInt("brdf_count", 0, 0.0) * 2;

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


void VOP::BRDFLayered::getCode(UT_String &codestr, const VOP_CodeGenContext &)
{
}


OP::VRayNode::PluginResult VOP::BRDFLayered::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	const fpreal &t = exporter.getContext().getTime();

	const int brdf_count = evalInt("brdf_count", 0, 0.0);

	VRay::ValueList brdfs;
	VRay::ValueList weights;

	for (int i = 1; i <= brdf_count; ++i) {
		const std::string &paramPrefix = boost::str(boost::format("@%i") % i);

		const std::string &brdfSockName = boost::str(boost::format("brdf_%i") % i);

		OP_Node *brdf_node = VRayExporter::getConnectedNode(this, brdfSockName);
		if (NOT(brdf_node)) {
			Log::getLog().warning("Node \"%s\": BRDF node is not connected to \"%s\", ignoring...",
					   getName().buffer(), brdfSockName.c_str());
		}
		else {
			const std::string &weightSockName = boost::str(boost::format("weight_%i") % i);

			VRay::Plugin brdf_plugin = exporter.exportVop(brdf_node, parentContext);
			if (NOT(brdf_plugin)) {
				Log::getLog().error("Node \"%s\": Failed to export BRDF node connected to \"%s\", ignoring...",
							getName().buffer(), brdfSockName.c_str());
			}
			else {
				VRay::Plugin weight_plugin = VRay::Plugin();

				OP_Node *weight_node = VRayExporter::getConnectedNode(this, weightSockName);
				if (weight_node) {
					weight_plugin = exporter.exportVop(weight_node, parentContext);
				}
				else {
					const fpreal weight_value = evalFloatInst("brdf#weight", &i, 0, t);

					Attrs::PluginDesc weight_tex(VRayExporter::getPluginName(this, paramPrefix), "TexAColor");
					weight_tex.add(Attrs::PluginAttr("texture", weight_value, weight_value, weight_value, 1.0f));

					weight_plugin = exporter.exportPlugin(weight_tex);
				}

				if (NOT(weight_plugin)) {
					Log::getLog().error("Node \"%s\": Failed to export BRDF weight node connected to \"%s\", ignoring...",
								getName().buffer(), brdfSockName.c_str());
				}
				else {
					brdfs.push_back(VRay::Value(brdf_plugin));
					weights.push_back(VRay::Value(weight_plugin));
				}
			}
		}
	}

	if (NOT(brdfs.size())) {
		return PluginResult::PluginResultError;
	}

	pluginDesc.add(Attrs::PluginAttr("brdfs", brdfs));
	pluginDesc.add(Attrs::PluginAttr("weights", weights));

	return PluginResult::PluginResultContinue;
}
