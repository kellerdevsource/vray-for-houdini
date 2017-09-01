//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_BRDF_DEF_H
#define VRAY_FOR_HOUDINI_VOP_NODE_BRDF_DEF_H

#include "vop_node_base.h"
#include "vop_BRDFLayered.h"
#include "vop_BRDFScanned.h"
#include "vop_BRDFVRayMtl.h"

namespace VRayForHoudini {
namespace VOP {

#define BRDF_DEF(PluginID) NODE_BASE_DEF(BRDF, PluginID)

BRDF_DEF(BRDFBlinn)
BRDF_DEF(BRDFBump)
BRDF_DEF(BRDFCSV)
BRDF_DEF(BRDFCarPaint)
BRDF_DEF(BRDFCookTorrance)
BRDF_DEF(BRDFDiffuse)
BRDF_DEF(BRDFDiffuse_forSSS)
BRDF_DEF(BRDFFlakes)
BRDF_DEF(BRDFGGX)
BRDF_DEF(BRDFGlass)
BRDF_DEF(BRDFGlassGlossy)
BRDF_DEF(BRDFHOPS)
BRDF_DEF(BRDFHair)
BRDF_DEF(BRDFHair2)
BRDF_DEF(BRDFHair3)
BRDF_DEF(BRDFLight)
BRDF_DEF(BRDFMirror)
BRDF_DEF(BRDFMultiBump)
BRDF_DEF(BRDFPhong)
BRDF_DEF(BRDFSSS)
BRDF_DEF(BRDFSSS2)
BRDF_DEF(BRDFSSS2Complex)
BRDF_DEF(BRDFSampled)
BRDF_DEF(BRDFSkinComplex)
BRDF_DEF(BRDFWard)
BRDF_DEF(BRDFAlSurface)
BRDF_DEF(BRDFAlHair)

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_BRDF_DEF_H
