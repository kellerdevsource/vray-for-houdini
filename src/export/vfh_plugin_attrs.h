//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_PLUGIN_ATTRS_H
#define VRAY_FOR_HOUDINI_PLUGIN_ATTRS_H

#include "vfh_vray.h"

#include <QVector>

namespace VRayForHoudini {
namespace Attrs {

typedef QVector<VRay::VUtils::Value> QValueList;
typedef QVector<float> QFloatList;
typedef QVector<int> QIntList;
typedef QVector<VRay::Color> QColorList;

template <typename ValueType>
VRay::VUtils::PtrArray<ValueType> toRefList(const std::vector<ValueType> &vrayVector)
{
	VRay::VUtils::PtrArray<ValueType> vutilsList;
	if (!vrayVector.size())
		return vutilsList;
	vutilsList = VRay::VUtils::PtrArray<ValueType>(vrayVector.size());
	for (int i = 0; i < vrayVector.size(); ++i) {
		vutilsList[i] = vrayVector[i];
	}
	return vutilsList;
}

/// Attribute value types.
enum VfhAttrType
{
	AttrTypeUnknown = 0,
	AttrTypeIgnore,
	/// To signal we should ignore this parameter.
	AttrTypeInt,
	AttrTypeFloat,
	AttrTypeVector,
	AttrTypeColor,
	AttrTypeAColor,
	AttrTypeTransform,
	AttrTypeMatrix,
	AttrTypeString,
	AttrTypePlugin,
	AttrTypePluginDesc,
	AttrTypeRawListInt,
	AttrTypeRawListFloat,
	AttrTypeRawListVector,
	AttrTypeRawListColor,
	AttrTypeRawListCharString,
	AttrTypeRawListValue,
};

/// Attribute value.
struct VfhAttrValue
{
	VfhAttrValue();
	VfhAttrValue(float a, float b, float c, float d);
	explicit VfhAttrValue(int value);
	explicit VfhAttrValue(float value);
	explicit VfhAttrValue(fpreal value);
	explicit VfhAttrValue(const QString &value);
	explicit VfhAttrValue(const VRay::Plugin &value, const QString &output = SL(""));
	explicit VfhAttrValue(const VRay::Vector &value);
	explicit VfhAttrValue(const VRay::Transform &value);
	explicit VfhAttrValue(const VRay::Matrix &value);
	explicit VfhAttrValue(const VRay::VUtils::IntRefList &value);
	explicit VfhAttrValue(const VRay::VUtils::FloatRefList &value);
	explicit VfhAttrValue(const VRay::VUtils::VectorRefList &value);
	explicit VfhAttrValue(const VRay::VUtils::ColorRefList &value);
	explicit VfhAttrValue(const VRay::VUtils::CharStringRefList &value);
	explicit VfhAttrValue(const VRay::VUtils::ValueRefList &value, int isAnimatedGenericList = false);

	int valInt = 0;
	fpreal valVector[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	QString valString;
	VRay::Plugin valPlugin;
	QString valPluginOutput;
	VRay::Transform valTransform{1};
	VRay::VUtils::IntRefList valRawListInt;
	VRay::VUtils::FloatRefList valRawListFloat;
	VRay::VUtils::VectorRefList valRawListVector;
	VRay::VUtils::ColorRefList valRawListColor;
	VRay::VUtils::CharStringRefList valRawListCharString;
	VRay::VUtils::ValueRefList valRawListValue;

	/// Generic list value (@a valRawListValue) need key-frames.
	int isAnimatedGenericList = false;
};

/// Plugin attribute.
struct PluginAttr
{
	PluginAttr();
	explicit PluginAttr(const QString &attrName);
	PluginAttr(const QString &attrName, VfhAttrType attrType);

