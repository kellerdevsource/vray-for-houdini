//
// Copyright (c) 2015-2018, Chaos Software Ltd
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
	pluginID   = SL("TexFalloff");
}

OP::VRayNode::PluginResult VOP::TexFalloff::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext*)
{
	if (!evalInt("use_blend_curve", 0, 0.0))
		return PluginResultContinue;

	Attrs::PluginDesc subTexFalloffDesc(VRayExporter::getPluginName(*this, SL("SubFalloff")),
		                                SL("TexFalloff"));
	subTexFalloffDesc.add(Attrs::PluginAttr(SL("use_blend_input"), false));
	subTexFalloffDesc.add(Attrs::PluginAttr(SL("blend_input"), VRay::Plugin()));

	const VRay::Plugin subFalloffTex = exporter.exportPlugin(subTexFalloffDesc);

	VRay::FloatList points;
	VRay::IntList   types;
	Texture::getCurveData(exporter, this, SL("curve"), types, points, nullptr, true);

	Attrs::PluginDesc texBezierCurveDesc(VRayExporter::getPluginName(*this, SL("SubCurve")),
		                                    SL("TexBezierCurve"));
	texBezierCurveDesc.add(Attrs::PluginAttr(SL("input_float"), subFalloffTex, "blend_output"));
	texBezierCurveDesc.add(Attrs::PluginAttr(SL("points"), points));
	texBezierCurveDesc.add(Attrs::PluginAttr(SL("types"), types));

	const VRay::Plugin texBezierCurve = exporter.exportPlugin(texBezierCurveDesc);

	pluginDesc.add(Attrs::PluginAttr(SL("use_blend_input"), true));
	pluginDesc.add(Attrs::PluginAttr(SL("blend_input"), texBezierCurve));

	return PluginResultContinue;
}
