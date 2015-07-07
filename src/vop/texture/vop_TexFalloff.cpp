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

#include "vop_TexFalloff.h"
#include "vfh_tex_utils.h"


using namespace VRayForHoudini;


void VOP::TexFalloff::setPluginType()
{
	pluginType = "TEXTURE";
	pluginID   = "TexFalloff";
}


OP::VRayNode::PluginResult VOP::TexFalloff::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	if (evalInt("use_blend_curve", 0, 0.0)) {
		Attrs::PluginDesc subTexFalloffDesc(this, "TexFalloff", "SubFalloff");
		subTexFalloffDesc.pluginAttrs.push_back(Attrs::PluginAttr("use_blend_input", false));
		subTexFalloffDesc.pluginAttrs.push_back(Attrs::PluginAttr("blend_input", VRay::Plugin()));

		VRay::Plugin subFalloffTex = exporter->exportPlugin(subTexFalloffDesc);

		VRay::FloatList points;
		VRay::IntList   types;
		Texture::getCurveData(exporter, this, "curve", types, points, nullptr, true);

		Attrs::PluginDesc texBezierCurveDesc(this, "TexBezierCurve", "SubCurve");
		texBezierCurveDesc.pluginAttrs.push_back(Attrs::PluginAttr("input_float", subFalloffTex, "blend_output"));
		texBezierCurveDesc.pluginAttrs.push_back(Attrs::PluginAttr("points", points));
		texBezierCurveDesc.pluginAttrs.push_back(Attrs::PluginAttr("types", types));

		VRay::Plugin texBezierCurve = exporter->exportPlugin(texBezierCurveDesc);

		pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("use_blend_input", true));
		pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("blend_input", texBezierCurve));
	}

	return OP::VRayNode::PluginResultContinue;
}
