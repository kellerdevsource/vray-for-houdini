//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
//

#ifndef VRAY_FOR_HOUDINI_TEXTURE_UTILS_H
#define VRAY_FOR_HOUDINI_TEXTURE_UTILS_H

#include "vop_node_base.h"


namespace VRayForHoudini {
namespace Texture {

void exportRampAttribute(VRayExporter *exporter, Attrs::PluginDesc &pluginDesc, OP_Node *op_node,
						 const std::string &rampAttrName,
						 const std::string &colAttrName, const std::string &posAttrName, const std::string &typesAttrName="",
						 const bool asColor=false);

void getCurveData(VRayExporter *exporter, OP_Node *node,
				  const std::string &curveAttrName,
				  VRay::IntList &interpolations, VRay::FloatList &positions, VRay::FloatList *values=nullptr,
				  const bool needHandles=false);

} // namespace Texture
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_TEXTURE_UTILS_H
