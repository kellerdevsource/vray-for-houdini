//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//  https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VFH_GPU_DEVICE_SELECT_H
#define VRAY_FOR_HOUDINI_VFH_GPU_DEVICE_SELECT_H

namespace VRayForHoudini {

typedef std::vector<int> ComputeDeviceIdList;

void getGpuDeviceIdList(int isCUDA, ComputeDeviceIdList &list);

void showGpuDeviceSelectUI();

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VFH_GPU_DEVICE_SELECT_H
