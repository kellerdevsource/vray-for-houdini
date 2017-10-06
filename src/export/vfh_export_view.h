//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORT_VIEW_H
#define VRAY_FOR_HOUDINI_EXPORT_VIEW_H

#include "vfh_vray.h"

#include <OP/OP_Node.h>

namespace VRayForHoudini {

struct RenderSizeParams {
	RenderSizeParams()
		: w(0)
		, h(0)
	{}

	int  w;
	int  h;
};

enum MenuItemSelected {
	physicalCameraUseHoudiniCameraSettings = 0,
	physicalCameraUseFieldOfView = 1,
	physicalCameraUsePhysicalCameraSettings = 2,
};

enum PhysicalCameraType {
	Still = 0,
	Cinematic = 1,
	Video = 2,
};

enum HoudiniFocalUnits {
	Millimeters = 0,
	Meters = 1,
	Nanometers = 2,
	Inches = 3,
	Feet = 4,
};

struct PhysicalCameraParams {
	PhysicalCameraParams()
		: type(Still)
		, useDof(false)
		, useMoBlur(false)
		, selectedItem(physicalCameraUseHoudiniCameraSettings)
		, exposure(true)
		, filmWidth(36.0f)
		, focalLength(50.0f)
		, fov(1.5708f)
		, fNumber(16.0f)
		, shutterSpeed(100.0f)
		, shutterAngle(180.0f)
		, shutterOffset(0.0f)
		, latency(0.0f)
		, ISO(100.0f)
		, zoomFactor(1.0f)
		, specifyFocus(true)
		, focusDistance(200.0f)
		, targeted(1)
		, targetDistance(200.0f)
		, balance(1.0f)
		, vignetting(1.0f)
		, opticalVignetting(0.0f)
		, subdivisions(4)
		, dontAffectSettings(false)
		, focalUnits(Millimeters)
		, houdiniFocalLength(50.0f)
		, houdiniAperture(41.4214f)
		, houdiniFNumber(5.6f)
		, houdiniFocusDistance(5.0f)
	{}

	bool operator == (const PhysicalCameraParams &other) const;

	PhysicalCameraType type;
	bool useDof;
	bool useMoBlur;
	MenuItemSelected selectedItem;
	bool exposure;
	float filmWidth;
	float focalLength;
	float fov;
	float fNumber;
	float shutterSpeed;
	float shutterAngle;
	float shutterOffset;
	float latency;
	float ISO;
	float zoomFactor;
	bool specifyFocus;
	float focusDistance;
	bool targeted;
	float targetDistance;
	VRay::Color balance;
	float vignetting;
	float opticalVignetting;
	int subdivisions;
	bool dontAffectSettings;
	// Houdini Params
	HoudiniFocalUnits focalUnits;
	float houdiniFocalLength;
	float houdiniAperture;
	float houdiniFNumber;
	float houdiniFocusDistance;
};

struct RenderCropRegionParams {
	RenderCropRegionParams()
		: x(0)
		, y(0)
		, width(0)
		, height(0)
	{}

	int x;
	int y;
	int width;
	int height;
};

struct StereoViewParams {
	StereoViewParams()
		: use(false)
		, stereo_eye_distance(6.5f)
		, stereo_interocular_method(0)
		, stereo_specify_focus(0)
		, stereo_focus_distance(200.0f)
		, stereo_focus_method(0)
		, stereo_view(0)
		, adjust_resolution(false)
	{}

	bool operator == (const StereoViewParams &other) const;
	bool operator != (const StereoViewParams &other) const;

	int    use;
	float  stereo_eye_distance;
	int    stereo_interocular_method;
	int    stereo_specify_focus;
	float  stereo_focus_distance;
	int    stereo_focus_method;
	int    stereo_view;
	int    adjust_resolution;
};


struct RenderViewParams {
	RenderViewParams()
		: fov(0.785398f)
		, fovOverride(false)
		, ortho(false)
		, ortho_width(1.0f)
		, use_clip_start(false)
		, clip_start(0.0f)
		, use_clip_end(false)
		, clip_end(1.0f)
	{}

	bool operator == (const RenderViewParams &other) const;
	bool operator != (const RenderViewParams &other) const;

	int              needReset(const RenderViewParams &other) const;

	float            fov;
	int              fovOverride;
	VRay::Transform  tm;

	int              ortho;
	float            ortho_width;

	int              use_clip_start;
	float            clip_start;
	int              use_clip_end;
	float            clip_end;

	StereoViewParams stereoParams;
};


struct ViewParams {
	explicit ViewParams(OBJ_Node *camera=nullptr)
		: usePhysicalCamera(false)
		, cameraObject(camera)
	{}

	int               changedParams(const ViewParams &other) const;
	int               changedSize(const ViewParams &other) const;
	int               needReset(const ViewParams &other) const;
	int               changedCropRegion(const ViewParams &other) const;
	int changedPhysCam(const ViewParams &other) const;

	void setCamera(OBJ_Node *camera) { cameraObject = camera; }

	RenderSizeParams  renderSize;
	RenderViewParams  renderView;
	RenderCropRegionParams cropRegion;
	PhysicalCameraParams physCam;

	int               usePhysicalCamera;
	OBJ_Node         *cameraObject;
};

/// Returns FOV value based on aperture and focal.
/// @param aperture Aperture.
/// @param focal Focal.
float getFov(float aperture, float focal);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_VIEW_H
