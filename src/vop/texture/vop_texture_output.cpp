//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_texture_output.h"


using namespace VRayForHoudini;


void VOP::TextureOutput::addPrmTemplate(Parm::PRMList &prmTemplate)
{
}


void VOP::TextureOutput::setPluginType()
{
	pluginType = VRayPluginType::CUSTOM_TEXTURE;
	pluginID   = "CustomTextureOutput";
}


OP::VRayNode::PluginResult VOP::TextureOutput::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	return OP::VRayNode::PluginResultSuccess;
}
