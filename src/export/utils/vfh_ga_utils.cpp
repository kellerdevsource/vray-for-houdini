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

#include "vfh_ga_utils.h"

#include <UT/UT_Version.h>


const char *VRayForHoudini::GA::getGaAttributeName(const GA_Attribute &gaAttr)
{
#if UT_MAJOR_VERSION_INT >= 15
	return gaAttr.getName().buffer();
#else
	return gaAttr.getName();
#endif
}
