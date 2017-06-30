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
OP_Node *getVRayNodeFromOp(OP_Node &matNode, const char *socketName, const char *pluginID);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VFH_OP_UTILS_H
