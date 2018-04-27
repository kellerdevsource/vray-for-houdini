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

namespace VRayForHoudini {
namespace Attrs {

struct PluginDesc;

/// Descriptor for a plugin attribute:
/// name, type and value
struct PluginAttr {
	/// Available parameter types.
	enum AttrType {
		AttrTypeUnknown = 0,
		AttrTypeIgnore, ///< To signal we should ignore updates for this parameter.
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
		AttrTypeListInt,
		AttrTypeListFloat,
		AttrTypeListVector,
		AttrTypeListColor,
		AttrTypeListTransform,
		AttrTypeListString,
		AttrTypeListPlugin,
		AttrTypeListValue,
		AttrTypeRawListInt,
		AttrTypeRawListFloat,
		AttrTypeRawListVector,
		AttrTypeRawListColor,
		AttrTypeRawListCharString,
		AttrTypeRawListValue,
	};

	PluginAttr() {
		paramName.clear();
		paramType = AttrTypeUnknown;
	}

	PluginAttr(const QString &attrName, const AttrType attrType) {
		paramName = attrName;
		paramType = attrType;
	}

	PluginAttr(const QString &attrName, const QString &attrValue) {
		paramName = attrName;
		paramType = AttrTypeString;
		paramValue.valString = attrValue;
	}

	PluginAttr(const QString &attrName, const char *attrValue) {
		paramName = attrName;
		paramType = AttrTypeString;
		paramValue.valString = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::Matrix &attrValue) {
		paramName = attrName;
		paramType = AttrTypeMatrix;
		paramValue.valTransform.matrix = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::Transform &attrValue) {
		paramName = attrName;
		paramType = AttrTypeTransform;
		paramValue.valTransform = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::Plugin &attrValue) {
		paramName = attrName;
		paramType = AttrTypePlugin;
		paramValue.valPlugin = attrValue;
	}

	PluginAttr(const QString &attrName, const PluginDesc *attrValue);

	PluginAttr(const QString &attrName, const VRay::Plugin &attrValue, const QString &output) {
		paramName = attrName;
		paramType = AttrTypePlugin;
		paramValue.valPlugin = attrValue;
		paramValue.valPluginOutput = output;
	}

	PluginAttr(const QString &attrName, const AttrType &attrType, const float &a, const float &b, const float &c, const float &d=0.0f) {
		paramName = attrName;
		paramType = attrType;
		paramValue.valVector[0] = a;
		paramValue.valVector[1] = b;
		paramValue.valVector[2] = c;
		paramValue.valVector[3] = d;
	}

	PluginAttr(const QString &attrName, const float &a, const float &b, const float &c, const float &d=1.0f) {
		paramName = attrName;
		paramType = AttrTypeAColor;
		paramValue.valVector[0] = a;
		paramValue.valVector[1] = b;
		paramValue.valVector[2] = c;
		paramValue.valVector[3] = d;
	}

	PluginAttr(const QString &attrName, const VRay::Vector &attrValue) {
		paramName = attrName;
		paramType = AttrTypeVector;
		paramValue.valVector[0] = attrValue.x;
		paramValue.valVector[1] = attrValue.y;
		paramValue.valVector[2] = attrValue.z;
		paramValue.valVector[3] = 1.0f;
	}

	PluginAttr(const QString &attrName, const int &attrValue) {
		paramName = attrName;
		paramType = AttrTypeInt;
		paramValue.valInt = attrValue;
	}

	PluginAttr(const QString &attrName, const exint &attrValue) {
		paramName = attrName;
		paramType = AttrTypeInt;
		paramValue.valInt = static_cast<int>(attrValue);
	}

	PluginAttr(const QString &attrName, const bool &attrValue) {
		paramName = attrName;
		paramType = AttrTypeInt;
		paramValue.valInt = attrValue;
	}

	PluginAttr(const QString &attrName, const float &attrValue) {
		paramName = attrName;
		paramType = AttrTypeFloat;
		paramValue.valFloat = attrValue;
	}

