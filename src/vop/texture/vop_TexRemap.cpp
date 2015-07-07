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

#include "vop_TexRemap.h"
#include "vfh_tex_utils.h"


using namespace VRayForHoudini;


void VOP::TexRemap::setPluginType()
{
	pluginType = "TEXTURE";
	pluginID   = "TexRemap";
}


OP::VRayNode::PluginResult VOP::TexRemap::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	Texture::exportRampAttribute(exporter, pluginDesc, this,
								 "ramp",
								 "color_colors", "color_positions", "color_types");

	return OP::VRayNode::PluginResultContinue;
}
