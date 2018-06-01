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

template <typename ValueType>
struct QReserveVector
	: QVector<ValueType>
{
	enum class Flags {
		/// Call QVector::resize() for indexed access.
		/// Useful when size is known in advance.
		resize = 0,

		/// Call QVector::reserve() for faster append().
		/// Useful when size is not known in advance.
		reserve,
	};

	QReserveVector() {}

	explicit QReserveVector(int size, Flags flags=Flags::reserve) {
		if (flags == Flags::resize) {
			QVector<ValueType>::resize(size);
		}
		else if (flags == Flags::reserve) {
			QVector<ValueType>::reserve(size);
		}
	}

	/// @param size Size.
	/// @param value Init items with this value.
	explicit QReserveVector(int size, const ValueType &value)
		: QVector<ValueType>(size, value)
	{}
};

typedef QReserveVector<float> QFloatList;
typedef QReserveVector<int> QIntList;
typedef QReserveVector<VRay::Color> QColorList;
typedef QReserveVector<VRay::VUtils::Value> QValueList;
typedef QReserveVector<VRay::VUtils::CharString> QCharStringList;

/// Converts QVector<ValueType> to VUtils::PtrArray<ValueType>.
/// @tparam ValueType Array item value type.
/// @param qList QVector<ValueType> array.
/// @returns VUtils::PtrArray<ValueType>.
template <typename ValueType>
VRay::VUtils::PtrArray<ValueType> toRefList(const QVector<ValueType> &qList)
{
	VRay::VUtils::PtrArray<ValueType> refList;
	if (qList.isEmpty())
		return refList;
	refList = VRay::VUtils::PtrArray<ValueType>(qList.size());
	for (int i = 0; i < qList.size(); ++i) {
		refList[i] = qList[i];
	}
	return refList;
}

/// Converts std::vector<ValueType> to VUtils::PtrArray<ValueType>.
/// @tparam ValueType Array item value type.
/// @param stdList std::vector<ValueType> array.
/// @returns VUtils::PtrArray<ValueType>.
template <typename ValueType>
VRay::VUtils::PtrArray<ValueType> toRefList(const std::vector<ValueType> &stdList)
{
	VRay::VUtils::PtrArray<ValueType> vutilsList;
	if (!stdList.size())
		return vutilsList;
	vutilsList = VRay::VUtils::PtrArray<ValueType>(stdList.size());
	for (int i = 0; i < stdList.size(); ++i) {
		vutilsList[i] = stdList[i];
	}
	return vutilsList;
}

/// Attribute value types.
enum VfhAttrType
{
	AttrTypeIgnore = -1, ///< To signal we should ignore this parameter.
	AttrTypeUnknown = 0,
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
	VfhAttrValue(float a, float b, float c, float d, int isAnimated = false);
	explicit VfhAttrValue(int value, int isAnimated = false);
	explicit VfhAttrValue(float value, int isAnimated = false);
	explicit VfhAttrValue(fpreal value, int isAnimated = false);
	explicit VfhAttrValue(const QString &value, int isAnimated = false);
	explicit VfhAttrValue(const VRay::PluginRef &value, int isAnimated = false);
	explicit VfhAttrValue(const VRay::Plugin &value, const QString &output = SL(""), int isAnimated = false);
	explicit VfhAttrValue(const VRay::Vector &value, int isAnimated = false);
	explicit VfhAttrValue(const VRay::Transform &value, int isAnimated = false);
	explicit VfhAttrValue(const VRay::Matrix &value, int isAnimated = false);
	explicit VfhAttrValue(const VRay::VUtils::IntRefList &value, int isAnimated = false);
	explicit VfhAttrValue(const VRay::VUtils::FloatRefList &value, int isAnimated = false);
	explicit VfhAttrValue(const VRay::VUtils::VectorRefList &value, int isAnimated = false);
	explicit VfhAttrValue(const VRay::VUtils::ColorRefList &value, int isAnimated = false);
	explicit VfhAttrValue(const VRay::VUtils::CharStringRefList &value, int isAnimated = false);
	explicit VfhAttrValue(const VRay::VUtils::ValueRefList &value, int isAnimated = false);

