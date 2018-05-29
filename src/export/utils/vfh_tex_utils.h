//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_TEXTURE_UTILS_H
#define VRAY_FOR_HOUDINI_TEXTURE_UTILS_H

#include "vfh_sys_utils.h"
#include "vop_node_base.h"

namespace VRayForHoudini {
namespace Texture {

/// Houdini ramp interpolation types
enum class HOU_InterpolationType {
	Constant,
	Linear,
	CatmullRom,
	MonotoneCubic,
	Bezier,
	BSpline,
	Hermite,
};

/// V-Ray ramp interpolation types
enum class VRAY_InterpolationType {
	None,
	Linear,
	Smooth,
	Spline,
	Bezier,
	LogBezier,
};

/// Map Houdini ramp interpolation type to the closest one supported by V-Ray
/// @param type[in] - Houdini ramp interpolation type
/// @retval V-Ray ramp interpolation type
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

/// Helpper function to translate a color ramp parameter to a set of
/// plugin attributes
/// @param exporter[in] - reference to the main vfh exporter
/// @param pluginDesc[out] - plugin description - the corresponding
///        ramp attributes will be added to it
/// @param opNode[in] - the node owning the ramp parameter
/// @param rampAttrName[in] - ramp parameter name
/// @param colAttrName[in] - plugin attribute name for ramp color list
/// @param posAttrName[in] - plugin attribute name for ramp intarpolation
///        intervals
/// @param typesAttrName[in] - plugin attribute name for ramp intarpolation
///        type per interval
/// @param asColor[in] - flags whether the color list should contain raw color data
///        or colors should be wrapped in texture plugins
/// @param remapInterp[in] - flags if interpolation type should be remapped to the
///        closest one supported by V-Ray
void exportRampAttribute(VRayExporter &exporter,
                         Attrs::PluginDesc &pluginDesc,
                         OP_Node &opNode,
                         const QString &rampAttrName,
                         const QString &colAttrName,
                         const QString &posAttrName,
                         const QString &typesAttrName = "",
                         bool asColor = false,
                         bool remapInterp = false);

enum CurveDataFlags {
	curveDataFlagsNone = 0,

	/// Export values into the @a values array.
	/// If not set values will be expoported into @a positions array.
	curveDataFlagsNeedValues = VFH_BIT(0),

	/// Export additional Besier points handles.
	curveDataFlagsNeedHandles = VFH_BIT(1),
};

/// Helpper function to obtain data from a curve ramp parameter
/// @param exporter reference to the main vfh exporter.
/// @param opNode the node owning the ramp parameter.
/// @param curveAttrName ramp parameter name.
/// @param interpolations output for the ramp intarpolation types
///        per interval.
/// @param positions output for the ramp interpolation intervals.
/// @param values this will hold the ouput of
///        curve control points, otherwise those would be added to positions
///        parameter so that every 2 sequential values form a tuple (position, value).
/// @param isAnimated This will be set to 1 if any of the values is animated.
/// @param flags Data export flags. Refer to CurveDataFlags enum.
void getCurveData(VRayExporter &exporter,
                  OP_Node &opNode,
                  const QString &curveAttrName,
                  VRay::VUtils::IntRefList &interpolations,
                  VRay::VUtils::FloatRefList &positions,
                  VRay::VUtils::FloatRefList &values,
                  int &isAnimated,
                  int flags = curveDataFlagsNeedValues);

} // namespace Texture
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_TEXTURE_UTILS_H
