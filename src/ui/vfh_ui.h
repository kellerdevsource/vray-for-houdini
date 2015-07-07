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

#ifndef VRAY_FOR_HOUDINI_UI_H
#define VRAY_FOR_HOUDINI_UI_H

#include "vfh_defines.h"

#include <OP/OP_Node.h>


namespace VRayForHoudini {
namespace UI {

struct StateInfo {
	enum StateCondition {
		CondNone = 0,
		CondEqual,
		CondNonEqual,
		CondGreater,
		CondGreaterOrEqual,
		CondLess,
		CondLessOrEqual,
	};

	enum StateVisual {
		VisualDisable = 0,
		VisualHide
	};

	StateInfo():
		condition(StateCondition::CondNone),
		visual(StateVisual::VisualDisable)
	{}

	operator bool() const {
		return condition != StateCondition::CondNone;
	}

	std::string     conditionAttr;
	int             conditionValue;
	StateCondition  condition;
	StateVisual     visual;

};

// <AffectedProp, StateInfo>
typedef std::map<std::string, StateInfo>         ActiveDependency;

// <PluginID, ActiveItems>
typedef std::map<std::string, ActiveDependency>  ActiveItems;


struct ActiveStateDeps
{
	static void         addStateInfo(const std::string &pluginID, const std::string &affectedProp, const StateInfo &stateInfo);
	static bool         hasStateInfo(const std::string &pluginID, const std::string &affectedProp);

	static void         showDependencies(const std::string &pluginID="");

	static void         activateElements(const std::string &pluginID, OP_Node *op_node, bool &changed);

	static ActiveItems  activeItems;

};

} // namespace UI
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_UI_H
