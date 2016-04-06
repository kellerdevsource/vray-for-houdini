//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_prm_json.h"
#include "vfh_prm_def.h"


using namespace VRayForHoudini;
using namespace VRayForHoudini::Parm;


ParmDefValue::PRM_DefaultPtrList ParmDefValue::PrmDefPtrList;
ParmDefValue::PRM_DefaultPtrList ParmDefValue::PrmDefArrPtrList;


boost::format Parm::FmtPrefix("%s_");
boost::format Parm::FmtPrefixAuto("%s_%s");
boost::format Parm::FmtPrefixManual("%s%s");


const char *ParmDefValue::typeStr() const
{
	switch (type) {
		case eInt:
			return "Int";
		case eFloat:
			return "Float";
		case eEnum:
			return "Enum";
		case eBool:
			return "Bool";
		case eColor:
			return "Color";
		case eAColor:
			return "AColor";
		case eString:
			return "String";
		case eTextureColor:
			return "TextureColor";
		case eTextureFloat:
			return "TextureFloat";
		case eTextureInt:
			return "TextureInt";
		case eCurve:
			return "Curve";
		case eRamp:
			return "Ramp";
		case ePlugin:
			return "Plugin";
		case eOutputPlugin:
			return "OutputPlugin";
		case eOutputColor:
			return "OutputColor";
		case eOutputTextureColor:
			return "OutputTextureColor";
		case eOutputTextureFloat:
			return "OutputTextureFloat";
		case eOutputTextureInt:
			return "OutputTextureInt";
		case eOutputTextureVector:
			return "OutputTextureVector";
		case eOutputTextureMatrix:
			return "OutputTextureMatrix";
		case eOutputTextureTransform:
			return "OutputTextureTransform";
		default:
			break;
	}
	return "Unknown";
}


void Parm::addTabItems(const Parm::TabItemDesc &tabItemDesc, PRMDefList &switcher, PRMTmplList &prmTemplate)
{
	Parm::PRMTmplList *prmTmplList = Parm::generatePrmTemplate(tabItemDesc.pluginID,
															   tabItemDesc.pluginID /* prefix for the attr name */);
	if (prmTmplList) {
		// Without list terminator
		const int prmTmplCount = prmTmplList->size() - 1;
		if (prmTmplCount > 0) {
			// Add switcher tab
			switcher.push_back(PRM_Default(prmTmplCount, tabItemDesc.label));

			// Add tab items
			for (int i = 0; i < prmTmplCount; ++i) {
				prmTemplate.push_back((*prmTmplList)[i]);
			}
		}
	}
}


void Parm::addTabsItems(Parm::TabItemDesc tabItemsDesc[], int tabItemsDescCount, PRMDefList &switcher, PRMTmplList &prmTemplate)
{
	for (int t = 0; t < tabItemsDescCount; ++t) {
		const Parm::TabItemDesc &tabItemDesc = tabItemsDesc[t];
		addTabItems(tabItemDesc, switcher, prmTemplate);
	}
}


void Parm::addTabWithTabs(const char *label,
						  TabItemDesc tabItemsDesc[], int tabItemsDescCount,
						  PRMDefList &switcher, PRM_Name &switcherName,
						  PRMTmplList &prmTemplate, PRMDefList &mainSwitcher)
{
	const int insertPos = prmTemplate.size();

	Parm::addTabsItems(tabItemsDesc, tabItemsDescCount, switcher, prmTemplate);

	prmTemplate.insert(prmTemplate.begin() + insertPos,
					   PRM_Template(PRM_SWITCHER,
									switcher.size(),
									&switcherName,
									&switcher[0]));

	mainSwitcher.push_back(PRM_Default(1, label));
}
