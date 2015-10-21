//
// Copyright (c) 2015, Chaos Software Ltd
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

	float            fov;
	int              fovOverride;
	VRay::Transform  tm;

	int              ortho;
	float            ortho_width;

	int              use_clip_start;
	float            clip_start;
	int              use_clip_end;
	float            clip_end;
};


struct ViewPluginsDesc {
	static const std::string settingsCameraDofPluginName;
	static const std::string settingsCameraPluginName;
	static const std::string cameraPhysicalPluginName;
	static const std::string cameraDefaultPluginName;
	static const std::string renderViewPluginName;

	ViewPluginsDesc()
		: settingsCameraDof(settingsCameraDofPluginName, "SettingsCameraDof")
		, settingsCamera(settingsCameraPluginName, "SettingsCamera")
		, cameraPhysical(cameraPhysicalPluginName, "CameraPhysical")
		, cameraDefault(cameraDefaultPluginName, "CameraDefault")
		, renderView(renderViewPluginName, "RenderView")
	{}

	int                needReset(const ViewPluginsDesc &other) const;

	Attrs::PluginDesc  settingsCameraDof;
	Attrs::PluginDesc  settingsCamera;
	Attrs::PluginDesc  cameraPhysical;
	Attrs::PluginDesc  cameraDefault;
	Attrs::PluginDesc  renderView;
};


struct ViewParams {
	ViewParams() {}
	ViewParams(OBJ_Node *camera)
		: usePhysicalCamera(false)
		, cameraObject(camera)
	{}

	int               changedParams(const ViewParams &other) const;
	int               changedSize(const ViewParams &other) const;
	int               needReset(const ViewParams &other) const;

	RenderSizeParams  renderSize;
	RenderViewParams  renderView;
	ViewPluginsDesc   viewPlugins;

	int               usePhysicalCamera;
	OBJ_Node         *cameraObject;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_VIEW_H
