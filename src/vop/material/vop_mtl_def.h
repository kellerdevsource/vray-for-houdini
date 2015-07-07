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

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_MTL_DEF_H
#define VRAY_FOR_HOUDINI_VOP_NODE_MTL_DEF_H

#include "vop_node_base.h"
#include "vop_MtlMulti.h"


namespace VRayForHoudini {
namespace VOP {

#define MTL_DEF(PluginID) NODE_BASE_DEF("MATERIAL", PluginID)

MTL_DEF(Mtl2Sided)
MTL_DEF(MtlBump)
MTL_DEF(MtlDiffuse)
MTL_DEF(MtlDoubleSided)
MTL_DEF(MtlLayeredBRDF)
MTL_DEF(MtlMaterialID)
MTL_DEF(MtlMayaRamp)
MTL_DEF(MtlObjBBox)
MTL_DEF(MtlOverride)
MTL_DEF(MtlRenderStats)
MTL_DEF(MtlRoundEdges)
MTL_DEF(MtlSingleBRDF)
MTL_DEF(MtlStreakFade)
MTL_DEF(MtlVRmat)
MTL_DEF(MtlWrapper)
MTL_DEF(MtlWrapperMaya)

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_MTL_DEF_H
