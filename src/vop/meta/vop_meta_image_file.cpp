//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_meta_image_file.h"

#include "vfh_prm_templates.h"
#include "vfh_prm_json.h"
#include "vfh_tex_utils.h"


using namespace VRayForHoudini;


static PRM_Name           AttrTabsSwitcher("MetaImageFile");
static Parm::PRMDefList   AttrTabsSwitcherTitles;
static Parm::PRMTmplList  AttrItems;


static Parm::TabItemDesc MetaImageFileTabItemsDesc[] = {
	{ "Bitmap",     "BitmapBuffer"             },
	{ "Texture",    "TexBitmap"                },
	{ "UV",         "UVWGenMayaPlace2dTexture" },
	{ "Projection", "UVWGenProjection"         }
};


PRM_Template* VOP::MetaImageFile::GetPrmTemplate()
{
	if (!AttrItems.size()) {
		Parm::addTabItems(MetaImageFileTabItemsDesc, CountOf(MetaImageFileTabItemsDesc), AttrTabsSwitcherTitles, AttrItems);

		AttrItems.push_back(PRM_Template()); // List terminator

		AttrItems.insert(AttrItems.begin(),
						 PRM_Template(PRM_SWITCHER,
									  AttrTabsSwitcherTitles.size(),
									  &AttrTabsSwitcher,
									  &AttrTabsSwitcherTitles[0]));
	}

	return &AttrItems[0];
}


void VOP::MetaImageFile::setPluginType()
{
	pluginType = "TEXTURE";

	// Base plugin
	pluginID = "CustomTexBitmap";
}


OP::VRayNode::PluginResult VOP::MetaImageFile::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, OP_Node *parent)
{
	// TODO:

	return OP::VRayNode::PluginResultNA;
}