	PluginAttr(const QString &attrName, const fpreal &attrValue) {
		paramName = attrName;
		paramType = AttrTypeFloat;
		paramValue.valFloat = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::IntList &attrValue) {
		paramName = attrName;
		paramType = AttrTypeListInt;
		paramValue.valListInt = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::FloatList &attrValue) {
		paramName = attrName;
		paramType = AttrTypeListFloat;
		paramValue.valListFloat = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::VectorList &attrValue) {
		paramName = attrName;
		paramType = AttrTypeListVector;
		paramValue.valListVector = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::ColorList &attrValue) {
		paramName = attrName;
		paramType = AttrTypeListColor;
		paramValue.valListColor = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::ValueList &attrValue) {
		paramName = attrName;
		paramType = AttrTypeListValue;
		paramValue.valListValue = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::VUtils::IntRefList &attrValue) {
		paramName = attrName;
		paramType = AttrTypeRawListInt;
		paramValue.valRawListInt = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::VUtils::FloatRefList &attrValue) {
		paramName = attrName;
		paramType = AttrTypeRawListFloat;
		paramValue.valRawListFloat = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::VUtils::VectorRefList &attrValue) {
		paramName = attrName;
		paramType = AttrTypeRawListVector;
		paramValue.valRawListVector = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::VUtils::ColorRefList &attrValue) {
		paramName = attrName;
		paramType = AttrTypeRawListColor;
		paramValue.valRawListColor = attrValue;
	}

	PluginAttr(const QString &attrName, const VRay::VUtils::CharStringRefList &attrValue) {
		paramName = attrName;
		paramType = AttrTypeRawListCharString;
		paramValue.valRawListCharString = attrValue;
	}
	
	/// Override constructor for ValueRefList parameter.
	/// @param name Plugin parameter name.
	/// @param value Plugin parameter value as ValueRefList.
	/// @param isAnimatedGenericList ValueRefList should create key-frames.
	PluginAttr(const QString &name, const VRay::VUtils::ValueRefList &value, int isAnimatedGenericList = false)
		: paramName(name)
		, paramType(AttrTypeRawListValue)
		, isAnimatedGenericList(isAnimatedGenericList)
	{
		paramValue.valRawListValue = value;
	}

	/// Get attribute type as string
	const char *typeStr() const;

	struct PluginAttrValue {
		int valInt = 0;
		float valFloat = 0.0f;
		float valVector[4] = { 0.0f, 0.0f, 0.0f, 0.0f};
		QString valString;
		VRay::Plugin valPlugin;
		const PluginDesc *valPluginDesc = nullptr;
		QString valPluginOutput;
		VRay::Transform valTransform;

		VRay::IntList valListInt;
		VRay::FloatList valListFloat;
		VRay::VectorList valListVector;
		VRay::ValueList valListValue;
		VRay::ColorList valListColor;

		VRay::VUtils::IntRefList valRawListInt;
		VRay::VUtils::FloatRefList valRawListFloat;
		VRay::VUtils::VectorRefList valRawListVector;
		VRay::VUtils::ColorRefList valRawListColor;
		VRay::VUtils::CharStringRefList valRawListCharString;
		VRay::VUtils::ValueRefList valRawListValue;
	};

	/// Plugin parameter name.
	QString paramName;

	/// Plugin parameter type.
	AttrType paramType;

	/// Plugin parameter value.
	PluginAttrValue paramValue;

	/// Parameter value needs to be exported as an animated generic list.
	int isAnimatedGenericList{0};
};

typedef QMap<QString, PluginAttr> PluginAttrs;

/// Description of a plugin instance and its attributes. It is used to
/// accumulate attribute changes and to allow to batch changes together for
/// a plugin.
struct PluginDesc {
	explicit PluginDesc(const QString &pluginName="", const QString &pluginID="");

	/// Test if we have attribute with a given name
	/// @param paramName attribute name
	bool contains(const QString &paramName) const;

	/// Append an attrubute to our description. If an attribute with the same
	/// name already exists, it will overwrite it.
	/// @param attr attribute
	void add(const PluginAttr &attr);

	/// Remove attrubute.
	/// @param name Attribute name.
	void remove(const char *name);

	/// Return the attibute with the specified name or nullptr
	/// @param paramName Attribute name
	const PluginAttr* get(const QString &paramName) const;

	/// Return the attibute with the specified name or nullptr
	/// @param paramName Attribute name
	PluginAttr* get(const QString &paramName);

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
