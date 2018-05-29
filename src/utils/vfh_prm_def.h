//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_PRM_DEF_H
#define VRAY_FOR_HOUDINI_PRM_DEF_H

#include "vfh_vray.h"
#include "vfh_prm_defaults.h"

// For VOP_Type
#include <VOP/VOP_Node.h>

namespace VRayForHoudini {
namespace Parm {

/// Type of parameter values
enum ParmType {
	eInt = 0,
	eFloat,
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
	eListNode,
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
		QString colors; ///< The name of the property for values
		QString positions; ///< The name of the property for positions
		QString interpolations; ///< The name of the property for interpolations
		int colorAsTexture = false; ///< Export colors list as TexAColor.
	};

	/// Descriptor for a curve param
	struct ParmCurveDesc {
		QString positions; ///< The name of the property for positions
		QString values; ///< The name of the property for values
		QString interpolations; ///< The name of the property for itnerpolations
	};

	/// Descriptor for Node list parameter.
	struct ParmNodeListDesc {
		/// The name of the property that sets the list to be inclusive.
		QString inclusiveFlag;

		/// Is list exclusive by default.
		int exclusive = true;
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

	/// Data for exporting exclude lists.
	ParmNodeListDesc nodeList;
};

enum AttrDescFlags {
	attrFlagNone           = 0,
	attrFlagCustomHandling = (1 << 0), ///< Parameter requires custom handling.
	attrFlagLinkedOnly     = (1 << 1), ///< Skip parameter if socket is not connected.
	attrFlagToRadians      = (1 << 2), ///< Value needs to be converted from degrees to radians.
	attrFlagEnabledOnly    = (1 << 3), ///< Skip parameter is it's UI is disabled or hidden.
};

/// Descriptor for a single param
struct AttrDesc {
	AttrDesc()
		: flags(attrFlagNone)
	{}

	/// Plugin attribute name.
	QString attr;

	/// UI label.
	QString label;

	/// UI help.
	QString desc;

	/// Value type for this parameter.
	ParmDefValue value;

	/// Parameter export flags.
	uint32_t flags;
};

typedef QMap<QString, AttrDesc> AttributeDescs;

struct SocketDesc {
	/// UI label.
	QString label;

	/// Plugin attribute name.
	/// May be empty for the default outputs.
	QString attrName;

	/// Plugin attribute type.
	ParmType attrType = eUnknown;

	/// UI token.
	UT_String socketLabel;

	/// Socket type.
	VOP_Type socketType = VOP_TYPE_UNDEF;
};

typedef QList<SocketDesc> VRayNodeSockets;

/// Returns true if current parameter template represents V-Ray Plugin parameter.
/// @param prm PRM_Parm instance. May be nullptr.
bool isVRayParm(const PRM_Template *prm);

} // namespace Parm
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PRM_DEF_H