	int valInt = 0;
	fpreal valVector[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	QString valString;
	VRay::PluginRef valPluginRef;
	VRay::Transform valTransform{1};
	VRay::VUtils::IntRefList valRawListInt;
	VRay::VUtils::FloatRefList valRawListFloat;
	VRay::VUtils::VectorRefList valRawListVector;
	VRay::VUtils::ColorRefList valRawListColor;
	VRay::VUtils::CharStringRefList valRawListCharString;
	VRay::VUtils::ValueRefList valRawListValue;

	/// If parameter value is animated.
	int isAnimated = false;
};

/// Plugin attribute.
struct PluginAttr
{
	PluginAttr();
	explicit PluginAttr(const QString &attrName);
	PluginAttr(const QString &attrName, VfhAttrType attrType);

	PluginAttr(const QString &attrName, bool attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const char *attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const QString &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const VRay::Matrix &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const VRay::Transform &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const VRay::Vector &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const QIntList &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const QFloatList &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const QColorList &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const QCharStringList &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const QValueList &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const VRay::VUtils::CharStringRefList &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const VRay::VUtils::ColorRefList &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const VRay::VUtils::FloatRefList &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const VRay::VUtils::IntRefList &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const VRay::VUtils::VectorRefList &attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const VRay::VUtils::ValueRefList &value, int isAnimated = false);
	PluginAttr(const QString &attrName, exint attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, float attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, float r, float g, float b, int isAnimated = false);
	PluginAttr(const QString &attrName, float r, float g, float b, float a, int isAnimated = false);
	PluginAttr(const QString &attrName, fpreal attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, int attrValue, int isAnimated = false);
	PluginAttr(const QString &attrName, const VRay::PluginRef &attrValue);
	PluginAttr(const QString &attrName, const VRay::Plugin &attrValue);
	PluginAttr(const QString &attrName, const VRay::Plugin &attrValue, const QString &output);

	/// Mark parameter value as animated;
	void setAnimated(int value);

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

	void add(const QString &attrName, bool attrValue, int isAnimated = false);
	void add(const QString &attrName, const char *attrValue, int isAnimated = false);
	void add(const QString &attrName, const QString &attrValue, int isAnimated = false);
	void add(const QString &attrName, const VRay::Matrix &attrValue, int isAnimated = false);
	void add(const QString &attrName, const VRay::Transform &attrValue, int isAnimated = false);
	void add(const QString &attrName, const VRay::Vector &attrValue, int isAnimated = false);
	void add(const QString &attrName, const QIntList &attrValue, int isAnimated = false);
	void add(const QString &attrName, const QFloatList &attrValue, int isAnimated = false);
	void add(const QString &attrName, const QColorList &attrValue, int isAnimated = false);
	void add(const QString &attrName, const QCharStringList &attrValue, int isAnimated = false);
	void add(const QString &attrName, const QValueList &attrValue, int isAnimated = false);
	void add(const QString &attrName, const VRay::VUtils::CharStringRefList &attrValue, int isAnimated = false);
	void add(const QString &attrName, const VRay::VUtils::ColorRefList &attrValue, int isAnimated = false);
	void add(const QString &attrName, const VRay::VUtils::FloatRefList &attrValue, int isAnimated = false);
	void add(const QString &attrName, const VRay::VUtils::IntRefList &attrValue, int isAnimated = false);
	void add(const QString &attrName, const VRay::VUtils::ValueRefList &attrValue, int isAnimated = false);
	void add(const QString &attrName, const VRay::VUtils::VectorRefList &attrValue, int isAnimated = false);
	void add(const QString &attrName, exint attrValue, int isAnimated = false);
	void add(const QString &attrName, float attrValue, int isAnimated = false);
	void add(const QString &attrName, float r, float g, float b, int isAnimated = false);
	void add(const QString &attrName, float r, float g, float b, float a, int isAnimated = false);
	void add(const QString &attrName, fpreal attrValue, int isAnimated = false);
	void add(const QString &attrName, int attrValue, int isAnimated = false);
	void add(const QString &attrName, const VRay::PluginRef &attrValue);
	void add(const QString &attrName, const VRay::Plugin &attrValue);
	void add(const QString &attrName, const VRay::Plugin &attrValue, const QString &output);

	/// Set fake parameter to prevent automatic export and disable settings it to the plugin instance.
	void setIngore(const QString &attrName);

	/// Remove attrubute.
	/// @param attrName Attribute name.
	void remove(const QString &attrName);

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
