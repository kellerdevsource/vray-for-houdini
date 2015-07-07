
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

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_ENVIRONMENT_DEF_H
#define VRAY_FOR_HOUDINI_VOP_NODE_ENVIRONMENT_DEF_H

#include "vop_node_base.h"
#include "vop_env_context.h"
#include "vop_SettingsEnvironment.h"


namespace VRayForHoudini {
namespace VOP {

#define ENV_DEF(PluginID) NODE_BASE_DEF("EFFECT", PluginID)

ENV_DEF(EnvironmentFog)
ENV_DEF(EnvFogMeshGizmo)
ENV_DEF(PhxShaderCache)
ENV_DEF(PhxShaderSim)
ENV_DEF(PhxShaderSimVol)
ENV_DEF(VolumeVRayToon)

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_ENVIRONMENT_DEF_H
