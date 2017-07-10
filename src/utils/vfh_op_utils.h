//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//   https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VFH_OP_UTILS_H
#define VRAY_FOR_HOUDINI_VFH_OP_UTILS_H

#include <OP/OP_Node.h>

namespace VRayForHoudini {

/// Returns node of type "pluginID" connected tothe socket "socketName" if matNode is a SHOP node.
/// Otherwize checks type "pluginID" of the matNode.
/// @param matNode SHOP or VOP node.
/// @param socketName Socket name for "V-Ray Material Output" node.
/// @param pluginID V-Ray plugin ID to match.
OP_Node *getVRayNodeFromOp(OP_Node &matNode, const char *socketName, const char *pluginID=nullptr);

/// Get the internal budle holding active lights that should be exported to V-Ray.
/// It will take into account parameters on the V-Ray ROP Object tab
OP_Bundle* getActiveLightsBundle(OP_Node &rop, fpreal t);

/// Get the internal budle holding forced lights set on the V-Ray ROP Object tab
OP_Bundle* getForcedLightsBundle(OP_Node &rop, fpreal t);

/// Get the internal budle holding active geometry nodes that should be exported to V-Ray.
/// It will take into account parameters on the V-Ray ROP Object tab
OP_Bundle* getActiveGeometryBundle(OP_Node &rop, fpreal t);

/// Get the internal budle holding forced geometry set on the V-Ray ROP Object tab
OP_Bundle* getForcedGeometryBundle(OP_Node &rop, fpreal t);

/// Get the internal budle holding matte geometry set on the V-Ray ROP Object tab
OP_Bundle* getMatteGeometryBundle(OP_Node &rop, fpreal t);

/// Get the internal budle holding phantom geometry set on the V-Ray ROP Object tab
OP_Bundle* getPhantomGeometryBundle(OP_Node &rop, fpreal t);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VFH_OP_UTILS_H
