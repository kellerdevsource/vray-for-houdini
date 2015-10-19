//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_brdfdiffuse.h"


using namespace VRayForHoudini;


void VOP::BRDFDiffuse::setPluginType()
{
	pluginType = "BRDF";
	pluginID   = "BRDFDiffuse";
}


void VOP::BRDFDiffuse::getCode(UT_String &codestr, const VOP_CodeGenContext &)
{
	codestr.sprintf("Cf = {%.3f,%.3f,%.3f};",
					1.0f, 0.0f, 0.0f);
}
