//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_plugin_attrs.h"
#include "vfh_vray.h"

#include <iostream>


using namespace VRayForHoudini;
using namespace VRayForHoudini::Attrs;


#define ReturnTrueIfNotEq(member) if (p.member != other->member) { return true; } break;
#define ReturnTrueIfFloatNotEq(member) if (!IsFloatEq(p.member, other->member)) { return true; } break;


void VRayForHoudini::Attrs::PluginDesc::showAttributes() const
{
	printf("Plugin \"%s_%s\" parameters:\n",
		   pluginID.c_str(), pluginName.c_str());

	for (const auto &pIt : pluginAttrs) {
		const PluginAttr &p = pIt;

		std::cout << p.paramName.c_str() << " [" << p.typeStr() << "] = ";

		switch (p.paramType) {
			case PluginAttr::AttrTypeInt:       std::cout << p.paramValue.valInt; break;
			case PluginAttr::AttrTypeFloat:     std::cout << p.paramValue.valFloat; break;
			case PluginAttr::AttrTypeVector:    std::cout << p.paramValue.valVector; break;
			case PluginAttr::AttrTypeColor:     std::cout << p.paramValue.valVector; break;
			case PluginAttr::AttrTypeAColor:    std::cout << p.paramValue.valVector; break;
			case PluginAttr::AttrTypeTransform: std::cout << p.paramValue.valTransform; break;
			case PluginAttr::AttrTypeMatrix:    std::cout << p.paramValue.valTransform.matrix; break;
			case PluginAttr::AttrTypeString:    std::cout << p.paramValue.valString; break;
			case PluginAttr::AttrTypePlugin:    std::cout << p.paramValue.valPlugin.getName(); break;
		}

		std::cout << std::endl;
	}
}


bool Attrs::PluginDesc::contains(const std::string &paramName) const
{
	if (get(paramName)) {
		return true;
	}
	return false;
}


const PluginAttr *Attrs::PluginDesc::get(const std::string &paramName) const
{
	for (const auto &pIt : pluginAttrs) {
		const PluginAttr &p = pIt;
		if (paramName == p.paramName) {
			return &p;
		}
	}
	return nullptr;
}


PluginAttr *Attrs::PluginDesc::get(const std::string &paramName)
{
	for (auto &pIt : pluginAttrs) {
		PluginAttr &p = pIt;
		if (paramName == p.paramName) {
			return &p;
		}
	}
	return nullptr;
}


void Attrs::PluginDesc::addAttribute(const PluginAttr &attr)
{
	PluginAttr *_attr = get(attr.paramName);
	if (_attr) {
		*_attr = attr;
	}
	else {
		pluginAttrs.push_back(attr);
	}
}


void Attrs::PluginDesc::add(const PluginAttr &attr)
{
	addAttribute(attr);
}


bool VRayForHoudini::Attrs::PluginDesc::isDifferent(const VRayForHoudini::Attrs::PluginDesc &otherDesc) const
{
	for (const auto &p : pluginAttrs) {
		const std::string &attrName = p.paramName;

		const PluginAttr *other = otherDesc.get(attrName);
		if (other) {
			if (p.paramType != other->paramType) {
				return true;
			}
			switch (p.paramType) {
				case PluginAttr::AttrTypeUnknown:
				case PluginAttr::AttrTypeIgnore:
					break;
				case PluginAttr::AttrTypeInt:       ReturnTrueIfNotEq(paramValue.valInt);
				case PluginAttr::AttrTypeFloat:     ReturnTrueIfFloatNotEq(paramValue.valFloat);
//				case PluginAttr::AttrTypeVector:    ReturnTrueIfNotEq(paramValue.valVector);
//				case PluginAttr::AttrTypeColor:     ReturnTrueIfNotEq(paramValue.valVector);
//				case PluginAttr::AttrTypeAColor:    ReturnTrueIfNotEq(paramValue.valVector);
//				case PluginAttr::AttrTypeTransform: ReturnTrueIfNotEq(paramValue.valTransform);
//				case PluginAttr::AttrTypeString:    ReturnTrueIfNotEq(paramValue.valString);
//				case PluginAttr::AttrTypePlugin:    ReturnTrueIfNotEq(paramValue.valPlugin);
			}
		}
	}

	return false;
}


bool Attrs::PluginDesc::isEqual(const PluginDesc &otherDesc) const
{
	return !isDifferent(otherDesc);
}
