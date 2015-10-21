//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_texture_output.h"


using namespace VRayForHoudini;


void VOP::TextureOutput::addPrmTemplate(Parm::PRMTmplList &prmTemplate)
{
}


void VOP::TextureOutput::setPluginType()
{
	pluginType = "CUSTOM_TEXTURE";
	pluginID   = "CustomTextureOutput";
}


OP::VRayNode::PluginResult VOP::TextureOutput::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	return OP::VRayNode::PluginResultSuccess;
}
