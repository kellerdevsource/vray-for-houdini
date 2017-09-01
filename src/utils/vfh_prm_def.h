//
// Copyright (c) 2015-2017, Chaos Software Ltd
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

/// Type of parameter values
enum ParmType {
	eInt = 0,
	eFloat,
	eFloatList,
	eEnum,
	eBool,
	eColor,
	eAColor,
	eString,
	eVector,
	eMatrix,
	eTransform,
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

/// Types of plugins
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

/// String type subtypes
enum ParmSubtype {
	eFilepath = 0,
	eDirpath,
	eNone
};

/// Descriptor for a color ramp param
struct ParmRampDesc {
	std::string  colors; ///< The name of the property for values
	std::string  positions; ///< The name of the property for positions
	std::string  interpolations; ///< The name of the property for interpolations
};

/// Descriptor for a curve param
struct ParmCurveDesc {
	std::string  positions; ///< The name of the property for positions
	std::string  values; ///< The name of the property for values
	std::string  interpolations; ///< The name of the property for itnerpolations
};

/// Descriptor for enum param
struct ParmEnumItem {
	std::string  value; ///< The value as string for the param
	std::string  label; ///< The display label for this valu
	std::string  desc; ///< The desctription for the value
};
typedef std::vector<ParmEnumItem> ParmEnumItems;

/// Descriptor for enum plugin parameter
struct EnumItem {
	enum EnumValueType {
		EnumValueInt = 0,
		EnumValueString
	};

	EnumItem():
		valueType(EnumItem::EnumValueInt)
	{}

	std::string    label; ///< The label for this enum
	std::string    desc; ///< The desctiption

	EnumValueType  valueType; ///< The type of the enum
	int            value; ///< Value as int

	/// For string enum
	/// NOTE: UVWGenEnvironment "mapping_type" only
	std::string    valueString;
};
typedef std::vector<EnumItem> EnumItems;

/// Factory for PRM_Default objects based on provided values
struct ParmDefValue {
	typedef std::vector<PRM_Default*> PRM_DefaultPtrList;

	static PRM_DefaultPtrList PrmDefPtrList;
	static PRM_DefaultPtrList PrmDefArrPtrList;

	/// Free all alocated data for default values
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

	/// Initialize all values to 0
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
		,defMatrix{1.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,0.0f}
#endif
	{
#ifdef _WIN32
		memset(defColor, 0, sizeof(defColor) * sizeof(defColor[0]));
		memset(defAColor, 0, sizeof(defAColor) * sizeof(defAColor[0]));
		defAColor[3] = 1.0f;
		memset(defMatrix, 0, sizeof(defMatrix) * sizeof(defMatrix[0]));
		// identity matrix
		for (int c = 0; c < 4; ++c) {
			defMatrix[c * 4 + c] = 1.0f;
		}
#endif
	}

	/// Get bool default
	PRM_Default* getDefBool() const {
		if (defBool)
			return PRMoneDefaults;
		return PRMzeroDefaults;
	}

	/// Get float default
	PRM_Default* getDefFloat() const {
		PRM_Default *prm_def = new PRM_Default((fpreal)defFloat);
		PrmDefPtrList.push_back(prm_def);
		return prm_def;
	}

	/// Get int default
	PRM_Default* getDefInt() const {
		PRM_Default *prm_def = new PRM_Default((fpreal)defInt);
		PrmDefPtrList.push_back(prm_def);
		return prm_def;
	}

	/// Get string default
	PRM_Default* getDefString() const {
		PRM_Default *prm_def = new PRM_Default(0.0f, defString.c_str());
		PrmDefPtrList.push_back(prm_def);
		return prm_def;
	}

	/// Get color default
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

	/// Get vector default
	PRM_Default* getDefVector() const {
		PRM_Default *prm_def = new PRM_Default[4];

		for (int i = 0; i < 3; ++i) {
			prm_def[i].setFloat(defColor[i]);
		}
		prm_def[3].setFloat(0.0f);

		PrmDefArrPtrList.push_back(prm_def);
		return prm_def;
	}

	/// Get the current type as string
	const char     *typeStr() const;

	ParmType        type; ///< Param type
	ParmSubtype     subType; ///< Param subtype

	/// Values for all supported types
	int             defInt;
	float           defFloat;
	bool            defBool;
	float           defColor[3];
	float           defAColor[4];
	std::string     defString;
	int             defEnum;
	EnumItems       defEnumItems;
	float           defMatrix[16];

	ParmRampDesc    defRamp;
	ParmCurveDesc   defCurve;

};

/// Descriptor for a single param
struct AttrDesc {
	AttrDesc()
		: custom_handling(false)
		, linked_only(false)
		, convert_to_radians(false)
	{}

	std::string  attr; ///< Attribute name
	std::string  label; ///< Attribute label
	std::string  desc; ///< Attribute description

	ParmDefValue value; ///< Default value for this param
	int          custom_handling; ///< 1 if this param requires custom handling
	int          linked_only; ///< 1 if this link should be skipped during auto export
	int          convert_to_radians; ///< 1 if this needs to be converted from degrees to radians
};

typedef std::map<std::string, AttrDesc>       AttributeDescs;
typedef std::map<std::string, AttributeDescs> PluginDescriptions;

/// Descriptor for a plugin's socket
struct SocketDesc {
	SocketDesc() {}

	SocketDesc(PRM_Name name, VOP_Type vopType, ParmType type=ParmType::eUnknown):
		name(name),
		vopType(vopType),
		type(type)
	{}

	PRM_Name  name; ///< Name for UI
	ParmType  type;	///< UI type

	VOP_Type  vopType; ///< Socket type
};
typedef std::vector<SocketDesc> SocketsDesc;


} // namespace Parm
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PRM_DEF_H
