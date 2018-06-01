//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_plugin_attrs.h"

using namespace VRayForHoudini;
using namespace Attrs;

VfhAttrValue::VfhAttrValue()
{}

VfhAttrValue::VfhAttrValue(int value, int isAnimated)
	: valInt(value)
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(float value, int isAnimated)
	: valVector{value, 0.0f, 0.0f, 0.0f}
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(fpreal value, int isAnimated)
	: valVector{value, 0.0f, 0.0f, 0.0f}
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(float a, float b, float c, float d, int isAnimated)
	: valVector{a, b, c, d}
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(const QString &value, int isAnimated)
	: valString(value)
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(const VRay::PluginRef &value, int isAnimated)
	: valPluginRef(value)
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(const VRay::Plugin &value, const QString &output, int isAnimated)
	: valPluginRef(value, qPrintable(output))
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(const VRay::Vector &value, int isAnimated)
	: valVector{value.x, value.y, value.z, 0.0f}
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(const VRay::Transform &value, int isAnimated)
	: valTransform(value)
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(const VRay::Matrix &value, int isAnimated)
	: valTransform(value, VRay::Vector(0.0f, 0.0f, 0.0f))
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(const VRay::VUtils::IntRefList &value, int isAnimated)
	: valRawListInt(value)
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(const VRay::VUtils::FloatRefList &value, int isAnimated)
	: valRawListFloat(value)
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(const VRay::VUtils::VectorRefList &value, int isAnimated)
	: valRawListVector(value)
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(const VRay::VUtils::ColorRefList &value, int isAnimated)
	: valRawListColor(value)
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(const VRay::VUtils::CharStringRefList &value, int isAnimated)
	: valRawListCharString(value)
	, isAnimated(isAnimated)
{}

VfhAttrValue::VfhAttrValue(const VRay::VUtils::ValueRefList &value, int isAnimated)
	: valRawListValue(value)
	, isAnimated(isAnimated)
{}

PluginAttr::PluginAttr()
	: paramType(AttrTypeUnknown)
{}

PluginAttr::PluginAttr(const QString &attrName)
	: paramName(attrName)
	, paramType(AttrTypeUnknown)
{}

PluginAttr::PluginAttr(const QString &attrName, VfhAttrType attrType)
	: paramName(attrName)
	, paramType(attrType)
{}

PluginAttr::PluginAttr(const QString &attrName, const QString &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeString)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const char *attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeString)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::Matrix &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeMatrix)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::Transform &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeTransform)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::PluginRef &attrValue)
	: paramName(attrName)
	, paramType(AttrTypePlugin)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::Plugin &attrValue)
	: paramName(attrName)
	, paramType(AttrTypePlugin)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::Plugin &attrValue, const QString &output)
	: paramName(attrName)
	, paramType(AttrTypePlugin)
	, paramValue(attrValue, output)
{}

PluginAttr::PluginAttr(const QString &attrName, float r, float g, float b, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeColor)
	, paramValue(r, g, b, 1.0, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, float r, float g, float b, float a, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeAColor)
	, paramValue(r, g, b, a, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::Vector &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeVector)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const QIntList &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeRawListInt)
	, paramValue(toRefList(attrValue), isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const QFloatList &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeRawListFloat)
	, paramValue(toRefList(attrValue), isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const QColorList &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeRawListColor)
	, paramValue(toRefList(attrValue), isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const QCharStringList &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeRawListColor)
	, paramValue(toRefList(attrValue), isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const QValueList &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeRawListValue)
	, paramValue(toRefList(attrValue), isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, int attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeInt)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, exint attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeInt)
	, paramValue(static_cast<int>(attrValue), isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, bool attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeInt)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, float attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeFloat)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, fpreal attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeFloat)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::VUtils::IntRefList &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeRawListInt)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::VUtils::FloatRefList &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeRawListFloat)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::VUtils::VectorRefList &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeRawListVector)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::VUtils::ColorRefList &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeRawListColor)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::VUtils::CharStringRefList &attrValue, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeRawListCharString)
	, paramValue(attrValue, isAnimated)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::VUtils::ValueRefList &value, int isAnimated)
	: paramName(attrName)
	, paramType(AttrTypeRawListValue)
	, paramValue(value, isAnimated)
{}

void PluginAttr::setAnimated(int value)
{
	paramValue.isAnimated = value;
}

QString PluginAttr::getTypeAsString() const
{
	switch (paramType) {
		case AttrTypeUnknown: return SL("AttrTypeUnknown");
		case AttrTypeIgnore: return SL("AttrTypeIgnore");
		case AttrTypeInt: return SL("Int");
		case AttrTypeFloat: return SL("Float");
		case AttrTypeVector: return SL("Vector");
		case AttrTypeColor: return SL("Color");
		case AttrTypeAColor: return SL("AColor");
		case AttrTypeTransform: return SL("Transform");
		case AttrTypeMatrix: return SL("Matrix");
		case AttrTypeString: return SL("String");
		case AttrTypePlugin: return SL("Plugin");
		case AttrTypePluginDesc: return SL("PluginDesc");
		case AttrTypeRawListInt: return SL("RawListInt");
		case AttrTypeRawListFloat: return SL("RawListFloat");
		case AttrTypeRawListVector: return SL("RawListVector");
		case AttrTypeRawListColor: return SL("RawListColor");
		case AttrTypeRawListCharString: return SL("RawListCharString");
		case AttrTypeRawListValue: return SL("RawListValue");
		default: return SL("AttrTypeUnknown");
	}
}

Attrs::PluginDesc::PluginDesc(const QString &pluginName, const QString &pluginID)
	: pluginName(pluginName)
	, pluginID(pluginID)
{}

bool Attrs::PluginDesc::contains(const QString &paramName) const
{
	return pluginAttrs.find(paramName) != pluginAttrs.end();
}

const PluginAttr* Attrs::PluginDesc::get(const QString &paramName) const
{
	PluginAttrs::const_iterator it = pluginAttrs.find(paramName);
	if (it != pluginAttrs.end())
		return &it.value();
	return nullptr;
}

PluginAttr* Attrs::PluginDesc::get(const QString &paramName)
{
	PluginAttrs::iterator it = pluginAttrs.find(paramName);
	if (it != pluginAttrs.end())
		return &it.value();
	return nullptr;
}

void Attrs::PluginDesc::remove(const QString &attrName)
{
	pluginAttrs.remove(attrName);
}

void Attrs::PluginDesc::setIngore(const QString &attrName)
{
	add(PluginAttr(attrName, AttrTypeIgnore));
}

void Attrs::PluginDesc::add(const PluginAttr &attr)
{
	pluginAttrs[attr.paramName] = attr;
}

void Attrs::PluginDesc::add(const QString &attrName, bool attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const char *attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const QString &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::PluginRef &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::Plugin &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::Plugin &attrValue, const QString &output)
{
	add(PluginAttr(attrName, attrValue, output));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::Matrix &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::Transform &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::Vector &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const QIntList &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const QFloatList &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const QColorList &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const QCharStringList &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const QValueList &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::VUtils::CharStringRefList &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::VUtils::ColorRefList &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::VUtils::FloatRefList &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::VUtils::IntRefList &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::VUtils::VectorRefList &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, exint attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, float r, float g, float b, int isAnimated)
{
	add(PluginAttr(attrName, r, g, b, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, float r, float g, float b, float a, int isAnimated)
{
	add(PluginAttr(attrName, r, g, b, a, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, float attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, fpreal attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, int attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::VUtils::ValueRefList &attrValue, int isAnimated)
{
	add(PluginAttr(attrName, attrValue, isAnimated));
}
