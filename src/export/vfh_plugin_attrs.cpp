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

using namespace VRayForHoudini;
using namespace Attrs;

#define ReturnTrueIfNotEq(member) if (p.member != other->member) { return true; } break;
#define ReturnTrueIfFloatNotEq(member) if (!IsFloatEq(p.member, other->member)) { return true; } break;

template <typename VectorType>
FORCEINLINE bool vectorNotEqual(const VectorType &a, const VectorType &b) {
	if (!IsFloatEq(a[0], b[0]))
		return true;
	if (!IsFloatEq(a[1], b[1]))
		return true;
	if (!IsFloatEq(a[2], b[2]))
		return true;
	return false;
}

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
		case PluginAttr::AttrTypeInt: return "Int";
		case PluginAttr::AttrTypeFloat: return "Float";
		case PluginAttr::AttrTypeVector: return "Vector";
		case PluginAttr::AttrTypeColor: return "Color";
		case PluginAttr::AttrTypeAColor: return "AColor";
		case PluginAttr::AttrTypeTransform: return "Transform";
		case PluginAttr::AttrTypeMatrix: return "Matrix";
		case PluginAttr::AttrTypeString: return "String";
		case PluginAttr::AttrTypePlugin: return "Plugin";
		case PluginAttr::AttrTypePluginDesc: return "PluginDesc";
		case PluginAttr::AttrTypeListInt: return "ListInt";
		case PluginAttr::AttrTypeListFloat: return "ListFloat";
		case PluginAttr::AttrTypeListVector: return "ListVector";
		case PluginAttr::AttrTypeListColor: return "ListColor";
		case PluginAttr::AttrTypeListTransform: return "ListTransform";
		case PluginAttr::AttrTypeListString: return "ListString";
		case PluginAttr::AttrTypeListPlugin: return "ListPlugin";
		case PluginAttr::AttrTypeListValue: return "ListValue";
		case PluginAttr::AttrTypeRawListInt: return "RawListInt";
		case PluginAttr::AttrTypeRawListFloat: return "RawListFloat";
		case PluginAttr::AttrTypeRawListVector: return "RawListVector";
		case PluginAttr::AttrTypeRawListColor: return "RawListColor";
		case PluginAttr::AttrTypeRawListCharString: return "RawListCharString";
		case PluginAttr::AttrTypeRawListValue: return "RawListValue";
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
	pluginAttrs.insert(attr.paramName.c_str(), attr);
}

void Attrs::PluginDesc::add(const PluginAttr &attr)
{
	addAttribute(attr);
}

void Attrs::PluginDesc::remove(const char *name)
{
	pluginAttrs.erase(name);
}

bool Attrs::PluginDesc::isDifferent(const VRayForHoudini::Attrs::PluginDesc &otherDesc) const
{
	FOR_CONST_IT (PluginAttrs, pIt, pluginAttrs) {
		const PluginAttr &p = pIt.data();
		const PluginAttr *other = otherDesc.get(pIt.key());
		if (other) {
			if (p.paramType != other->paramType) {
				return true;
			}
			switch (p.paramType) {
				case PluginAttr::AttrTypeUnknown:
				case PluginAttr::AttrTypeIgnore:
					break;
				case PluginAttr::AttrTypeInt: {
					ReturnTrueIfNotEq(paramValue.valInt);
				}
				case PluginAttr::AttrTypeFloat: {
					ReturnTrueIfFloatNotEq(paramValue.valFloat);
				}
				case PluginAttr::AttrTypeColor:
				case PluginAttr::AttrTypeVector:
				case PluginAttr::AttrTypeAColor: {
					if (vectorNotEqual(p.paramValue.valVector, other->paramValue.valVector))
						return true;
					break;
				}
				case PluginAttr::AttrTypeTransform: {
					if (vectorNotEqual(p.paramValue.valTransform.matrix.v0, other->paramValue.valTransform.matrix.v0))
						return true;
					if (vectorNotEqual(p.paramValue.valTransform.matrix.v1, other->paramValue.valTransform.matrix.v1))
						return true;
					if (vectorNotEqual(p.paramValue.valTransform.matrix.v2, other->paramValue.valTransform.matrix.v2))
						return true;
					if (vectorNotEqual(p.paramValue.valTransform.offset, other->paramValue.valTransform.offset))
						return true;
					break;
				}
				case PluginAttr::AttrTypeString: {
					if (vutils_strcmp(p.paramValue.valString.c_str(), other->paramValue.valString.c_str()) != 0)
						return true;
					break;
				}
				case PluginAttr::AttrTypePlugin: {
					if (vutils_strcmp(p.paramValue.valPlugin.getName(), other->paramValue.valPlugin.getName()) != 0)
						return true;
					break;
				}
				default:
					break;
			}
		}
	}

	return false;
}

bool Attrs::PluginDesc::isEqual(const PluginDesc &otherDesc) const
{
	return !isDifferent(otherDesc);
}
