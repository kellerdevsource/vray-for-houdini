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

#ifndef VRAY_FOR_HOUDINI_GA_UTILS_H
#define VRAY_FOR_HOUDINI_GA_UTILS_H

#include "vop_node_base.h"

#include <GA/GA_Attribute.h>


namespace VRayForHoudini {
namespace GA {

const char *getGaAttributeName(const GA_Attribute &gaAttr);

} // namespace GA
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_GA_UTILS_H
