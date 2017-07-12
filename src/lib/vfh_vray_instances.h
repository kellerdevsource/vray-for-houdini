//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//  https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//


#ifndef VRAY_FOR_HOUDINI_VFH_VRAY_INSTANCES_H
#define VRAY_FOR_HOUDINI_VFH_VRAY_INSTANCES_H

#include <vraysdk.hpp>

namespace VRayForHoudini {

int newVRayInit();
void deleteVRayInit();

VRay::VRayRenderer* newVRayRenderer(const VRay::RendererOptions &options);
void deleteVRayRenderer(VRay::VRayRenderer* &instance);

void dumpSharedMemory();

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VFH_VRAY_INSTANCES_H
