//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//  https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_sys_utils.h"

#include <stdlib.h>

using namespace VRayForHoudini;
using namespace Sys;

const char *GetEnvVar::getValue()
{
	if (!initialized) {
		varValue = getenv(varName);
		initialized = true;
	}
	return varValue;
}

const char *GetEnvVar::getName() const
{
	return varName;
}

void GetEnvVar::resetValue()
{
	initialized = false;
}
