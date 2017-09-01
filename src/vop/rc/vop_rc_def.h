//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_RENDERCHANNEL_DEF_H
#define VRAY_FOR_HOUDINI_VOP_NODE_RENDERCHANNEL_DEF_H

#include "vop_node_base.h"
#include "vop_rc_container.h"


namespace VRayForHoudini {
namespace VOP {

#define RC_DEF(PluginID) NODE_BASE_DEF(RENDERCHANNEL, PluginID)

RC_DEF(RenderChannelBumpNormals)
RC_DEF(RenderChannelColor)
RC_DEF(RenderChannelCoverage)
RC_DEF(RenderChannelDRBucket)
RC_DEF(RenderChannelExtraTex)
RC_DEF(RenderChannelGlossiness)
RC_DEF(RenderChannelNodeID)
RC_DEF(RenderChannelNormals)
RC_DEF(RenderChannelRenderID)
RC_DEF(RenderChannelVelocity)
RC_DEF(RenderChannelZDepth)
RC_DEF(RenderChannelDenoiser)
RC_DEF(RenderChannelMultiMatte)
RC_DEF(RenderChannelCryptomatte)

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_RENDERCHANNEL_DEF_H
