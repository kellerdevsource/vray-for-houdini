//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_MTL_DEF_H
#define VRAY_FOR_HOUDINI_VOP_NODE_MTL_DEF_H

#include "vop_MtlMulti.h"
#include "vop_MaterialOutput.h"
#include "vop_PhoenixSim.h"

namespace VRayForHoudini {
namespace VOP {

#define MTL_DEF(PluginID) NODE_BASE_DEF(MATERIAL, PluginID)

// TODO:
//   [ ] MtlMDL
//   [ ] MtlGLSL

MTL_DEF(Mtl2Sided)
MTL_DEF(MtlLayeredBRDF)
MTL_DEF(MtlMaterialID)
MTL_DEF(MtlOverride)
MTL_DEF(MtlRenderStats)
MTL_DEF(MtlRoundEdges)
MTL_DEF(MtlSingleBRDF)
MTL_DEF(MtlVRmat)
MTL_DEF(MtlWrapper)

// XXX: Move into a separate file.
MTL_DEF(GeomDisplacedMesh)
MTL_DEF(GeomStaticSmoothedMesh)

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_MTL_DEF_H
