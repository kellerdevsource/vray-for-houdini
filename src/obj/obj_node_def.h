//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_OBJ_NODE_DEF_H
#define VRAY_FOR_HOUDINI_OBJ_NODE_DEF_H


#include "obj_node_base.h"

namespace VRayForHoudini {
namespace OBJ {

typedef LightNodeBase< VRayPluginID::SunLight > SunLight;
typedef LightNodeBase< VRayPluginID::LightDirect > LightDirect;
typedef LightNodeBase< VRayPluginID::LightAmbient > LightAmbient;
typedef LightNodeBase< VRayPluginID::LightOmni > LightOmni;
typedef LightNodeBase< VRayPluginID::LightSphere > LightSphere;
typedef LightNodeBase< VRayPluginID::LightSpot > LightSpot;
typedef LightNodeBase< VRayPluginID::LightRectangle > LightRectangle;
typedef LightNodeBase< VRayPluginID::LightMesh > LightMesh;
typedef LightNodeBase< VRayPluginID::LightIES > LightIES;
typedef LightNodeBase< VRayPluginID::LightDome > LightDome;

} // namespace OBJ
} // namespace VRayForHoudini



#endif // VRAY_FOR_HOUDINI_OBJ_NODE_DEF_H
