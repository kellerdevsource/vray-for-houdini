//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_BRDFToonMtl.h"

using namespace VRayForHoudini;

void BRDFToonMtl::setPluginType()
{
	pluginType = VRayPluginType::BRDF;
	pluginID = "BRDFToonMtl";
}

OP::VRayNode::PluginResult BRDFToonMtl::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext*)
{
	return BRDFVRayMtl::asPluginDesc(pluginDesc, exporter);
}
