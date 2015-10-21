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
	    , tm(VRay::Matrix(1.0f), VRay::Vector(0.0f))
	    , ortho(false)
	    , ortho_width(1.0f)
	    , use_clip_start(false)
	    , clip_start(0.0f)
	    , use_clip_end(false)
	    , clip_end(1.0f)
	{}

	bool operator == (const RenderViewParams &other) const {
		return (MemberEq(fov) &&
		        MemberEq(ortho) &&
		        MemberEq(ortho_width) &&
		        MemberEq(use_clip_start) &&
		        MemberEq(clip_start) &&
		        MemberEq(use_clip_end) &&
		        MemberEq(clip_end) &&
		        (tm.matrix == other.tm.matrix && tm.offset == other.tm.offset));
	}

	bool operator != (const RenderViewParams &other) const {
		return !(*this == other);
	}

	float            fov;
	VRay::Transform  tm;

	int              ortho;
	float            ortho_width;

	int              use_clip_start;
	float            clip_start;
	int              use_clip_end;
	float            clip_end;
};


struct ViewParams {
	static const std::string renderViewPluginName;
	static const std::string physicalCameraPluginName;
	static const std::string defaultCameraPluginName;
	static const std::string settingsCameraDofPluginName;

	ViewParams()
	    : usePhysicalCamera(false)
	    , cameraObject(nullptr)
	{}

	int changedParams(const ViewParams &other) const {
		return MemberNotEq(renderView);
	}

	int changedSize(const ViewParams &other) const {
		return (MemberNotEq(renderSize.w) ||
		        MemberNotEq(renderSize.h));
	}

	int needReset(ViewParams &other) const {
		return (MemberNotEq(usePhysicalCamera) ||
		        MemberNotEq(renderView.ortho) ||
		        MemberNotEq(cameraObject));
	}

	RenderSizeParams  renderSize;
	RenderViewParams  renderView;

	int               usePhysicalCamera;
	OP_Node          *cameraObject;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_VIEW_H
