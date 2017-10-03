//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
/// @param op_node[in] - the node owning the ramp parameter
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
void exportRampAttribute(VRayExporter &exporter, Attrs::PluginDesc &pluginDesc, OP_Node *op_node,
						 const std::string &rampAttrName,
						 const std::string &colAttrName, const std::string &posAttrName, const std::string &typesAttrName="",
						 const bool asColor=false, const bool remapInterp=false);

/// Helpper function to obtain data from a curve ramp parameter
/// @param exporter[in] - reference to the main vfh exporter
/// @param node[in] - the node owning the ramp parameter
/// @param curveAttrName[in] - ramp parameter name
/// @param interpolations[out] - output for the ramp intarpolation types
///        per interval
/// @param positions[out] - output for the ramp interpolation intervals
/// @param values[out] - if a valid pointer, this will hold the ouput of
///        curve control points, otherwise those would be added to positions
///        parameter so that every 2 sequential values form a tuple (position, value)
/// @param needHandles[in] - flags whether positions and values need to be transformed
///        // TODO: need more info here
/// @param remapInterp[in] - flags if interpolation type should be remapped to the
///        closest one supported by V-Ray
void getCurveData(VRayExporter &exporter, OP_Node *node,
				  const std::string &curveAttrName,
				  VRay::IntList &interpolations, VRay::FloatList &positions, VRay::FloatList *values=nullptr,
				  const bool needHandles=false, const bool remapInterp=false);

} // namespace Texture
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_TEXTURE_UTILS_H
