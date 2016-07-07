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