	PluginAttr(const QString &attrName, bool attrValue);
	PluginAttr(const QString &attrName, const char *attrValue);
	PluginAttr(const QString &attrName, const QString &attrValue);
	PluginAttr(const QString &attrName, const VRay::Matrix &attrValue);
	PluginAttr(const QString &attrName, const VRay::Plugin &attrValue);
	PluginAttr(const QString &attrName, const VRay::Plugin &attrValue, const QString &output);
	PluginAttr(const QString &attrName, const VRay::Transform &attrValue);
	PluginAttr(const QString &attrName, const VRay::Vector &attrValue);
	PluginAttr(const QString &attrName, const QIntList &attrValue);
	PluginAttr(const QString &attrName, const QFloatList &attrValue);
	PluginAttr(const QString &attrName, const QColorList &attrValue);
	PluginAttr(const QString &attrName, const QValueList &attrValue);
	PluginAttr(const QString &attrName, const VRay::VUtils::CharStringRefList &attrValue);
	PluginAttr(const QString &attrName, const VRay::VUtils::ColorRefList &attrValue);
	PluginAttr(const QString &attrName, const VRay::VUtils::FloatRefList &attrValue);
	PluginAttr(const QString &attrName, const VRay::VUtils::IntRefList &attrValue);
	PluginAttr(const QString &attrName, const VRay::VUtils::VectorRefList &attrValue);
	PluginAttr(const QString &attrName, exint attrValue);
	PluginAttr(const QString &attrName, float attrValue);
	PluginAttr(const QString &attrName, float r, float g, float b);
	PluginAttr(const QString &attrName, float r, float g, float b, float a);
	PluginAttr(const QString &attrName, fpreal attrValue);
	PluginAttr(const QString &attrName, int attrValue);

	/// Override constructor for ValueRefList parameter.
	/// @param attrName Plugin parameter name.
	/// @param value Plugin parameter value as ValueRefList.
	/// @param isAnimatedGenericList ValueRefList should create key-frames.
	PluginAttr(const QString &attrName, const VRay::VUtils::ValueRefList &value, int isAnimatedGenericList = false);

	/// Get attribute type as string
	QString getTypeAsString() const;

	/// Plugin parameter name.
	QString paramName;

	/// Plugin parameter type.
	VfhAttrType paramType;

	/// Plugin parameter value.
	VfhAttrValue paramValue;
};

typedef QMap<QString, PluginAttr> PluginAttrs;

/// Description of a plugin instance and its attributes. It is used to
/// accumulate attribute changes and to allow to batch changes together for
/// a plugin.
struct PluginDesc
{
	explicit PluginDesc(const QString &pluginName = SL(""), const QString &pluginID = SL(""));

	/// Test if we have attribute with a given name
	/// @param paramName attribute name
	bool contains(const QString &paramName) const;

	/// Append an attrubute to our description. If an attribute with the same
	/// name already exists, it will overwrite it.
	/// @param attr attribute
	void add(const PluginAttr &attr);

	void add(const QString &attrName, bool attrValue);
	void add(const QString &attrName, const char *attrValue);
	void add(const QString &attrName, const QString &attrValue);
	void add(const QString &attrName, const VRay::Matrix &attrValue);
	void add(const QString &attrName, const VRay::Plugin &attrValue);
	void add(const QString &attrName, const VRay::Plugin &attrValue, const QString &output);
	void add(const QString &attrName, const VRay::Transform &attrValue);
	void add(const QString &attrName, const VRay::Vector &attrValue);
	void add(const QString &attrName, const QIntList &attrValue);
	void add(const QString &attrName, const QFloatList &attrValue);
	void add(const QString &attrName, const QColorList &attrValue);
	void add(const QString &attrName, const QValueList &attrValue);
	void add(const QString &attrName, const VRay::VUtils::CharStringRefList &attrValue);
	void add(const QString &attrName, const VRay::VUtils::ColorRefList &attrValue);
	void add(const QString &attrName, const VRay::VUtils::FloatRefList &attrValue);
	void add(const QString &attrName, const VRay::VUtils::IntRefList &attrValue);
	void add(const QString &attrName, const VRay::VUtils::ValueRefList &attrValue, int isAnimatedGenericList = false);
	void add(const QString &attrName, const VRay::VUtils::VectorRefList &attrValue);
	void add(const QString &attrName, exint attrValue);
	void add(const QString &attrName, float attrValue);
	void add(const QString &attrName, float r, float g, float b);
	void add(const QString &attrName, float r, float g, float b, float a);
	void add(const QString &attrName, fpreal attrValue);
	void add(const QString &attrName, int attrValue);

	/// Remove attrubute.
	/// @param name Attribute name.
	void remove(const char *name);

	/// Return the attibute with the specified name or nullptr
	/// @param paramName Attribute name
	const PluginAttr *get(const QString &paramName) const;

	/// Return the attibute with the specified name or nullptr
	/// @param paramName Attribute name
	PluginAttr *get(const QString &paramName);

	/// Plugin instance name.
	QString pluginName;

	/// Plugin type name.
	QString pluginID;

	/// A list of plugin attributes.
	PluginAttrs pluginAttrs;
};

} // namespace Attrs
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PLUGIN_ATTRS_H
