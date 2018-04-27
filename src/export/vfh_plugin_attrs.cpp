//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_plugin_attrs.h"

using namespace VRayForHoudini;
using namespace Attrs;

Attrs::PluginDesc::PluginDesc(const QString &pluginName, const QString &pluginID)
	: pluginName(pluginName)
	, pluginID(pluginID)
{}

PluginAttr::PluginAttr(const QString &attrName, const Attrs::PluginDesc *attrValue)
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

bool Attrs::PluginDesc::contains(const QString &paramName) const
{
	return pluginAttrs.find(paramName) != pluginAttrs.end();
}

const PluginAttr* Attrs::PluginDesc::get(const QString &paramName) const
{
	PluginAttrs::const_iterator it = pluginAttrs.find(paramName);
	if (it != pluginAttrs.end()) {
		return &it.value();
	}
	return nullptr;
}

PluginAttr* Attrs::PluginDesc::get(const QString &paramName)
{
	PluginAttrs::iterator it = pluginAttrs.find(paramName);
	if (it != pluginAttrs.end()) {
		return &it.value();
	}
	return nullptr;
}

void Attrs::PluginDesc::add(const PluginAttr &attr)
{
	pluginAttrs[attr.paramName] = attr;
}

void Attrs::PluginDesc::remove(const char *name)
{
	pluginAttrs.remove(name);
}
