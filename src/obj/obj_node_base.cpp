//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "obj_node_base.h"
#include "vfh_defines.h"
#include "vfh_includes.h"
#include "vfh_class_utils.h"
#include "vfh_prm_json.h"

#include <map>


namespace VRayForHoudini {
namespace OBJ {


static PRM_Name vrayswitcher("vrayswitcher");
static PRM_Name prm_dome_tex("dome_tex_op", "Dome Texture");


const char *getVRayPluginTypeName(VRayPluginType pluginType)
{
	static std::map<VRayPluginType, const char *> pluginTypeNames;
	if (NOT(pluginTypeNames.size())) {
		pluginTypeNames[VRayPluginType::Light] = "LIGHT";
	}

	return pluginTypeNames[pluginType];
}


const char *getVRayPluginIDName(VRayPluginID pluginID)
{
	static std::map<VRayPluginID, const char *> pluginIDNames;
	if (NOT(pluginIDNames.size())) {
		pluginIDNames[VRayPluginID::SunLight] = "SunLight";
		pluginIDNames[VRayPluginID::LightDirect] = "LightDirect";
		pluginIDNames[VRayPluginID::LightAmbient] = "LightAmbient";
		pluginIDNames[VRayPluginID::LightOmni] = "LightOmni";
		pluginIDNames[VRayPluginID::LightSphere] = "LightSphere";
		pluginIDNames[VRayPluginID::LightSpot] = "LightSpot";
		pluginIDNames[VRayPluginID::LightRectangle] = "LightRectangle";
		pluginIDNames[VRayPluginID::LightMesh] = "LightMesh";
		pluginIDNames[VRayPluginID::LightIES] = "LightIES";
		pluginIDNames[VRayPluginID::LightDome] = "LightDome";
	}

	return pluginIDNames[pluginID];
}



template< VRayPluginID PluginID >
PRM_Template* LightNodeBase< PluginID >::GetPrmTemplate()
{
	static Parm::PRMDefList prmFolders;
	static Parm::PRMTmplList prmList;

	if (NOT(prmList.size())) {
		PRM_Template *defPrmList = OBJ_Light::getTemplateList(OBJ_PARMS_PLAIN);
		const int defPrmCnt = PRM_Template::countTemplates(defPrmList);
		prmList.reserve(defPrmCnt + 1);

		if (defPrmCnt > 0) {
			// assume "stdswitcher" is first and "Transform" folder tab is first
			PRM_Default *xformFolder = defPrmList->getFactoryDefaults();
			const fpreal xformPrmCnt = xformFolder->getFloat();
			//put all xform params in "Transform" folder tab
			prmFolders.emplace_back(xformPrmCnt, "Transform");
			//put all other params in "Misc" folder tab
			prmFolders.emplace_back(defPrmCnt - 1 - xformPrmCnt , "Misc");

			for (int i = 0; i < defPrmCnt; ++i) {
				prmList.push_back( *(defPrmList + i) );
			}

			UT_String prmName;
			for (int i = 1+xformPrmCnt; i < prmList.size(); ++i) {
				PRM_Template &prmTmpl = prmList[i];
				prmTmpl.getToken(prmName);
				if (   prmName != "picking"
					&& prmName != "pickscript"
					&& prmName != "caching" )
				{
					prmTmpl.setInvisible(true);
				}
			}
		}
		else {
			prmList.push_back( PRM_Template(PRM_SWITCHER,
											0,
											&vrayswitcher,
											0) );
		}

		GetMyPrmTemplate(prmList, prmFolders);

		// assign switcher folders
		PRM_Template &switcher = prmList[0];
		switcher.assign(switcher, prmFolders.size(), prmFolders.data());

		// add param list terminator
		prmList.push_back(PRM_Template());
	}

	return prmList.data();
}


template< VRayPluginID PluginID >
int LightNodeBase< PluginID >::GetMyPrmTemplate(Parm::PRMTmplList &prmList, Parm::PRMDefList &prmFolders)
{
	Parm::PRMTmplList *plgPrmList = Parm::generatePrmTemplate( getVRayPluginIDName(PluginID) );
	const int plgPrmCnt = plgPrmList->size()-1;

	for (int i = 0; i < plgPrmCnt; ++i){
		prmList.push_back( (*plgPrmList)[i] );
	}

	// put all plugin params in "V-Ray Light Setting" folder tab
	// assume folders from prmFolders contain all param templates in prmList
	// otherwise we need to calculate the index to properly insert our plugin params in the folder
	prmFolders.emplace_back(plgPrmCnt, "V-Ray Light");

	return plgPrmCnt;
}


template< VRayPluginID PluginID >
OP::VRayNode::PluginResult LightNodeBase< PluginID >::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, OP_Node *parent)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);

	return OP::VRayNode::PluginResultContinue;
}


