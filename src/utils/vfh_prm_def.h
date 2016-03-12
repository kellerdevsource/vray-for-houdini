//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_PRM_DEF_H
#define VRAY_FOR_HOUDINI_PRM_DEF_H

#include "vfh_prm_defaults.h"

#include <PRM/PRM_Name.h>
#include <PRM/PRM_Shared.h>
#include <PRM/PRM_ChoiceList.h>
#include <PRM/PRM_Template.h>
#include <PRM/PRM_Range.h>

// For VOP_Type
#include <VOP/VOP_Node.h>

#include <boost/format.hpp>

namespace VRayForHoudini {
namespace Parm {

extern boost::format FmtPrefix;
extern boost::format FmtPrefixAuto;
extern boost::format FmtPrefixManual;

typedef std::vector<PRM_Template> PRMTmplList;
typedef std::vector<PRM_Default>  PRMDefList;

enum ParmType {
	eInt = 0,
	eFloat,
	eFloatList,
	eEnum,
	eBool,
	eColor,
	eAColor,
	eString,
	eTextureColor,
	eTextureFloat,
	eTextureInt,
	eManualExportStart,
	eCurve,
	eRamp,
	ePlugin,
	eManualExportEnd,
	eOutputPlugin,
	eOutputColor,
	eOutputTextureColor,
	eOutputTextureFloat,
	eOutputTextureInt,
	eOutputTextureVector,
	eOutputTextureMatrix,
	eOutputTextureTransform,
	eUnknown,
};

enum PluginType {
	PluginTypeUnknown = 0,
	PluginTypeBRDF,
	PluginTypeCamera,
	PluginTypeRenderChannel,
	PluginTypeEffect,
	PluginTypeFilter,
	PluginTypeGeometry,
	PluginTypeLight,
	PluginTypeMaterial,
	PluginTypeObject,
	PluginTypeSettings,
	PluginTypeTexture,
	PluginTypeUvwgen,
};

enum ParmSubtype {
	eFilepath = 0,
	eDirpath,
	eNone
};


struct ParmRampDesc {
	std::string  colors;
	std::string  positions;
	std::string  interpolations;
};


struct ParmCurveDesc {
	std::string  positions;
	std::string  values;
	std::string  interpolations;
};


struct ParmEnumItem {
	std::string  value;
	std::string  label;
	std::string  desc;
};
typedef std::vector<ParmEnumItem> ParmEnumItems;


struct EnumItem {
	enum EnumValueType {
		EnumValueInt = 0,
		EnumValueString
	};

	EnumItem():
		valueType(EnumItem::EnumValueInt)
	{}

	std::string    label;
	std::string    desc;

	EnumValueType  valueType;
	int            value;
	// For string enum
	// NOTE: AFAIR, UVWGenEnvironment "mapping_type" only
	std::string    valueString;
};
typedef std::vector<EnumItem> EnumItems;


struct ParmDefValue {
	typedef std::vector<PRM_Default*> PRM_DefaultPtrList;

	static PRM_DefaultPtrList PrmDefPtrList;
	static PRM_DefaultPtrList PrmDefArrPtrList;

	static void FreeData() {
		for (auto pIt : ParmDefValue::PrmDefPtrList) {
			delete pIt;
		}
		for (auto pIt : ParmDefValue::PrmDefArrPtrList) {
			delete [] pIt;
		}
		ParmDefValue::PrmDefPtrList.clear();
		ParmDefValue::PrmDefArrPtrList.clear();
	}

	ParmDefValue():
		type(eUnknown),
		subType(eNone),
		defInt(0),
		defEnum(0),
		defFloat(1.0f),
		defBool(false)
#ifndef _WIN32
		,defColor{0.0f,0.0f,0.0f}
		,defAColor{0.0f,0.0f,0.0f,1.0f}
#endif
	{}

	PRM_Default* getDefBool() const {
		if (defBool)
			return PRMoneDefaults;
		return PRMzeroDefaults;
	}

	PRM_Default* getDefFloat() const {
		PRM_Default *prm_def = new PRM_Default((fpreal)defFloat);
		PrmDefPtrList.push_back(prm_def);
		return prm_def;
	}

	PRM_Default* getDefInt() const {
		PRM_Default *prm_def = new PRM_Default((fpreal)defInt);
		PrmDefPtrList.push_back(prm_def);
		return prm_def;
	}

	PRM_Default* getDefString() const {
		PRM_Default *prm_def = new PRM_Default(0.0f, defString.c_str());
		PrmDefPtrList.push_back(prm_def);
		return prm_def;
	}

	PRM_Default* getDefColor() const {
		PRM_Default *prm_def = new PRM_Default[4];
		if (type == eColor) {
			for (int i = 0; i < 3; ++i)
				prm_def[i].setFloat(defColor[i]);
			prm_def[3].setFloat(1.0f);
		}
		else {
			for (int i = 0; i < 4; ++i)
				prm_def[i].setFloat(defAColor[i]);
		}
		PrmDefArrPtrList.push_back(prm_def);
		return prm_def;
	}

	const char     *typeStr() const;

	ParmType        type;
	ParmSubtype     subType;

	int             defInt;
	float           defFloat;
	bool            defBool;
	float           defColor[3];
	float           defAColor[4];
	std::string     defString;
	int             defEnum;
	EnumItems       defEnumItems;

	ParmRampDesc    defRamp;
	ParmCurveDesc   defCurve;

};


struct AttrDesc {
	AttrDesc()
		: custom_handling(false)
		, linked_only(false)
		, convert_to_radians(false)
	{}

	std::string  attr;
	std::string  label;
	std::string  desc;

	ParmDefValue value;
	int          custom_handling;
	int          linked_only;
	int          convert_to_radians;

	// Custom template
	PRMTmplList  custom_template;
};

typedef std::map<std::string, AttrDesc>       AttributeDescs;
typedef std::map<std::string, AttributeDescs> PluginDescriptions;


struct SocketDesc {
	SocketDesc() {}

	SocketDesc(PRM_Name name, VOP_Type vopType, ParmType type=ParmType::eUnknown):
		name(name),
		vopType(vopType),
		type(type)
	{}

	// Name pair for UI
	PRM_Name  name;
	ParmType  type;

	// Socket type
	VOP_Type  vopType;
};
typedef std::vector<SocketDesc> SocketsDesc;

struct TabItemDesc {
	const char *label;
	const char *pluginID;
};

void addTabItems(const Parm::TabItemDesc &tabItemDesc, PRMDefList &switcher, PRMTmplList &prmTemplate);
void addTabsItems(Parm::TabItemDesc tabItemsDesc[], int tabItemsDescCount, PRMDefList &switcher, PRMTmplList &prmTemplate);

void addTabWithTabs(const char *label,
					TabItemDesc tabItemsDesc[], int tabItemsDescCount,
					PRMDefList &switcher, PRM_Name &switcherName,
					PRMTmplList &prmTemplate, PRMDefList &mainSwitcher);

} // namespace Parm
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PRM_DEF_H
