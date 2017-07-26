//
// Copyright (c) 2015-2016, Chaos Software Ltd
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

boost::format Parm::FmtPrefix("%s_");
boost::format Parm::FmtPrefixAuto("%s_%s");
boost::format Parm::FmtPrefixManual("%s%s");

bool Parm::isVRayParm(const PRM_Template *parm)
{
	if (!parm)
		return false;

	const PRM_SpareData *parmSpare = parm->getSparePtr();
	if (!parmSpare)
		return false;

	return UTisstring(parmSpare->getValue("vray_type"));
}
