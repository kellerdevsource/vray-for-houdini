//
// Copyright (c) 2015, Chaos Software Ltd
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

	PluginAttr(const std::string &attrName, const VUtils::IntRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListInt;
		paramValue.valRawListInt = attrValue;
	}

	PluginAttr(const std::string &attrName, const VUtils::FloatRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListFloat;
		paramValue.valRawListFloat = attrValue;
	}

	PluginAttr(const std::string &attrName, const VUtils::VectorRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListVector;
		paramValue.valRawListVector = attrValue;
	}

	PluginAttr(const std::string &attrName, const VUtils::ColorRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListColor;
		paramValue.valRawListColor = attrValue;
	}

	const char *typeStr() const {
		switch (paramType) {
			case AttrTypeInt: return "Int";
			case AttrTypeFloat: return "Float";
			case AttrTypeVector: return "Vector";
			case AttrTypeColor: return "Color";
			case AttrTypeAColor: return "AColor";
			case AttrTypeTransform: return "Transform";
			case AttrTypeString: return "String";
			case AttrTypePlugin: return "Plugin";
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

		VUtils::ColorRefList  valRawListColor;
		VUtils::VectorRefList valRawListVector;
		VUtils::IntRefList    valRawListInt;
		VUtils::FloatRefList  valRawListFloat;
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

	void addAttribute(const PluginAttr &attr) {
		PluginAttr *_attr = get(attr.paramName);
		if (_attr) {
			*_attr = attr;
		}
		else {
			pluginAttrs.push_back(attr);
		}
	}

	void add(const PluginAttr &attr) {
		addAttribute(attr);
	}

	std::string             pluginID;
	std::string             pluginName;
	PluginAttrs             pluginAttrs;

	bool contains(const std::string &paramName) const {
		if (get(paramName)) {
			return true;
		}
		return false;
	}

	const PluginAttr *get(const std::string &paramName) const {
		for (const auto &pIt : pluginAttrs) {
			const PluginAttr &p = pIt;
			if (paramName == p.paramName) {
				return &p;
			}
		}
		return nullptr;
	}

	PluginAttr *get(const std::string &paramName) {
		for (auto &pIt : pluginAttrs) {
			PluginAttr &p = pIt;
			if (paramName == p.paramName) {
				return &p;
			}
		}
		return nullptr;
	}

	void showAttributes() const {
		PRINT_INFO("Plugin \"%s.%s\" parameters:",
				   pluginID.c_str(), pluginName.c_str())
				for (const auto &pIt : pluginAttrs) {
			const PluginAttr &p = pIt;
			PRINT_INFO("  %s [%s]",
					   p.paramName.c_str(), p.typeStr());
		}
	}
};

} // namespace Attrs
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PLUGIN_ATTRS_H
