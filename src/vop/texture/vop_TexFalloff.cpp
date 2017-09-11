//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_TexFalloff.h"
#include "vfh_tex_utils.h"


using namespace VRayForHoudini;


void VOP::TexFalloff::setPluginType()
{
	pluginType = VRayPluginType::TEXTURE;
	pluginID   = "TexFalloff";
}


OP::VRayNode::PluginResult VOP::TexFalloff::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	if (evalInt("use_blend_curve", 0, 0.0)) {
		Attrs::PluginDesc subTexFalloffDesc(VRayExporter::getPluginName(this, "SubFalloff"), "TexFalloff");
		subTexFalloffDesc.add(Attrs::PluginAttr("use_blend_input", false));
		subTexFalloffDesc.add(Attrs::PluginAttr("blend_input", VRay::Plugin()));

		VRay::Plugin subFalloffTex = exporter.exportPlugin(subTexFalloffDesc);

		VRay::FloatList points;
		VRay::IntList   types;
		Texture::getCurveData(exporter, this, "curve", types, points, nullptr, true);

		Attrs::PluginDesc texBezierCurveDesc(VRayExporter::getPluginName(this, "SubCurve"), "TexBezierCurve");
		texBezierCurveDesc.add(Attrs::PluginAttr("input_float", subFalloffTex, "blend_output"));
		texBezierCurveDesc.add(Attrs::PluginAttr("points", points));
		texBezierCurveDesc.add(Attrs::PluginAttr("types", types));

		VRay::Plugin texBezierCurve = exporter.exportPlugin(texBezierCurveDesc);

		pluginDesc.add(Attrs::PluginAttr("use_blend_input", true));
		pluginDesc.add(Attrs::PluginAttr("blend_input", texBezierCurve));
	}

	return OP::VRayNode::PluginResultContinue;
}
