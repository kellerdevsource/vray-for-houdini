//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_plugin_attrs.h"
#include "vfh_vray.h"

using namespace VRayForHoudini;
using namespace Attrs;

Attrs::PluginDesc::PluginDesc()
{}

Attrs::PluginDesc::PluginDesc(const std::string &pluginName, const std::string &pluginID)
	: pluginName(pluginName)
	, pluginID(pluginID)
{}

PluginAttr::PluginAttrValue::PluginAttrValue()
	: valInt(0)
	, valFloat(0)
	, valPluginDesc(nullptr)
{}

PluginAttr::PluginAttr(const std::string &attrName, const Attrs::PluginDesc *attrValue)
	: paramName(attrName)
	, paramType(AttrTypePluginDesc)
{
	paramValue.valPluginDesc = attrValue;
}

const char *PluginAttr::typeStr() const
{
	switch (paramType) {
		case AttrTypeInt: return "Int";
		case AttrTypeFloat: return "Float";
		case AttrTypeVector: return "Vector";
		case AttrTypeColor: return "Color";
		case AttrTypeAColor: return "AColor";
		case AttrTypeTransform: return "Transform";
		case AttrTypeMatrix: return "Matrix";
		case AttrTypeString: return "String";
		case AttrTypePlugin: return "Plugin";
		case AttrTypePluginDesc: return "PluginDesc";
		case AttrTypeListInt: return "ListInt";
		case AttrTypeListFloat: return "ListFloat";
		case AttrTypeListVector: return "ListVector";
		case AttrTypeListColor: return "ListColor";
		case AttrTypeListTransform: return "ListTransform";
		case AttrTypeListString: return "ListString";
		case AttrTypeListPlugin: return "ListPlugin";
		case AttrTypeListValue: return "ListValue";
		case AttrTypeRawListInt: return "RawListInt";
		case AttrTypeRawListFloat: return "RawListFloat";
		case AttrTypeRawListVector: return "RawListVector";
		case AttrTypeRawListColor: return "RawListColor";
		case AttrTypeRawListCharString: return "RawListCharString";
		case AttrTypeRawListValue: return "RawListValue";
		default:
			break;
	}
	return "AttrTypeUnknown";
}

bool Attrs::PluginDesc::contains(const std::string &paramName) const
{
	return pluginAttrs.find(paramName.c_str()) != pluginAttrs.end();
}

const PluginAttr *Attrs::PluginDesc::get(const std::string &paramName) const
{
	PluginAttrs::const_iterator it = pluginAttrs.find(paramName.c_str());
	if (it != pluginAttrs.end()) {
		return &it.data();
	}
	return nullptr;
}

PluginAttr *Attrs::PluginDesc::get(const std::string &paramName)
{
	PluginAttrs::iterator it = pluginAttrs.find(paramName.c_str());
	if (it != pluginAttrs.end()) {
		return &it.data();
	}
	return nullptr;
}

void Attrs::PluginDesc::addAttribute(const PluginAttr &attr)
{
	pluginAttrs[attr.paramName.c_str()] = attr;
}

void Attrs::PluginDesc::add(const PluginAttr &attr)
{
	addAttribute(attr);
}

void Attrs::PluginDesc::remove(const char *name)
{
	pluginAttrs.erase(name);
}
