//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_BRDFVRayMtl.h"

using namespace VRayForHoudini;

void BRDFVRayMtl::setPluginType()
{
	pluginType = VRayPluginType::BRDF;
	pluginID = "BRDFVRayMtl";
}

OP::VRayNode::PluginResult BRDFVRayMtl::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter)
{
	const fpreal t = exporter.getContext().getTime();

	const int hilightLockVal = evalInt("hilight_glossiness_lock", 0, t);

	const fpreal hilightGlossiness = evalFloat("hilight_glossiness", 0, t);
	const fpreal reflectGlossiness = evalFloat("reflect_glossiness", 0, t);

	pluginDesc.add(SL("hilight_glossiness"), hilightLockVal ? reflectGlossiness : hilightGlossiness);

	const fpreal selfIlluminationMult = evalFloat("self_illumination_mult", 0, t);

	pluginDesc.add(SL("self_illumination"),
	               evalFloat("self_illumination", 0, t) * selfIlluminationMult,
	               evalFloat("self_illumination", 1, t) * selfIlluminationMult,
	               evalFloat("self_illumination", 2, t) * selfIlluminationMult,
	               evalFloat("self_illumination", 3, t) * selfIlluminationMult,
	               isParmTimeDependent("self_illumination_mult") || isParmTimeDependent("self_illumination")
	);

	return PluginResultNA;
}
