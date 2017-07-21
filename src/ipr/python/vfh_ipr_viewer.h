//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini Python IPR Module
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_IRP_VIEWER_H
#define VRAY_FOR_HOUDINI_IRP_VIEWER_H

#include "vfh_vray.h"

namespace VRayForHoudini {

void initImdisplay(VRay::VRayRenderer &renderer);
void setImdisplayPort(int port);
int getImdisplayPort();
void closeImdisplay();

void onRTImageUpdated(VRay::VRayRenderer &renderer, VRay::VRayImage *image, void *userData);
void onImageReady(VRay::VRayRenderer &renderer, void *userData);
void onBucketReady(VRay::VRayRenderer &renderer, int x, int y, const char* host, VRay::VRayImage *img, void *userData);
void onRenderStart(VRay::VRayRenderer &renderer, void *userData);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_IRP_VIEWER_H
