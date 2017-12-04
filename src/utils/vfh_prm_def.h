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

#include <hash_map.h>

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
	eTextureVector,
	eTextureMatrix,
	eTextureTransform,
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

/// String type subtypes
enum ParmSubtype {
	eFilepath = 0,
	eDirpath,
	eNone
};

/// Factory for PRM_Default objects based on provided values
struct ParmDefValue {
	/// Descriptor for a color ramp param
	struct ParmRampDesc {
		std::string colors; ///< The name of the property for values
		std::string positions; ///< The name of the property for positions
		std::string interpolations; ///< The name of the property for interpolations
	};

	/// Descriptor for a curve param
	struct ParmCurveDesc {
		std::string positions; ///< The name of the property for positions
		std::string values; ///< The name of the property for values
		std::string interpolations; ///< The name of the property for itnerpolations
	};

	ParmDefValue()
		: type(eUnknown)
	{}

	/// Parameter type.
	ParmType type;

	/// Data for exporting color ramp.
	ParmRampDesc colorRampInfo;

	/// Data for exporting curve ramp.
	ParmCurveDesc curveRampInfo;
};

enum AttrDescFlags {
	attrFlagNone           = 0,
	attrFlagCustomHandling = (1 << 0), ///< Parameter requires custom handling.
	attrFlagLinkedOnly     = (1 << 1), ///< Skip parameter if socket is not connected.
	attrFlagToRadians      = (1 << 2), ///< Value needs to be converted from degrees to radians.
};

/// Descriptor for a single param
struct AttrDesc {
	AttrDesc()
		: flags(attrFlagNone)
	{}

	/// Plugin attribute name.
	VUtils::CharString attr;

	/// UI label.
	VUtils::CharString label;

	/// UI help.
	VUtils::CharString desc;

	/// Value type for this parameter.
	ParmDefValue value;

	/// Parameter export flags.
	uint32_t flags;
};

typedef VUtils::HashMap<AttrDesc, true, 512, false, 32> AttributeDescs;

struct SocketDesc {
	SocketDesc()
		: attrType(eUnknown)
		, socketType(VOP_TYPE_UNDEF)
	{}

	/// UI label.
	VUtils::CharString label;

	/// Plugin attribute name.
	/// May be empty for the default outputs.
	VUtils::CharString attrName;

	/// Plugin attribute type.
	ParmType attrType;

	/// UI token.
	VUtils::CharString socketLabel;

	/// Socket type.
	VOP_Type socketType;
};

typedef VUtils::Table<SocketDesc, 10> VRayNodeSockets;

/// Returns true if current parameter template represents V-Ray Plugin parameter.
/// @param prm PRM_Parm instance. May be nullptr.
bool isVRayParm(const PRM_Template *prm);

} // namespace Parm
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PRM_DEF_H
