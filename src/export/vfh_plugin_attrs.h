//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_PLUGIN_ATTRS_H
#define VRAY_FOR_HOUDINI_PLUGIN_ATTRS_H

#include "vfh_defines.h"
#include "vfh_vray.h"

namespace VRayForHoudini {
namespace Attrs {

const float RAD_TO_DEG = M_PI / 180.f;

struct PluginDesc;

/// Descriptor for a plugin attribute:
/// name, type and value
struct PluginAttr {
	/// Available attibute types
	enum AttrType {
		AttrTypeUnknown = 0,
		AttrTypeIgnore, ///< to signal we should ignore updates for this attibute
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
		paramType = PluginAttr::AttrTypeUnknown;
	}

	PluginAttr(const std::string &attrName, const AttrType attrType) {
		paramName = attrName;
		paramType = attrType;
	}

	PluginAttr(const std::string &attrName, const std::string &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeString;
		paramValue.valString = attrValue;
	}

	PluginAttr(const std::string &attrName, const char *attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeString;
		paramValue.valString = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::Matrix &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeMatrix;
		paramValue.valTransform.matrix = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::Transform &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeTransform;
		paramValue.valTransform = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::Plugin &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypePlugin;
		paramValue.valPlugin = attrValue;
	}

	PluginAttr(const std::string &attrName, const PluginDesc *attrValue);

	PluginAttr(const std::string &attrName, const VRay::Plugin &attrValue, const std::string &output) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypePlugin;
		paramValue.valPlugin = attrValue;
		paramValue.valPluginOutput = output;
	}

	PluginAttr(const std::string &attrName, const AttrType &attrType, const float &a, const float &b, const float &c, const float &d=0.0f) {
		paramName = attrName;
		paramType = attrType;
		paramValue.valVector[0] = a;
		paramValue.valVector[1] = b;
		paramValue.valVector[2] = c;
		paramValue.valVector[3] = d;
	}

	PluginAttr(const std::string &attrName, const float &a, const float &b, const float &c, const float &d=1.0f) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeAColor;
		paramValue.valVector[0] = a;
		paramValue.valVector[1] = b;
		paramValue.valVector[2] = c;
		paramValue.valVector[3] = d;
	}

	PluginAttr(const std::string &attrName, const VRay::Vector &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeVector;
		paramValue.valVector[0] = attrValue.x;
		paramValue.valVector[1] = attrValue.y;
		paramValue.valVector[2] = attrValue.z;
		paramValue.valVector[3] = 1.0f;
	}

	PluginAttr(const std::string &attrName, const int &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeInt;
		paramValue.valInt = attrValue;
	}

	PluginAttr(const std::string &attrName, const exint &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeInt;
		paramValue.valInt = static_cast<int>(attrValue);
	}

	PluginAttr(const std::string &attrName, const bool &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeInt;
		paramValue.valInt = attrValue;
	}

	PluginAttr(const std::string &attrName, const float &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeFloat;
		paramValue.valFloat = attrValue;
	}

	PluginAttr(const std::string &attrName, const fpreal &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeFloat;
		paramValue.valFloat = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::IntList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListInt;
		paramValue.valListInt = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::FloatList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListFloat;
		paramValue.valListFloat = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::VectorList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListVector;
		paramValue.valListVector = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::ColorList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListColor;
		paramValue.valListColor = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::ValueList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListValue;
		paramValue.valListValue = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::VUtils::IntRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListInt;
		paramValue.valRawListInt = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::VUtils::FloatRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListFloat;
		paramValue.valRawListFloat = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::VUtils::VectorRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListVector;
		paramValue.valRawListVector = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::VUtils::ColorRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListColor;
		paramValue.valRawListColor = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::VUtils::CharStringRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListCharString;
		paramValue.valRawListCharString = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::VUtils::ValueRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListValue;
		paramValue.valRawListValue = attrValue;
	}

	/// Get attribute type as string
	const char *typeStr() const;

	struct PluginAttrValue {
		PluginAttrValue();

		int                 valInt;
		float               valFloat;
		float               valVector[4];
		std::string         valString;
		VRay::Plugin        valPlugin;
		const PluginDesc   *valPluginDesc;
		std::string         valPluginOutput;
		VRay::Transform     valTransform;

		VRay::IntList       valListInt;
		VRay::FloatList     valListFloat;
		VRay::VectorList    valListVector;
		VRay::ValueList     valListValue;
		VRay::ColorList     valListColor;

		VRay::VUtils::IntRefList         valRawListInt;
		VRay::VUtils::FloatRefList       valRawListFloat;
		VRay::VUtils::VectorRefList      valRawListVector;
		VRay::VUtils::ColorRefList       valRawListColor;
		VRay::VUtils::CharStringRefList  valRawListCharString;
		VRay::VUtils::ValueRefList       valRawListValue;
	} paramValue; ///< attribute value

	std::string             paramName; ///< attribute name
	AttrType                paramType; ///< attribute type
};

typedef VUtils::HashMap<PluginAttr, true, 512, false, 50> PluginAttrs;

/// Description of a plugin instance and its attributes. It is used to
/// accumulate attribute changes and to allow to batch changes together for
/// a plugin.
struct PluginDesc {
	PluginDesc();
	PluginDesc(const std::string &pluginName, const std::string &pluginID);

	/// Test if we have attribute with a given name
	/// @param paramName[in] - attribute name
	bool contains(const std::string &paramName) const;

	/// Append an attrubute to our description. If an attribute with the same
	/// name already exists, it will overwrite it.
	/// @param attr[in] - attribute
	void addAttribute(const PluginAttr &attr);

	/// Append an attrubute to our description. If an attribute with the same
	/// name already exists, it will overwrite it.
	/// @param attr[in] - attribute
	void add(const PluginAttr &attr);

	/// Remove attrubute.
	/// @param name Attribute name.
	void remove(const char *name);

	/// Return the attibute with the specified name or nullptr
	/// @param paramName[in] - attribute name
	const PluginAttr* get(const std::string &paramName) const;
	PluginAttr* get(const std::string &paramName);

	/// Compare for difference our attributes with those of the other plugin
	/// description.
	bool isDifferent(const PluginDesc &otherDesc) const;

	/// Compare for equality our attributes with those of the other plugin
	/// description.
	bool isEqual(const PluginDesc &otherDesc) const;

	/// Plugin instance name.
	std::string pluginName;

	/// Plugin type name.
	std::string pluginID;

	/// A list of plugin attributes.
	PluginAttrs pluginAttrs;
};

} // namespace Attrs
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PLUGIN_ATTRS_H
