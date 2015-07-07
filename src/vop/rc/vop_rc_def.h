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

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_RENDERCHANNEL_DEF_H
#define VRAY_FOR_HOUDINI_VOP_NODE_RENDERCHANNEL_DEF_H

#include "vop_node_base.h"
#include "vop_rc_container.h"
#include "vop_rc_context.h"


namespace VRayForHoudini {
namespace VOP {

#define RC_DEF(PluginID) NODE_BASE_DEF("RENDERCHANNEL", PluginID)

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

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_RENDERCHANNEL_DEF_H
