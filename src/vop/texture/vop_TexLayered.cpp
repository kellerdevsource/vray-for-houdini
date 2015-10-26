//
// Copyright (c) 2015, Chaos Software Ltd
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


static PRM_Name  rpm_name_tex_count("textures_count", "Texture Count");
static PRM_Range rpm_range_tex_count(PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 8);

static PRM_Name  rpm_name_tex_blend_mode("tex#blend_mode", "Blend Mode #");
static PRM_Name  rpm_name_tex_blend_mode_items[] = {
	PRM_Name("None"),
	PRM_Name("Over"),
	PRM_Name("In"),
	PRM_Name("Out"),
	PRM_Name("Add"),
	PRM_Name("Subtract"),
	PRM_Name("Multiply"),
	PRM_Name("Difference"),
	PRM_Name("Lighten"),
	PRM_Name("Darken"),
	PRM_Name("Saturate"),
	PRM_Name("Desaturate"),
	PRM_Name("Illuminate"),
	PRM_Name(),
};
static PRM_ChoiceList rpm_name_tex_blend_mode_menu(PRM_CHOICELIST_SINGLE, rpm_name_tex_blend_mode_items);

static PRM_Name  rpm_name_tex_blend_amount("tex#blend_amount", "Blend Amount #");
static PRM_Range rpm_range_tex_blend_amount(PRM_RANGE_RESTRICTED, 0.0, PRM_RANGE_RESTRICTED, 1.0);

static PRM_Template rpm_tmpl_textures[] = {
	PRM_Template(PRM_ORD, 1, &rpm_name_tex_blend_mode, PRMoneDefaults, &rpm_name_tex_blend_mode_menu),
	PRM_Template(PRM_FLT, 1, &rpm_name_tex_blend_amount, PRMoneDefaults, /*choicelist*/ nullptr, &rpm_range_tex_blend_amount),
	PRM_Template()
};


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
	pluginType = "TEXTURE";
	pluginID   = "TexLayered";
}


void VOP::TexLayered::addPrmTemplate(Parm::PRMTmplList &prmTemplate)
{
	prmTemplate.push_back(PRM_Template(PRM_MULTITYPE_LIST, rpm_tmpl_textures, 1, &rpm_name_tex_count, /*choicelist*/ nullptr, &rpm_range_tex_count));
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
	int numCustomInputs = evalInt(rpm_name_tex_count.getToken(), 0, 0.0);

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


OP::VRayNode::PluginResult VOP::TexLayered::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, OP_Node *parent)
{
	const fpreal &t = exporter.getContext().getTime();

	const int tex_count = evalInt(rpm_name_tex_count.getToken(), 0, 0.0);

	VRay::ValueList textures;
	VRay::IntList   blend_modes;

	for (int i = 1; i <= tex_count; ++i) {
		const std::string &paramPrefix = boost::str(boost::format("@%i") % i);

		const std::string &texSockName = boost::str(boost::format("tex_%i") % i);

		OP_Node *tex_node = VRayExporter::getConnectedNode(this, texSockName);
		if (NOT(tex_node)) {
			PRINT_WARN("Node \"%s\": Texture node is not connected to \"%s\", ignoring...",
					   getName().buffer(), texSockName.c_str());
		}
		else {
			VRay::Plugin tex_plugin = exporter.exportVop(tex_node);
			if (NOT(tex_plugin)) {
				PRINT_ERROR("Node \"%s\": Failed to export texture node connected to \"%s\", ignoring...",
							getName().buffer(), texSockName.c_str());
			}
			else {
				const int   blend_mode   = evalIntInst(rpm_name_tex_blend_mode.getToken(), &i, 0, t);
				const float blend_amount = evalFloatInst(rpm_name_tex_blend_amount.getToken(), &i, 0, t);

				if (blend_amount != 1.0f) {
					Attrs::PluginDesc blendDesc(VRayExporter::getPluginName(this, paramPrefix), "TexAColorOp");
					blendDesc.pluginAttrs.push_back(Attrs::PluginAttr("mode", 0)); // Mode: "result_a"
					blendDesc.pluginAttrs.push_back(Attrs::PluginAttr("color_a", tex_plugin));
					blendDesc.pluginAttrs.push_back(Attrs::PluginAttr("mult_a", 1.0f));
					blendDesc.pluginAttrs.push_back(Attrs::PluginAttr("result_alpha", blend_amount));

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

	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("textures", textures));
	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("blend_modes", blend_modes));

	return PluginResult::PluginResultContinue;
}
