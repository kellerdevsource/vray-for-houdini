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

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_UVWGEN_DEF_H
#define VRAY_FOR_HOUDINI_VOP_NODE_UVWGEN_DEF_H

#include "vop_node_base.h"


namespace VRayForHoudini {
namespace VOP {

#define UVWGEN_DEF(PluginID) NODE_BASE_DEF("UVWGEN", PluginID)

UVWGEN_DEF(UVWGenBercon)
UVWGEN_DEF(UVWGenC4D)
UVWGEN_DEF(UVWGenChannel)
UVWGEN_DEF(UVWGenEnvironment)
UVWGEN_DEF(UVWGenExplicit)
UVWGEN_DEF(UVWGenMayaPlace2dTexture)
UVWGEN_DEF(UVWGenObject)
UVWGEN_DEF(UVWGenObjectBBox)
UVWGEN_DEF(UVWGenPlanarWorld)
UVWGEN_DEF(UVWGenProjection)
UVWGEN_DEF(UVWGenSwitch)

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_UVWGEN_DEF_H
