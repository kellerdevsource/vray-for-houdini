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

#include "vfh_ui.h"


using namespace VRayForHoudini;
using namespace VRayForHoudini::UI;


ActiveItems ActiveStateDeps::activeItems;


void ActiveStateDeps::addStateInfo(const std::string &pluginID, const std::string &affectedProp, const StateInfo &stateInfo)
{
	activeItems[pluginID][affectedProp] = stateInfo;
}


bool ActiveStateDeps::hasStateInfo(const std::string &pluginID, const std::string &affectedProp)
{
	if (NOT(activeItems.count(pluginID)))
		return false;
	if (NOT(activeItems[pluginID].count(affectedProp)))
		return false;
	return true;
}


void ActiveStateDeps::showDependencies(const std::string &pluginID)
{
	for (const auto &pIt : activeItems) {
		if (NOT(pluginID.empty())) {
			if (pIt.first != pluginID) {
				continue;
			}
		}

		PRINT_INFO("Plugin \"%s\" UI:",
				   pIt.first.c_str());

		for (const auto &iIt : pIt.second) {
			const std::string &affectedProp = iIt.first;
			const StateInfo   &stateInfo    = iIt.second;

			PRINT_INFO("  Property \"%s\" affected by \"%s\" (mode: %s)",
					   affectedProp.c_str(),
					   stateInfo.conditionAttr.c_str(),
					   stateInfo.visual == StateInfo::VisualDisable ? "disable" : "hide");
		}
	}
}


void ActiveStateDeps::activateElements(const std::string &pluginID, OP_Node *op_node, bool &changed)
{
	if (activeItems.count(pluginID)) {
#if 0
		ActiveDependencies::showDependencies(pluginID);
#endif
		for (const auto &iIt : activeItems[pluginID]) {
			const std::string &affectedProp = iIt.first;
			const StateInfo   &stateInfo    = iIt.second;

			int activeValue = op_node->evalInt(stateInfo.conditionAttr.c_str(), 0, 0.0f);

			bool state = true;

			if (stateInfo.condition == StateInfo::CondEqual) {
				state = activeValue == stateInfo.conditionValue;
			}
			else if (stateInfo.condition == StateInfo::CondNonEqual) {
				state = activeValue != stateInfo.conditionValue;
			}
			else if (stateInfo.condition == StateInfo::CondGreater) {
				state = activeValue > stateInfo.conditionValue;
			}
			else if (stateInfo.condition == StateInfo::CondGreaterOrEqual) {
				state = activeValue >= stateInfo.conditionValue;
			}
			else if (stateInfo.condition == StateInfo::CondLess) {
				state = activeValue < stateInfo.conditionValue;
			}
			else if (stateInfo.condition == StateInfo::CondLessOrEqual) {
				state = activeValue <= stateInfo.conditionValue;
			}
#if 0
			PRINT_INFO("Property \"%s\" affected by \"%s\" (mode: %s)",
					   affectedProp.c_str(),
					   stateInfo.conditionAttr.c_str(),
					   stateInfo.visual == StateInfo::VisualDisable ? "disable" : "hide");

			PRINT_INFO("  State property value = %i; condition value = %i; state value = %i",
					   activeValue, stateInfo.conditionValue, state);
#endif
			if (stateInfo.visual == StateInfo::VisualDisable) {
				changed |= op_node->enableParm(affectedProp.c_str(), state);
			}
			else if (stateInfo.visual == StateInfo::VisualHide) {
				changed |= op_node->setVisibleState(affectedProp.c_str(), state);
			}
		}
	}
}
