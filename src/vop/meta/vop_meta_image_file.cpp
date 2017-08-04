//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_meta_image_file.h"
#include "vfh_prm_templates.h"
#include "vfh_tex_utils.h"
#include <map>
#include <vector>

using namespace VRayForHoudini;

struct MetaImageFileSocket {
	const char *label;
	const VOP_TypeInfo typeInfo;
};

static MetaImageFileSocket metaImageFileOutputSockets[] = {
	{ "color", VOP_TypeInfo(VOP_TYPE_COLOR) },
	{ "out_transparency", VOP_TypeInfo(VOP_TYPE_COLOR) },
	{ "out_alpha", VOP_TypeInfo(VOP_TYPE_FLOAT) },
	{ "out_intensity", VOP_TypeInfo(VOP_TYPE_FLOAT) }
};

static enum MenuOption {
	UVWGenMayaPlace2dTexture = 0,
	UVWGenEnvironment = 1,
	UVWGenExplicit = 2,
	UVWGenChannel = 3,
	UVWGenObject = 4,
	UVWGenObjectBBox = 5,
	UVWGenPlanarWorld = 6,
	UVWGenProjection = 7
} current;

static std::map<MenuOption, std::vector<MetaImageFileSocket>> inputsMap = {
	{ UVWGenMayaPlace2dTexture, { { "uvwgen", VOP_TypeInfo(VOP_TYPE_VECTOR) },
									{ "coverage_u_tex", VOP_TypeInfo(VOP_TYPE_FLOAT) },
									{ "coverage_v_tex", VOP_TypeInfo(VOP_TYPE_FLOAT) },
									{ "translate_frame_u_tex", VOP_TypeInfo(VOP_TYPE_FLOAT) },
									{ "translate_frame_v_tex", VOP_TypeInfo(VOP_TYPE_FLOAT) },
									{ "rotate_frame_tex", VOP_TypeInfo(VOP_TYPE_FLOAT) },
									{ "repeat_u_tex", VOP_TypeInfo(VOP_TYPE_FLOAT) },
									{ "repeat_v_tex", VOP_TypeInfo(VOP_TYPE_FLOAT) },
									{ "offset_u_tex", VOP_TypeInfo(VOP_TYPE_FLOAT) },
									{ "offset_v_tex", VOP_TypeInfo(VOP_TYPE_FLOAT) },
									{ "rotate_uv_tex", VOP_TypeInfo(VOP_TYPE_FLOAT) },
									{ "noise_u_tex", VOP_TypeInfo(VOP_TYPE_FLOAT) },
									{ "noise_v_tex", VOP_TypeInfo(VOP_TYPE_FLOAT) },
									{ "uvw_channel_tex", VOP_TypeInfo(VOP_TYPE_INTEGER) } }},
	{ UVWGenEnvironment, { { "uvw_matrix", VOP_TypeInfo(VOP_TYPE_MATRIX3) }, 
							{ "uvw_transform", VOP_TypeInfo(VOP_TYPE_MATRIX4) },
							{ "ground_position", VOP_TypeInfo(VOP_TYPE_VECTOR) } }},
	{ UVWGenExplicit,{ { "u", VOP_TypeInfo(VOP_TYPE_FLOAT) },
						{ "v", VOP_TypeInfo(VOP_TYPE_FLOAT) },
						{ "w", VOP_TypeInfo(VOP_TYPE_FLOAT) },
						{ "uvw", VOP_TypeInfo(VOP_TYPE_COLOR) } }},
	{ UVWGenChannel, { { "uvw_transform", VOP_TypeInfo(VOP_TYPE_MATRIX4) }, 
						{ "uvw_transform tex", VOP_TypeInfo() },
						{ "tex_transfrom", VOP_TypeInfo(VOP_TYPE_MATRIX4) },
						{ "coverage", VOP_TypeInfo(VOP_TYPE_VECTOR) },
						{ "uvwgen", VOP_TypeInfo(VOP_TYPE_VECTOR) }	}},
	{ UVWGenObject, { { "uvw_transform", VOP_TypeInfo(VOP_TYPE_MATRIX4) } }},
	{ UVWGenObjectBBox, { { "bbox_min", VOP_TypeInfo(VOP_TYPE_VECTOR) },
							{ "bbox_max", VOP_TypeInfo(VOP_TYPE_VECTOR) },
							{ "basemtl", VOP_TypeInfo(VOP_TYPE_UNDEF) } }},
	{ UVWGenPlanarWorld, { { "uvw_transform", VOP_TypeInfo(VOP_TYPE_MATRIX4) }, 
							{ "uvw_transform tex", VOP_TypeInfo() },
							{ "tex_transfrom", VOP_TypeInfo(VOP_TYPE_MATRIX4) },
							{ "coverage", VOP_TypeInfo(VOP_TYPE_VECTOR) } } },
	{ UVWGenProjection, { { "uvw_transform", VOP_TypeInfo(VOP_TYPE_MATRIX4) },
							{ "uvw_transform tex", VOP_TypeInfo(VOP_TYPE_UNDEF) },
							{ "tex_transfrom", VOP_TypeInfo(VOP_TYPE_MATRIX4) }, 
							{ "camera_settings", VOP_TypeInfo(VOP_TYPE_UNDEF) },
							{ "camera_view", VOP_TypeInfo(VOP_TYPE_UNDEF) },
							{ "bitmap", VOP_TypeInfo(VOP_TYPE_UNDEF) } } }
};

static const int ouputSocketCount = COUNT_OF(metaImageFileOutputSockets);

