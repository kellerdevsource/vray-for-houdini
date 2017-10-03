//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_ENVIRONMENT_DEF_H
#define VRAY_FOR_HOUDINI_VOP_NODE_ENVIRONMENT_DEF_H

#include "vop_node_base.h"
#include "vop_SettingsEnvironment.h"


namespace VRayForHoudini {
namespace VOP {

#define ENV_DEF(PluginID) NODE_BASE_DEF(EFFECT, PluginID)

ENV_DEF(EnvironmentFog)
ENV_DEF(EnvFogMeshGizmo)
ENV_DEF(VolumeVRayToon)

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_ENVIRONMENT_DEF_H
