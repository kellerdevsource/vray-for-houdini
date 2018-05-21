//
// Copyright (c) 2015-2018, Chaos Software Ltd
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
#include "vfh_ipr_imdisplay_viewer.h"

namespace VRayForHoudini {

/// Returns image writer thread instance.
ImdisplayThread &getImdisplay();

/// Initializes writer thread instance.
void initImdisplay(VRay::VRayRenderer &renderer, const char *ropName);

void onProgressiveImageUpdated(VRay::VRayRenderer&, VRay::VRayImage* img, unsigned long long changeIndex, VRay::ImagePassType pass, double instant, void *userData);
void onStateChanged(VRay::VRayRenderer &renderer, VRay::RendererState oldState, VRay::RendererState newState, double instant, void *userData);
void onBucketReady(VRay::VRayRenderer &renderer, int x, int y, const char* host, VRay::VRayImage *image, VRay::ImagePassType pass, double instant, void *userData);
void onProgress(VRay::VRayRenderer &renderer, const char *msg, int elementNumber, int elementsCount, double instant, void *data);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_IRP_VIEWER_H
