//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_UVWGEN_DEF_H
#define VRAY_FOR_HOUDINI_VOP_NODE_UVWGEN_DEF_H

#include "vop_node_base.h"


namespace VRayForHoudini {
namespace VOP {

#define UVWGEN_DEF(PluginID) NODE_BASE_DEF(UVWGEN, PluginID)

UVWGEN_DEF(UVWGenBercon)
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
