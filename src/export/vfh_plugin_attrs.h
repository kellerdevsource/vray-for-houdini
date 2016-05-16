//
// Copyright (c) 2015-2016, Chaos Software Ltd
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

struct PluginAttr {
	enum AttrType {
		AttrTypeUnknown = 0,
		AttrTypeIgnore,
		AttrTypeInt,
		AttrTypeFloat,
		AttrTypeVector,
		AttrTypeColor,
		AttrTypeAColor,
		AttrTypeTransform,
		AttrTypeMatrix,
		AttrTypeString,
		AttrTypePlugin,
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

	const char *typeStr() const {
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

	struct PluginAttrValue {
		int                 valInt;
		float               valFloat;
		float               valVector[4];
		std::string         valString;
		VRay::Plugin        valPlugin;
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
	} paramValue;

	std::string             paramName;
	AttrType                paramType;

};
typedef std::vector<PluginAttr> PluginAttrs;


struct PluginDesc {
	PluginDesc() {}
	PluginDesc(const std::string &pluginName, const std::string &pluginID):
		pluginName(pluginName),
		pluginID(pluginID)
	{}

	bool              contains(const std::string &paramName) const;
	void              addAttribute(const PluginAttr &attr);
	void              add(const PluginAttr &attr);
	const PluginAttr *get(const std::string &paramName) const;
	PluginAttr       *get(const std::string &paramName);

	bool              isDifferent(const PluginDesc &otherDesc) const;
	bool              isEqual(const PluginDesc &otherDesc) const;
	void              showAttributes() const;

	std::string       pluginID;
	std::string       pluginName;
	PluginAttrs       pluginAttrs;
};

} // namespace Attrs
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PLUGIN_ATTRS_H
