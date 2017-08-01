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

static enum UvwMenuSelection {
	UVWGenChannel            = 0,
	UVWGenEnvironment        = 1,
	UVWGenExplicit           = 2,
	UVWGenMayaPlace2dTexture = 3,
	UVWGenObject             = 4,
	UVWGenObjectBBox         = 5,
	UVWGenPlanarWorld        = 6,
	UVWGenProjection         = 7
};

static MetaImageFileOutputSocket metaImageFileOutputSockets[] = {
	{ "color", VOP_TypeInfo(VOP_TYPE_COLOR) },
	{ "color_transperancy", VOP_TypeInfo(VOP_TYPE_COLOR) },
	{ "out_alpha", VOP_TypeInfo(VOP_TYPE_FLOAT) },
	{ "out_intensity", VOP_TypeInfo(VOP_TYPE_FLOAT) }
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
	//this will have to change to acomodate for different UVWGen types
	Attrs::PluginDesc bitmapBufferDesc(VRayExporter::getPluginName(*this, "BitmapBuffer"), "BitmapBuffer");
	exporter.setAttrsFromOpNodePrms(bitmapBufferDesc, this, "meta_image_bimap_buffer_");

	const fpreal &t = exporter.getContext().getTime();
	const int selectedUVGen = evalInt("meta_image_uv_generator", 0, t);
	
	const UT_String uvGenOptions[] = {
		"UVWGenChannel",
		"UVWGenEnvironment",
		"UVWGenExplicit",
		"UVWGenMayaPlace2dTexture",
		"UVWGenObject",
		"UVWGenObjectBBox",
		"UVWGenPlanarWorld",
		"UVWGenProjection"
	};
	
	UT_String selectedUVWGen = "";

	vassert(selectedUVGen > -1 && selectedUVGen < COUNT_OF(uvGenOptions));

	selectedUVWGen = uvGenOptions[selectedUVGen];
	
	
	Attrs::PluginDesc selectedUVPluginDesc(VRayExporter::getPluginName(*this, selectedUVWGen.c_str()), selectedUVWGen.c_str());
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
	if (idx >= 0 && idx < 4) {
		type_info.setType(metaImageFileOutputSockets[idx].typeInfo.getType());
	}
}

//define inputs
//TODO: Change input number and type based on selected UVW
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
