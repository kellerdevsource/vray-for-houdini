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

namespace VRayForHoudini {

struct RenderSizeParams {
	RenderSizeParams()
		: w(0)
		, h(0)
	{}

	int w;
	int h;
};

enum class CameraFovMode {
	/// Use Houdini camera settings.
	useHoudini = 0,

	/// Use Physical camera native settings.
	usePhysical,

	/// FOV override.
	useFovOverride,
};

enum class PhysicalCameraMode {
	/// Non physical camera.
	modeNone = 0,

	/// User have added physical camera properties.
	modeUser,

	/// Some Houdini camera properties could be implemented only
	/// utilizing physical camera plugin.
	modeAuto,
};

enum class PhysicalCameraType {
	typeStill = 0,
	typeCinematic,
	typeVideo,
};

enum class HoudiniFocalUnits {
	millimeters = 0,
	meters = 1,
	nanometers = 2,
	inches = 3,
	feet = 4,
};

struct PhysicalCameraParams {
	CameraFovMode fovMode = CameraFovMode::useHoudini;
	HoudiniFocalUnits focalUnits = HoudiniFocalUnits::millimeters;

	PhysicalCameraType type = PhysicalCameraType::typeStill;
	float film_width = 36.0f;
	float focal_length = 40.0f;
	float zoom_factor = 1.0f;
	float distortion = 0.0f;
	int distortion_type = 0;
	float f_number = 8.0f;
	float lens_shift = 0.0f;
	float shutter_speed = 300.0f;
	float shutter_angle = 180.0f;
	float shutter_offset = 0.0f;
	float latency = 0.0f;
	float ISO = 200.0f;
	int specify_focus = true;
	float focus_distance = 200.0f;
	int targeted = false; ///< If camera object has a target. Unused. Set to "False"
	float target_distance = 200.0f; ///< Camera object target distance. Unused.
	float dof_display_threshold = 0.001f;
	int exposure = true;
	VRay::Color white_balance = VRay::Color(1.0f, 1.0f, 1.0f);
	float vignetting = 1.0f;
	int blades_enable = false;
	int blades_num = 5;
	float blades_rotation = 0.0f;
	float center_bias = 0.0f;
	float anisotropy = 0.0f;
	int use_dof = false;
	int use_moblur = false;
	int subdivs = 1;
	int dont_affect_settings = false;
	UT_String lens_file = "";
	int specify_fov = false;
	float fov = 1.5708f;
	float horizontal_shift = 0.0f;
	float horizontal_offset = 0.0f;
	float vertical_offset = 0.0f;
	UT_String distortion_tex = "";
	int bmpaperture_enable = false;
	int bmpaperture_resolution = 512;
	UT_String bmpaperture_tex = "";
	float optical_vignetting = 0.0f;
	int bmpaperture_affects_exposure = true;
	int enable_thin_lens_equation = true;

	bool operator == (const PhysicalCameraParams &other) const;
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

	int use;
	float stereo_eye_distance;
	int stereo_interocular_method;
	int stereo_specify_focus;
	float stereo_focus_distance;
	int stereo_focus_method;
	int stereo_view;
	int adjust_resolution;
};

struct RenderViewParams {
	RenderViewParams()
		: fov(0.785398f)
		, ortho(false)
		, ortho_width(1.0f)
		, use_clip_start(true)
		, clip_start(0.0f)
		, use_clip_end(true)
		, clip_end(1.0f)
	{}

	bool operator == (const RenderViewParams &other) const;
	bool operator != (const RenderViewParams &other) const;

	int needReset(const RenderViewParams &other) const;

	float fov;
	VRay::Transform tm;
	int ortho;
	float ortho_width;
	int use_clip_start;
	float clip_start;
	int use_clip_end;
	float clip_end;

	StereoViewParams stereoParams;
};

struct ViewParams {
	ViewParams()
		: useCameraPhysical(PhysicalCameraMode::modeNone)
	{}

	int needReset(const ViewParams &other) const;

	int changedParams(const ViewParams &other) const;
	int changedSize(const ViewParams &other) const;
	int changedCropRegion(const ViewParams &other) const;
	int changedPhysCam(const ViewParams &other) const;

	RenderSizeParams renderSize;
	RenderViewParams renderView;
	RenderCropRegionParams cropRegion;

	PhysicalCameraMode useCameraPhysical;
	PhysicalCameraParams cameraPhysical;
};

/// Returns FOV value based on aperture and focal.
/// @param aperture Aperture.
/// @param focal Focal.
float getFov(float aperture, float focal);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_VIEW_H
