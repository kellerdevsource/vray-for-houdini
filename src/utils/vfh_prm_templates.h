//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
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
