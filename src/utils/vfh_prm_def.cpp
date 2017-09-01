//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_prm_def.h"


using namespace VRayForHoudini;
using namespace VRayForHoudini::Parm;


ParmDefValue::PRM_DefaultPtrList ParmDefValue::PrmDefPtrList;
ParmDefValue::PRM_DefaultPtrList ParmDefValue::PrmDefArrPtrList;


boost::format Parm::FmtPrefix("%s_");
boost::format Parm::FmtPrefixAuto("%s_%s");
boost::format Parm::FmtPrefixManual("%s%s");


const char *ParmDefValue::typeStr() const
{
	switch (type) {
		case eInt:
			return "Int";
		case eFloat:
			return "Float";
		case eEnum:
			return "Enum";
		case eBool:
			return "Bool";
		case eColor:
			return "Color";
		case eAColor:
			return "AColor";
		case eString:
			return "String";
		case eTextureColor:
			return "TextureColor";
		case eTextureFloat:
			return "TextureFloat";
		case eTextureInt:
			return "TextureInt";
		case eCurve:
			return "Curve";
		case eRamp:
			return "Ramp";
		case ePlugin:
			return "Plugin";
		case eOutputPlugin:
			return "OutputPlugin";
		case eOutputColor:
			return "OutputColor";
		case eOutputTextureColor:
			return "OutputTextureColor";
		case eOutputTextureFloat:
			return "OutputTextureFloat";
		case eOutputTextureInt:
			return "OutputTextureInt";
		case eOutputTextureVector:
			return "OutputTextureVector";
		case eOutputTextureMatrix:
			return "OutputTextureMatrix";
		case eOutputTextureTransform:
			return "OutputTextureTransform";
		default:
			break;
	}
	return "Unknown";
}
