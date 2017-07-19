//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORT_VIEW_H
#define VRAY_FOR_HOUDINI_EXPORT_VIEW_H

#include "vfh_defines.h"
#include "vfh_vray.h"
#include "vfh_plugin_attrs.h"

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


struct ViewPluginsDesc {
	static const std::string settingsCameraDofPluginName;
	static const std::string settingsMotionBlurPluginName;
	static const std::string settingsCameraPluginName;
	static const std::string cameraPhysicalPluginName;
	static const std::string cameraDefaultPluginName;
	static const std::string renderViewPluginName;
	static const std::string stereoSettingsPluginName;

	ViewPluginsDesc()
		: settingsCameraDof(settingsCameraDofPluginName, "SettingsCameraDof")
		, settingsMotionBlur(settingsMotionBlurPluginName, "SettingsMotionBlur")
		, settingsCamera(settingsCameraPluginName, "SettingsCamera")
		, cameraPhysical(cameraPhysicalPluginName, "CameraPhysical")
		, cameraDefault(cameraDefaultPluginName, "CameraDefault")
		, renderView(renderViewPluginName, "RenderView")
		, stereoSettings(stereoSettingsPluginName, "VRayStereoscopicSettings")
	{}

	int                needReset(const ViewPluginsDesc &other) const;

	Attrs::PluginDesc  settingsCameraDof;
	Attrs::PluginDesc  settingsMotionBlur;
	Attrs::PluginDesc  settingsCamera;
	Attrs::PluginDesc  cameraPhysical;
	Attrs::PluginDesc  cameraDefault;
	Attrs::PluginDesc  renderView;
	Attrs::PluginDesc  stereoSettings;
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

	RenderSizeParams  renderSize;
	RenderViewParams  renderView;
	RenderCropRegionParams cropRegion;

	int               usePhysicalCamera;
	OBJ_Node         *cameraObject;
};

/// Returns FOV value based on aperture and focal.
/// @param aperture Aperture.
/// @param focal Focal.
float getFov(float aperture, float focal);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_VIEW_H
