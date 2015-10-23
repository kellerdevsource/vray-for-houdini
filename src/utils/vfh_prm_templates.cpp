//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_prm_templates.h"

#include <OP/OP_Node.h>


int VRayForHoudini::Parm::isParmExist(const OP_Node &node, const std::string &attrName)
{
	int parmExist = false;

	const PRM_ParmList *parmList = node.getParmList();
	if (parmList) {
		const PRM_Parm *param = parmList->getParmPtr(attrName.c_str());
		if (param) {
			parmExist = true;
		}
	}

	return parmExist;
}


int VRayForHoudini::Parm::isParmSwitcher(const OP_Node &node, const int index)
{
	int isSwitcher = false;

	const PRM_ParmList *parmList = node.getParmList();
	if (parmList) {
		const PRM_Parm *param = parmList->getParmPtr(index);
		if (param) {
			isSwitcher = param->getType().isSwitcher();
		}
	}

	return isSwitcher;
}


const PRM_Parm* VRayForHoudini::Parm::getParm(const OP_Node &node, const int index)
{
	const PRM_Parm *param = nullptr;

	const PRM_ParmList *parmList = node.getParmList();
	if (parmList) {
		param = parmList->getParmPtr(index);
	}

	return param;
}


const PRM_Parm *VRayForHoudini::Parm::getParm(const OP_Node &node, const std::string &attrName)
{
	const PRM_Parm *param = nullptr;

	const PRM_ParmList *parmList = node.getParmList();
	if (parmList) {
		param = parmList->getParmPtr(attrName.c_str());
	}

	return param;
}


int VRayForHoudini::Parm::getParmInt(const OP_Node &node, const std::string &attrName, fpreal t)
{
	int value = 0;

	if (isParmExist(node, attrName)) {
		value = node.evalInt(attrName.c_str(), 0, t);
	}

	return value;
}


float VRayForHoudini::Parm::getParmFloat(const OP_Node &node, const std::string &attrName, fpreal t)
{
	float value = 0.0f;

	if (isParmExist(node, attrName)) {
		value = node.evalFloat(attrName.c_str(), 0, t);
	}

	return value;
}


int VRayForHoudini::Parm::getParmEnumExt(const OP_Node &node, const VRayForHoudini::Parm::AttrDesc &attrDesc, const std::string &attrName, fpreal t)
{
	int value = node.evalInt(attrName.c_str(), 0, t);

	if (value < attrDesc.value.defEnumItems.size()) {
		const Parm::EnumItem &enumItem = attrDesc.value.defEnumItems[value];
		if (enumItem.valueType == Parm::EnumItem::EnumValueInt) {
			value = enumItem.value;
		}
	}

	return value;
}