PRM_Template* VOP::MetaImageFile::GetPrmTemplate()
{
	static Parm::PRMList myPrmList;
	if (myPrmList.empty()) {
		UT_String uiPath = getenv("VRAY_UI_DS_PATH");
		myPrmList.addFromFile(Parm::expandUiPath("MetaImage.ds").c_str(), uiPath.buffer());
	}

	return myPrmList.getPRMTemplate();
}

void VOP::MetaImageFile::setPluginType()
{
	pluginType = VRayPluginType::TEXTURE;

	// Base plugin
	pluginID = "TexBitmap";
}

OP::VRayNode::PluginResult VOP::MetaImageFile::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	Attrs::PluginDesc bitmapBufferDesc(VRayExporter::getPluginName(*this, "BitmapBuffer"), "BitmapBuffer");
	exporter.setAttrsFromOpNodePrms(bitmapBufferDesc, this, "meta_image_bimap_buffer_");

	const fpreal &t = exporter.getContext().getTime();
	const int selectedUVGen = evalInt("meta_image_uv_generator", 0, t);
	
	const UT_String uvGenOptions[] = {
		"UVWGenMayaPlace2dTexture",
		"UVWGenEnvironment",
		"UVWGenExplicit",
		"UVWGenChannel",
		"UVWGenObject",
		"UVWGenObjectBBox",
		"UVWGenPlanarWorld",
		"UVWGenProjection"
	};

	vassert(selectedUVGen >= 0 && selectedUVGen < COUNT_OF(uvGenOptions));

	UT_String selectedUVWGen = uvGenOptions[selectedUVGen];
	MenuOption current = static_cast<MenuOption>(selectedUVGen);

	Attrs::PluginDesc selectedUVPluginDesc(VRayExporter::getPluginName(*this, selectedUVWGen.c_str()), selectedUVWGen.c_str());

	std::vector<MetaImageFileSocket> temp = inputsMap.at(current);
	for (int i = 0; i < temp.size(); i++) {
		const int idx = getInputFromName(temp.at(i).label);
		OP_Node *connectedInput = getInput(idx);
		if (connectedInput) {
			VRay::Plugin connectedPlugin = exporter.exportVop(connectedInput, parentContext);
			if (connectedPlugin) {
				const Parm::SocketDesc *fromSocketInfo = exporter.getConnectedOutputType(this, temp.at(i).label);
				selectedUVPluginDesc.addAttribute(Attrs::PluginAttr(temp.at(i).label, connectedPlugin, fromSocketInfo->name.getToken()));
			}
		}
	}
	selectedUVWGen += "_";
	exporter.setAttrsFromOpNodePrms(selectedUVPluginDesc, this, selectedUVWGen.c_str());

	pluginDesc.addAttribute(Attrs::PluginAttr("bitmap", exporter.exportPlugin(bitmapBufferDesc)));
	pluginDesc.addAttribute(Attrs::PluginAttr("uvwgen", exporter.exportPlugin(selectedUVPluginDesc)));
	exporter.setAttrsFromOpNodePrms(pluginDesc, this, "TexBitmap_");

	return OP::VRayNode::PluginResultContinue;
}

//define outputs
unsigned VOP::MetaImageFile::getNumVisibleOutputs() const {
	return ouputSocketCount;
}

unsigned VOP::MetaImageFile::maxOutputs() const {
	return ouputSocketCount;
}

const char* VOP::MetaImageFile::outputLabel(unsigned idx) const {
	if (idx >= 0 && idx < ouputSocketCount) {
		return metaImageFileOutputSockets[idx].label;
	}

	return nullptr;
}

void VOP::MetaImageFile::getOutputNameSubclass(UT_String &out, int idx) const {
	if (idx >= 0 && idx < ouputSocketCount) {
		out = metaImageFileOutputSockets[idx].label;
	}
	else {
		out = "unknown";
	}

}

int VOP::MetaImageFile::getOutputFromName(const UT_String &out) const {
	for (int idx = 0; idx < ouputSocketCount; idx++) {
		if (out.equal(metaImageFileOutputSockets[idx].label)) {
			return idx;
		}
	}

	return -1;
}

void VOP::MetaImageFile::getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) {
	if (idx >= 0 && idx < ouputSocketCount) {
		type_info.setType(metaImageFileOutputSockets[idx].typeInfo.getType());
	}
}

//define inputs

const char *VOP::MetaImageFile::inputLabel(unsigned idx) const {
	return inputsMap.at(current).at(idx).label;
}

void VOP::MetaImageFile::getInputNameSubclass(UT_String &in, int idx) const {
	if (idx < inputsMap.at(current).size() || idx >= 0) {
		in = inputsMap.at(current).at(idx).label;
	}
	else {
		in = "unknown";
	}
}

int VOP::MetaImageFile::getInputFromNameSubclass(const UT_String &out) const {
	const std::vector<MetaImageFileSocket> &temp = inputsMap.at(current);

	for (int i = 0; i < temp.size(); i++) {
		if (out == temp.at(i).label)
			return i;
	}
	
	return -1;
}

unsigned VOP::MetaImageFile::getNumVisibleInputs() const {
	current = static_cast<MenuOption>(evalInt("meta_image_uv_generator", 0, 0.0));
	return orderedInputs();
}

unsigned VOP::MetaImageFile::orderedInputs() const {
	return inputsMap.at(current).size();
}

void VOP::MetaImageFile::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) {
	type_info = inputsMap.at(current).at(idx).typeInfo;
}