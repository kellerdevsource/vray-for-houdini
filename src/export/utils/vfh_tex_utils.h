//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_TEXTURE_UTILS_H
#define VRAY_FOR_HOUDINI_TEXTURE_UTILS_H

#include "vop_node_base.h"


namespace VRayForHoudini {
namespace Texture {

enum class HOU_InterpolationType {
	Constant,
	Linear,
	CatmullRom,
	MonotoneCubic,
	Bezier,
	BSpline,
	Hermite,
};

enum class VRAY_InterpolationType {
	None,
	Linear,
	Smooth,
	Spline,
	Bezier,
	LogBezier,
};

inline VRAY_InterpolationType mapToVray(HOU_InterpolationType type) {
	switch(type) {
	case HOU_InterpolationType::Constant:      return VRAY_InterpolationType::None;
	case HOU_InterpolationType::Linear:        return VRAY_InterpolationType::Linear;
	case HOU_InterpolationType::CatmullRom:    return VRAY_InterpolationType::Smooth;
	case HOU_InterpolationType::MonotoneCubic: return VRAY_InterpolationType::Smooth;
	case HOU_InterpolationType::Bezier:        return VRAY_InterpolationType::Bezier;
	case HOU_InterpolationType::BSpline:       return VRAY_InterpolationType::Spline;
	case HOU_InterpolationType::Hermite:       return VRAY_InterpolationType::Spline;
	default:                                   return VRAY_InterpolationType::None;
	}
}

void exportRampAttribute(VRayExporter &exporter, Attrs::PluginDesc &pluginDesc, OP_Node *op_node,
						 const std::string &rampAttrName,
						 const std::string &colAttrName, const std::string &posAttrName, const std::string &typesAttrName="",
						 const bool asColor=false, const bool remapInterp=false);

void getCurveData(VRayExporter &exporter, OP_Node *node,
				  const std::string &curveAttrName,
				  VRay::IntList &interpolations, VRay::FloatList &positions, VRay::FloatList *values=nullptr,
				  const bool needHandles=false, const bool remapInterp=false);

} // namespace Texture
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_TEXTURE_UTILS_H
