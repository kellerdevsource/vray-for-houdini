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

#include "vfh_prm_defaults.h"


using namespace VRayForHoudini;


PRM_Default    Parm::PRMemptyStringDefault(0.0f, "");
PRM_SpareData  Parm::PRMcurveDefault(PRM_SpareToken("rampshowcontrolsdefault", "1"));
PRM_SpareData  Parm::RPMrampDefault(PRM_SpareToken("rampcolordefault", "1pos ( 0 ) 1c ( 0 0 1 ) 1interp ( linear )  2pos ( 1 ) 2c ( 1 0 0 ) 2interp ( linear )"));
