//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_prm_defaults.h"


using namespace VRayForHoudini;


PRM_Default    Parm::PRMemptyStringDefault(0.0f, "");
PRM_SpareData  Parm::PRMcurveDefault(PRM_SpareToken("rampshowcontrolsdefault", "1"));
PRM_SpareData  Parm::PRMrampDefault(PRM_SpareToken("rampcolordefault", "1pos ( 0 ) 1c ( 0 0 1 ) 1interp ( linear )  2pos ( 1 ) 2c ( 1 0 0 ) 2interp ( linear )"));
