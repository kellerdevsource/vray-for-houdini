//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_ui.h"
#include "vfh_prm_def.h"
#include "vfh_prm_templates.h"


using namespace VRayForHoudini;
using namespace VRayForHoudini::UI;


ActiveItems ActiveStateDeps::activeItems;


void ActiveStateDeps::addStateInfo(const std::string &pluginID, const std::string &affectedProp, const StateInfo &stateInfo)
{
	activeItems[pluginID][affectedProp].push_back(stateInfo);
}


bool ActiveStateDeps::hasStateInfo(const std::string &pluginID, const std::string &affectedProp)
{
	return activeItems.count(pluginID) && activeItems[pluginID].count(affectedProp);
}


void ActiveStateDeps::showDependencies(const std::string &pluginID)
{
	if (activeItems.count(pluginID)) {
		for (const auto &iIt : activeItems[pluginID]) {
			const std::string &affectedProp = iIt.first;

			for (const auto &stateInfo : iIt.second) {
				const std::string affectedBy = stateInfo.conditionPlugin.empty()
											   ? stateInfo.conditionAttr.c_str()
											   : boost::str(Parm::FmtPrefixAuto
															% stateInfo.conditionPlugin
															% stateInfo.conditionAttr);

				PRINT_INFO("  Property \"%s\" affected by \"%s\" (mode: %s)",
						   affectedProp.c_str(),
						   affectedBy.c_str(),
						   stateInfo.visual == StateInfo::VisualDisable ? "disable" : "hide");
			}
		}
	}
}


void ActiveStateDeps::activateElements(const std::string &pluginID, OP_Node &opNode, bool &changed, const std::string &prefix)
{
	if (activeItems.count(pluginID)) {
#if 0
		ActiveStateDeps::showDependencies(pluginID);
#endif
		for (const auto &iIt : activeItems[pluginID]) {
			const std::string &affectedProp = iIt.first;
			const std::string &affectedName = prefix.empty()
											  ? affectedProp
											  : boost::str(Parm::FmtPrefixManual % prefix % affectedProp);

			StateInfo::StateVisual propVisual = StateInfo::VisualDisable;
			bool propState = true;

			for (const auto &stateInfo : iIt.second) {
				const std::string &attrPrefix = stateInfo.conditionPlugin.empty()
												? prefix
												: boost::str(Parm::FmtPrefix % stateInfo.conditionPlugin);

				const std::string &prmName = attrPrefix.empty()
											 ? stateInfo.conditionAttr
											 : boost::str(Parm::FmtPrefixManual % attrPrefix % stateInfo.conditionAttr);

				const PRM_Parm *parm = Parm::getParm(opNode, prmName);
				if (parm) {
					int activeValue = 0;
					if (parm->getType() == PRM_ORD) {
						// Enum
						Parm::VRayPluginInfo *pluginInfo = Parm::GetVRayPluginInfo(pluginID);
						if (pluginInfo && pluginInfo->attributes.count(stateInfo.conditionAttr)) {
							const Parm::AttrDesc &attrDesc = pluginInfo->attributes[stateInfo.conditionAttr];
							activeValue = Parm::getParmEnumExt(opNode, attrDesc, prmName, 0.0);
						}
					}
					else {
						// Assume int, bool
						activeValue = Parm::getParmInt(opNode, prmName, 0.0);
					}

					bool state = true;
					switch (stateInfo.condition) {
						case StateInfo::CondEqual: {
							state = activeValue == stateInfo.conditionValue;
							break;
						}
						case StateInfo::CondNonEqual: {
							state = activeValue != stateInfo.conditionValue;
							break;
						}
						case StateInfo::CondGreater: {
							state = activeValue > stateInfo.conditionValue;
							break;
						}
						case StateInfo::CondGreaterOrEqual: {
							state = activeValue >= stateInfo.conditionValue;
							break;
						}
						case StateInfo::CondLess: {
							state = activeValue < stateInfo.conditionValue;
							break;
						}
						case StateInfo::CondLessOrEqual: {
							state = activeValue <= stateInfo.conditionValue;
							break;
						}
						case StateInfo::CondIn:
						case StateInfo::CondNotIn: {
							int inValues = false;
							for (const auto &val : stateInfo.conditionValues) {
								if (activeValue == val) {
									inValues = true;
									break;
								}
							}
							state = stateInfo.condition == StateInfo::CondIn
									? inValues
									: !inValues;
							break;
						}
					}

					propVisual  = stateInfo.visual;
					propState  &= state;
#if 0
					PRINT_INFO("Property \"%s\" affected by \"%s\" (mode: %s)",
							   affectedProp.c_str(),
							   stateInfo.conditionAttr.c_str(),
							   stateInfo.visual == StateInfo::VisualDisable ? "disable" : "hide");

					PRINT_INFO("  State property value = %i; condition value = %i; state value = %i",
							   activeValue, stateInfo.conditionValue, state);
#endif
				}
			}

			switch (propVisual) {
				case StateInfo::VisualDisable: {
					changed |= opNode.enableParm(affectedName.c_str(), propState);
					break;
				}
				case StateInfo::VisualHide: {
					changed |= opNode.setVisibleState(affectedName.c_str(), propState);
					break;
				}
			}
		}
	}
}
