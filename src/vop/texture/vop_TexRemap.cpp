//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_TexRemap.h"
#include "vfh_tex_utils.h"

using namespace VRayForHoudini;

void VOP::TexRemap::setPluginType()
{
	pluginType = VRayPluginType::TEXTURE;
	pluginID = SL("TexRemap");
}

OP::VRayNode::PluginResult VOP::TexRemap::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter)
{
	return PluginResultContinue;
}
