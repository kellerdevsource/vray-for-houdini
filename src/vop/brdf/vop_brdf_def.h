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

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_BRDF_DEF_H
#define VRAY_FOR_HOUDINI_VOP_NODE_BRDF_DEF_H

#include "vop_node_base.h"
#include "vop_BRDFLayered.h"


namespace VRayForHoudini {
namespace VOP {

#define BRDF_DEF(PluginID) NODE_BASE_DEF("BRDF", PluginID)

BRDF_DEF(BRDFBlinn)
BRDF_DEF(BRDFBump)
BRDF_DEF(BRDFCSV)
BRDF_DEF(BRDFCarPaint)
BRDF_DEF(BRDFCookTorrance)
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

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_BRDF_DEF_H
