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


static void renamePRMTemplate(PRM_Template *tmpl, const char *prefix)
{
	if (!UTisstring(prefix)){
		return;
	}

	static boost::format prmname("%s_%s");

	int i = 0;
	while (tmpl && (tmpl[i].getType() != PRM_LIST_TERMINATOR)) {
		if (tmpl[i].getType() == PRM_SWITCHER) {
			continue;
		}

		PRM_Name *name = tmpl[i].getNamePtr();
		if (name) {
			std::string prmtoken = boost::str(prmname % prefix % name->getToken()) ;
			name->setToken(prmtoken.c_str());
			name->harden();
		}
		++i;
	}
}

PRM_Template* VOP::MetaImageFile::GetPrmTemplate()
{
	static Parm::PRMList myPrmList;
	if (myPrmList.empty()) {
		myPrmList.reserve(90);

		static boost::format dspath("plugins/%s.ds");
		std::string dsfullpath;

		myPrmList.switcherBegin("MetaImageFile");

		// Bitmap tab
		myPrmList.addFolder("Bitmap");
		dsfullpath = Parm::PRMList::expandUiPath( boost::str(dspath % "BitmapBuffer") );

		int idx = myPrmList.size();
		myPrmList.addFromFile(dsfullpath.c_str());
		// hacky way to rename params on order to keep compatibility with old UI
		renamePRMTemplate(myPrmList.getPRMTemplate() + idx, "BitmapBuffer");

		// Texture tab
		myPrmList.addFolder("Texture");
		dsfullpath = Parm::PRMList::expandUiPath( boost::str(dspath % "TexBitmap") );

		idx = myPrmList.size();
		myPrmList.addFromFile(dsfullpath.c_str());
		// hacky way to rename params on order to keep compatibility with old UI
		renamePRMTemplate(myPrmList.getPRMTemplate() + idx, "TexBitmap");

		// UV tab
		myPrmList.addFolder("UV");
		dsfullpath = Parm::PRMList::expandUiPath( boost::str(dspath % "UVWGenMayaPlace2dTexture") );

		idx = myPrmList.size();
		myPrmList.addFromFile(dsfullpath.c_str());
		// hacky way to rename params on order to keep compatibility with old UI
		renamePRMTemplate(myPrmList.getPRMTemplate() + idx, "UVWGenMayaPlace2dTexture");

		// Projection tab
		myPrmList.addFolder("Projection");
		dsfullpath = Parm::PRMList::expandUiPath( boost::str(dspath % "UVWGenProjection") );

		idx = myPrmList.size();
		myPrmList.addFromFile(dsfullpath.c_str());
		// hacky way to rename params on order to keep compatibility with old UI
		renamePRMTemplate(myPrmList.getPRMTemplate() + idx, "UVWGenProjection");

		myPrmList.switcherEnd();
	}

	return myPrmList.getPRMTemplate();
}


void VOP::MetaImageFile::setPluginType()
{
	pluginType = "TEXTURE";

	// Base plugin
	pluginID = "CustomTexBitmap";
}


OP::VRayNode::PluginResult VOP::MetaImageFile::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	// TODO:

	return OP::VRayNode::PluginResultNA;
}
