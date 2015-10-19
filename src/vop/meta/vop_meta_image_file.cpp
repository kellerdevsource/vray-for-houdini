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
static AttributesTabs     AttrTabs;
static Parm::PRMTmplList  AttrItems;


PRM_Template* VOP::MetaImageFile::GetPrmTemplate()
{
	if (AttrItems.size()) {
		return &AttrItems[0];
	}

	AttrTabs.push_back(AttributesTab("Bitmap",
									 "BitmapBuffer",
									 Parm::GeneratePrmTemplate("TEXTURE", "BitmapBuffer", true, true, "VOP")));
	AttrTabs.push_back(AttributesTab("Texture",
								     "TexBitmap",
								     Parm::GeneratePrmTemplate("TEXTURE", "TexBitmap", true, true, "VOP")));
	AttrTabs.push_back(AttributesTab("UV",
									 "UVWGenMayaPlace2dTexture",
									 Parm::GeneratePrmTemplate("TEXTURE", "UVWGenMayaPlace2dTexture", true, true, "VOP")));
	AttrTabs.push_back(AttributesTab("Projection",
									 "UVWGenProjection",
									 Parm::GeneratePrmTemplate("TEXTURE", "UVWGenProjection", true, true, "VOP")));

	// TODO: Move to some function
	//
	for (const auto &tab : AttrTabs) {
		PRM_Template *prm = tab.items;
		int           prm_count = 0;
		while (prm->getType() != PRM_LIST_TERMINATOR) {
			prm_count++;
			prm++;
		}

		AttrTabsSwitcherTitles.push_back(PRM_Default(prm_count, tab.label.c_str()));
		for (int i = 0; i < prm_count; ++i) {
			AttrItems.push_back(tab.items[i]);
		}
	}

	AttrItems.insert(AttrItems.begin(),
					 PRM_Template(PRM_SWITCHER,
								  AttrTabsSwitcherTitles.size(),
								  &AttrTabsSwitcher,
								  &AttrTabsSwitcherTitles[0]));

	return &AttrItems[0];
}


void VOP::MetaImageFile::setPluginType()
{
	pluginType = "TEXTURE";

	// Base plugin
	pluginID = "CustomTexBitmap";
}


OP::VRayNode::PluginResult VOP::MetaImageFile::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	// TODO:

	return OP::VRayNode::PluginResultNA;
}
