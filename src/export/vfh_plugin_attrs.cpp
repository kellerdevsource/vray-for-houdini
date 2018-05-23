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

VfhAttrValue::VfhAttrValue(int value)
	: valInt(value)
{}

VfhAttrValue::VfhAttrValue(float value)
	: valVector{value, 0.0f, 0.0f, 0.0f}
{}

VfhAttrValue::VfhAttrValue(fpreal value)
	: valVector{value, 0.0f, 0.0f, 0.0f}
{}

VfhAttrValue::VfhAttrValue(float a, float b, float c, float d)
	: valVector{a, b, c, d}
{}

VfhAttrValue::VfhAttrValue(const QString &value)
	: valString(value)
{}

VfhAttrValue::VfhAttrValue(const VRay::PluginRef &value)
	: valPluginRef(value)
{}

VfhAttrValue::VfhAttrValue(const VRay::Plugin &value, const QString &output)
	: valPluginRef(value, qPrintable(output))
{}

VfhAttrValue::VfhAttrValue(const VRay::Vector &value)
	: valVector{value.x, value.y, value.z, 0.0f}
{}

VfhAttrValue::VfhAttrValue(const VRay::Transform &value)
	: valTransform(value)
{}

VfhAttrValue::VfhAttrValue(const VRay::Matrix &value)
	: valTransform(value, VRay::Vector(0.0f, 0.0f, 0.0f))
{}

VfhAttrValue::VfhAttrValue(const VRay::VUtils::IntRefList &value)
	: valRawListInt(value)
{}

VfhAttrValue::VfhAttrValue(const VRay::VUtils::FloatRefList &value)
	: valRawListFloat(value)
{}

VfhAttrValue::VfhAttrValue(const VRay::VUtils::VectorRefList &value)
	: valRawListVector(value)
{}

VfhAttrValue::VfhAttrValue(const VRay::VUtils::ColorRefList &value)
	: valRawListColor(value)
{}

VfhAttrValue::VfhAttrValue(const VRay::VUtils::CharStringRefList &value)
	: valRawListCharString(value)
{}

VfhAttrValue::VfhAttrValue(const VRay::VUtils::ValueRefList &value, int isAnimatedGenericList)
	: valRawListValue(value)
	, isAnimatedGenericList(isAnimatedGenericList)
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

PluginAttr::PluginAttr(const QString &attrName, const QString &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeString)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, const char *attrValue)
	: paramName(attrName)
	, paramType(AttrTypeString)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::Matrix &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeMatrix)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::Transform &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeTransform)
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

PluginAttr::PluginAttr(const QString &attrName, float r, float g, float b)
	: paramName(attrName)
	, paramType(AttrTypeColor)
	, paramValue(r, g, b, 1.0)
{}

PluginAttr::PluginAttr(const QString &attrName, float r, float g, float b, float a)
	: paramName(attrName)
	, paramType(AttrTypeAColor)
	, paramValue(r, g, b, a)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::Vector &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeVector)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, const QIntList &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeRawListInt)
	, paramValue(toRefList(attrValue))
{}

PluginAttr::PluginAttr(const QString &attrName, const QFloatList &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeRawListFloat)
	, paramValue(toRefList(attrValue))
{}

PluginAttr::PluginAttr(const QString &attrName, const QColorList &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeRawListColor)
	, paramValue(toRefList(attrValue))
{}

PluginAttr::PluginAttr(const QString &attrName, const QValueList &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeRawListValue)
	, paramValue(toRefList(attrValue))
{}

PluginAttr::PluginAttr(const QString &attrName, int attrValue)
	: paramName(attrName)
	, paramType(AttrTypeInt)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, exint attrValue)
	: paramName(attrName)
	, paramType(AttrTypeInt)
	, paramValue(static_cast<int>(attrValue))
{}

PluginAttr::PluginAttr(const QString &attrName, bool attrValue)
	: paramName(attrName)
	, paramType(AttrTypeInt)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, float attrValue)
	: paramName(attrName)
	, paramType(AttrTypeFloat)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, fpreal attrValue)
	: paramName(attrName)
	, paramType(AttrTypeFloat)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::VUtils::IntRefList &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeRawListInt)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::VUtils::FloatRefList &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeRawListFloat)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::VUtils::VectorRefList &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeRawListVector)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::VUtils::ColorRefList &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeRawListColor)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::VUtils::CharStringRefList &attrValue)
	: paramName(attrName)
	, paramType(AttrTypeRawListCharString)
	, paramValue(attrValue)
{}

PluginAttr::PluginAttr(const QString &attrName, const VRay::VUtils::ValueRefList &value, int isAnimatedGenericList)
	: paramName(attrName)
	, paramType(AttrTypeRawListValue)
	, paramValue(value, isAnimatedGenericList)
{}

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

void Attrs::PluginDesc::remove(const char *name)
{
	pluginAttrs.remove(name);
}

void Attrs::PluginDesc::add(const PluginAttr &attr)
{
	pluginAttrs[attr.paramName] = attr;
}

void Attrs::PluginDesc::add(const QString &attrName, bool attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const char *attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const QString &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::Matrix &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::PluginRef &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::Plugin &attrValue, const QString &output)
{
	add(PluginAttr(attrName, attrValue, output));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::Transform &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::Vector &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const QIntList &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const QFloatList &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const QColorList &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const QValueList &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::VUtils::CharStringRefList &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::VUtils::ColorRefList &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::VUtils::FloatRefList &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::VUtils::IntRefList &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::VUtils::VectorRefList &attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, exint attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, float r, float g, float b)
{
	add(PluginAttr(attrName, r, g, b));
}

void Attrs::PluginDesc::add(const QString &attrName, float r, float g, float b, float a)
{
	add(PluginAttr(attrName, r, g, b, a));
}

void Attrs::PluginDesc::add(const QString &attrName, float attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, fpreal attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, int attrValue)
{
	add(PluginAttr(attrName, attrValue));
}

void Attrs::PluginDesc::add(const QString &attrName, const VRay::VUtils::ValueRefList &attrValue, int isAnimatedGenericList)
{
	add(PluginAttr(attrName, attrValue, isAnimatedGenericList));
}
