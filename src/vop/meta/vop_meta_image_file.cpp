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


PRM_Template* VOP::MetaImageFile::GetPrmTemplate()
{
	static Parm::PRMList myPrmList;
	if (myPrmList.empty()) {
		myPrmList.reserve(90);

		myPrmList.switcherBegin("MetaImageFile");

		// Bitmap tab
		myPrmList.addFolder("Bitmap");

		int idx = myPrmList.size();
		Parm::addPrmTemplateForPlugin( "BitmapBuffer", myPrmList);
		// hacky way to rename params on order to keep compatibility with old UI
		Parm::PRMList::renamePRMTemplate(myPrmList.getPRMTemplate() + idx, "BitmapBuffer");

		// Texture tab
		myPrmList.addFolder("Texture");

		idx = myPrmList.size();
		Parm::addPrmTemplateForPlugin( "TexBitmap", myPrmList);
		// hacky way to rename params on order to keep compatibility with old UI
		Parm::PRMList::renamePRMTemplate(myPrmList.getPRMTemplate() + idx, "TexBitmap");

		// UV tab
		myPrmList.addFolder("UV");

		idx = myPrmList.size();
		Parm::addPrmTemplateForPlugin( "UVWGenMayaPlace2dTexture", myPrmList);
		// hacky way to rename params on order to keep compatibility with old UI
		Parm::PRMList::renamePRMTemplate(myPrmList.getPRMTemplate() + idx, "UVWGenMayaPlace2dTexture");

		// Projection tab
		myPrmList.addFolder("Projection");

		idx = myPrmList.size();
		Parm::addPrmTemplateForPlugin( "UVWGenProjection", myPrmList);
		// hacky way to rename params on order to keep compatibility with old UI
		Parm::PRMList::renamePRMTemplate(myPrmList.getPRMTemplate() + idx, "UVWGenProjection");

		myPrmList.switcherEnd();
	}

	return myPrmList.getPRMTemplate();
}


void VOP::MetaImageFile::setPluginType()
{
	pluginType = VRayPluginType::TEXTURE;

	// Base plugin
	pluginID = "CustomTexBitmap";
}


OP::VRayNode::PluginResult VOP::MetaImageFile::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	// TODO:

	return OP::VRayNode::PluginResultNA;
}
