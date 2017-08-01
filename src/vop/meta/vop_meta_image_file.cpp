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


using namespace VRayForHoudini;

struct MetaImageFileOutputSocket {
	const char *label;
	const VOP_TypeInfo typeInfo;
};

static MetaImageFileOutputSocket metaImageFileOutputSockets[] = {
	{ "Output",   VOP_TypeInfo(VOP_TYPE_COLOR) },
	{ "color", VOP_TypeInfo(VOP_TYPE_COLOR) },
	{ "color_transperancy", VOP_TypeInfo(VOP_TYPE_COLOR) },
	{ "out_alpha", VOP_TypeInfo(VOP_TYPE_FLOAT) },
	{ "out_intensity", VOP_TypeInfo(VOP_TYPE_FLOAT) }
};

static UT_String pluginIds[] = {
	"UVWGenChannel",
	"UVWGenEnvironment",
	"UVWGenExplicit",
	"UVWGenMayaPlace2dTexture",
	"UVWGenObject",
	"UVWGenObjectBBox",
	"UVWGenPlanarWorld",
	"UVWGenProjection"
};

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
	//this will have to change to acomodate for different UVWGen types
	Attrs::PluginDesc bitmapBufferDesc(VRayExporter::getPluginName(*this, "BitmapBuffer"), "BitmapBuffer");
	exporter.setAttrsFromOpNodePrms(bitmapBufferDesc, this, "meta_image_bimap_buffer_");

	const fpreal &t = exporter.getContext().getTime();
	const int selectedUVGen = evalInt("meta_image_uv_generator", 0, t);
	UT_String selectedUVWGen = "";
	switch (selectedUVGen) {
	case 0:
		selectedUVWGen = "meta_image_channel";
		break;

	case 1:
		selectedUVWGen = "meta_image_environment";
		break;

	case 2:
		selectedUVWGen = "meta_image_explicit";
		break;

	case 3:
		selectedUVWGen = "meta_image_maya_place_2d";
		break;

	case 4:
		selectedUVWGen = "meta_image_object";
		break;

	case 5:
		selectedUVWGen = "meta_image_object_bbox";
		break;

	case 6:
		selectedUVWGen = "meta_image_planar_world";
		break;

	case 7:
		selectedUVWGen = "meta_image_projection";
		break;

	default:
		selectedUVWGen = "meta_image_channel";
		break;
	}
	//this probably doesn't work
	selectedUVWGen += "_";
	Attrs::PluginDesc selectedUVPluginDesc(VRayExporter::getPluginName(*this, selectedUVWGen.c_str()),pluginIds[selectedUVGen].c_str());
	exporter.setAttrsFromOpNodePrms(selectedUVPluginDesc, this, selectedUVWGen.c_str());

	pluginDesc.addAttribute(Attrs::PluginAttr("bitmap", exporter.exportPlugin(bitmapBufferDesc)));
	pluginDesc.addAttribute(Attrs::PluginAttr("uvwgen", exporter.exportPlugin(selectedUVPluginDesc)));
	exporter.setAttrsFromOpNodePrms(pluginDesc, this, "TexBitmap_");

	return OP::VRayNode::PluginResultContinue;
}

//define outputs
unsigned VOP::MetaImageFile::getNumVisibleOutputs() const {
	return 5;
}



unsigned VOP::MetaImageFile::maxOutputs() const {
	return 5;
}

const char* VOP::MetaImageFile::outputLabel(unsigned idx) const {
	if (idx >= 0 && idx < 5) {
		return metaImageFileOutputSockets[idx].label;
	}

	return nullptr;
}

void VOP::MetaImageFile::getOutputNameSubclass(UT_String &out, int idx) const {
	if (idx >= 0 && idx < 5) {
		out = metaImageFileOutputSockets[idx].label;
	}
	else {
		out = "unknown";
	}

}

int VOP::MetaImageFile::getOutputFromName(const UT_String &out) const {
	for (int idx = 0; idx < 5; idx++) {
		if (out.equal(metaImageFileOutputSockets[idx].label)) {
			return idx;
		}
	}

	return -1;
}

void VOP::MetaImageFile::getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) {
	if (idx >= 0 && idx < 5) {
		type_info.setType(metaImageFileOutputSockets[idx].typeInfo.getType());
	}
}

//define inputs, we are supposed to have no inputs, only one output
const char *VOP::MetaImageFile::inputLabel(unsigned idx) const {
	return nullptr;
}

void VOP::MetaImageFile::getInputNameSubclass(UT_String &in, int idx) const {
	in = "unknown";
}

int VOP::MetaImageFile::getInputFromNameSubclass(const UT_String &out) const {
	return -1;
}

unsigned VOP::MetaImageFile::orderedInputs() const {
	return 0;
}

unsigned VOP::MetaImageFile::getNumVisibleInputs() const {
	return 0;
}
//register?