// explicitly instantiate CustomPrmTemplates for LightDome op node
template<>
int LightNodeBase< VRayPluginID::LightDome >::GetMyPrmTemplate(Parm::PRMTmplList &prmList, Parm::PRMDefList &prmFolders)
{
	// filter default params and hide unnecessery ones
	UT_String prmName;
	for (int i = 0; i < prmList.size(); ++i) {
		PRM_Template &prmTmpl = prmList[i];
		int switcherIdx = -1;
		int folderIdx = -1;
		PRM_Template::getEnclosingSwitcherFolder(prmList.data(), i, switcherIdx, folderIdx);
		if (   switcherIdx == 0
			&& folderIdx ==0 )
		{
			prmTmpl.getToken(prmName);
			if (   prmName != "xOrd"
				&& prmName != "rOrd"
				&& prmName != "r"
				&& prmName != "lookatpath"
				&& prmName != "lookup" )
			{
				prmTmpl.setInvisible(true);
			}
		}
	}

	// add custom params
	const int myPrmIdx = prmList.size();
	prmList.push_back( PRM_Template(PRM_STRING_E,
									PRM_TYPE_DYNAMIC_PATH,
									1,
									&prm_dome_tex,
									&Parm::PRMemptyStringDefault) );

	Parm::PRMTmplList *plgPrmList = Parm::generatePrmTemplate( getVRayPluginIDName(VRayPluginID::LightDome) );
	// last element is list terminator
	for (int i = 0; i < plgPrmList->size()-1; ++i){
		prmList.push_back( (*plgPrmList)[i] );
	}

	// put all plugin params in "V-Ray Light Setting" folder tab
	// assume folders from prmFolders contain all param templates in prmList
	// otherwise we need to calculate the index to properly insert our plugin params in the folder
	const int myPrmCnt = prmList.size() - myPrmIdx;
	prmFolders.emplace_back(myPrmCnt, "V-Ray Light");

	return myPrmCnt;
}


template<>
OP::VRayNode::PluginResult LightNodeBase< VRayPluginID::LightDome >::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, OP_Node *parent)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);

	// Need to flip tm
	VRay::Transform tm = VRayExporter::getObjTransform(parent->castToOBJNode(), exporter.getContext(), true);
	pluginDesc.addAttribute(Attrs::PluginAttr("transform", tm));

	// Dome texture
	//
	UT_String dome_tex;
	evalString(dome_tex, prm_dome_tex.getToken(), 0, 0.0f);
	if (NOT(dome_tex.equal(""))) {
		OP_Node *tex_node = OPgetDirector()->findNode(dome_tex.buffer());
		if (NOT(tex_node)) {
			Log::getLog().error("Texture node not found!");
		}
		else {
			VRay::Plugin texture = exporter.exportVop(tex_node);
			if (NOT(texture)) {
				Log::getLog().error("Texture node export failed!");
			}
			else {
				pluginDesc.addAttribute(Attrs::PluginAttr("dome_tex", texture));
			}
		}
	}

	return OP::VRayNode::PluginResultContinue;
}


// explicitly instantiate op node classes for light plugins
template class LightNodeBase< VRayPluginID::SunLight >;
template class LightNodeBase< VRayPluginID::LightDirect >;
template class LightNodeBase< VRayPluginID::LightAmbient >;
template class LightNodeBase< VRayPluginID::LightOmni >;
template class LightNodeBase< VRayPluginID::LightSphere >;
template class LightNodeBase< VRayPluginID::LightSpot >;
template class LightNodeBase< VRayPluginID::LightRectangle >;
template class LightNodeBase< VRayPluginID::LightMesh >;
template class LightNodeBase< VRayPluginID::LightIES >;
template class LightNodeBase< VRayPluginID::LightDome >;


}
}
