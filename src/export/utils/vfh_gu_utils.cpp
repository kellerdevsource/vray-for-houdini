//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_gu_utils.h"

#include <GA/GA_Handle.h>

using namespace VRayForHoudini;

int VRayForHoudini::GU::isHairGdp(const GU_Detail &gdp)
{
	// XXX: Check if this is still valid.
	const GA_ROHandleF &guardhairHndl = gdp.findAttribute(GA_ATTRIB_PRIMITIVE, "guardhair");
	const GA_ROHandleF &hairidHndl = gdp.findAttribute(GA_ATTRIB_PRIMITIVE, "hairid");

	const GA_ROHandleF &widthHnld = gdp.findAttribute(GA_ATTRIB_POINT, "width");
	const GA_ROHandleF &pscaleHnld = gdp.findAttribute(GA_ATTRIB_POINT, "pscale");

	const int isHair = ((guardhairHndl.isValid() && hairidHndl.isValid()) ||
	                    widthHnld.isValid() ||
	                    pscaleHnld.isValid());

	return isHair;
}
