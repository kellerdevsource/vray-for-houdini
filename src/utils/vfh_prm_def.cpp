//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
//

#include "vfh_prm_def.h"


using namespace VRayForHoudini;
using namespace VRayForHoudini::Parm;


ParmDefValue::PRM_DefaultPtrList ParmDefValue::PrmDefPtrList;
ParmDefValue::PRM_DefaultPtrList ParmDefValue::PrmDefArrPtrList;


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
