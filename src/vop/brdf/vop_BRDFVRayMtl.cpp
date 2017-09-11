//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_BRDFVRayMtl.h"

using namespace VRayForHoudini;

OP::VRayNode::PluginResult BRDFVRayMtl::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	const fpreal &t = exporter.getContext().getTime();

	const int hilightLockVal = evalInt("hilight_glossiness_lock", 0, t);
	const fpreal hilightGlossinessValue = evalFloat("hilight_glossiness", 0, t);
	const fpreal reflectionColourValue = evalFloat("reflect_glossiness", 0, t);

	if (hilightLockVal) {
		pluginDesc.addAttribute(Attrs::PluginAttr("hilight_glossiness", reflectionColourValue));
	}

	return PluginResult::PluginResultNA;
}
