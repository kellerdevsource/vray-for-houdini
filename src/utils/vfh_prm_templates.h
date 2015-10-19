//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_PRM_TEMPLATES_H
#define VRAY_FOR_HOUDINI_PRM_TEMPLATES_H

#include "vfh_prm_defaults.h"


struct AttributesTab {
	AttributesTab(const std::string &label, const std::string &pluginID, PRM_Template *items):
		label(label),
		pluginID(pluginID),
		items(items)
	{}
	std::string   label;
	std::string   pluginID;
	PRM_Template *items;
};

typedef std::vector<AttributesTab> AttributesTabs;

#endif
