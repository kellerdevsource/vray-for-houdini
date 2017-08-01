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

void VOP::MetaImageFile::setPluginType()
{
	pluginType = VRayPluginType::TEXTURE;
	pluginID = "MetaImageFile";
}

void VOP::MetaImageFile::init()
{
	VRayNode::init();
}

OP::VRayNode::PluginResult VOP::MetaImageFile::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	// Base plugin
	pluginDesc.pluginID = "TexBitmap";

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

	vassert(selectedUVGen >= 0 && selectedUVGen < COUNT_OF(uvGenOptions));

	UT_String selectedUVWGen = uvGenOptions[selectedUVGen];

	Attrs::PluginDesc bitmapBufferDesc(VRayExporter::getPluginName(*this, "BitmapBuffer"), "BitmapBuffer");
	exporter.setAttrsFromOpNodePrms(bitmapBufferDesc, this, "BitmapBuffer_");
	
	Attrs::PluginDesc selectedUVPluginDesc(VRayExporter::getPluginName(*this, selectedUVWGen.c_str()), selectedUVWGen.c_str());
	selectedUVWGen += "_";
	exporter.setAttrsFromOpNodePrms(selectedUVPluginDesc, this, selectedUVWGen.c_str());

	pluginDesc.addAttribute(Attrs::PluginAttr("bitmap", exporter.exportPlugin(bitmapBufferDesc)));
	pluginDesc.addAttribute(Attrs::PluginAttr("uvwgen", exporter.exportPlugin(selectedUVPluginDesc)));
	exporter.setAttrsFromOpNodePrms(pluginDesc, this, "TexBitmap_");

	return OP::VRayNode::PluginResultContinue;
}
