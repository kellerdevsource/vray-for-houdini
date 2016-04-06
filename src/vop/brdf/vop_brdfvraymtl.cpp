//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_brdfvraymtl.h"


using namespace VRayForHoudini;


void VOP::BRDFVRayMtl::setPluginType()
{
	pluginType = "BRDF";
	pluginID   = "BRDFVRayMtl";
}


void VOP::BRDFVRayMtl::getCode(UT_String &codestr, const VOP_CodeGenContext &)
{
	codestr.sprintf("Cf = {%.3f,%.3f,%.3f};",
					0.0f, 1.0f, 0.0f);
}


OP::VRayNode::PluginResult VOP::BRDFVRayMtl::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	// TODO:
#if 0
	if (RNA_boolean_get(&brdfVRayMtl, "hilight_glossiness_lock")) {
		pluginAttrs["hilight_glossiness"] = pluginAttrs["reflect_glossiness"];
	}
#endif
	return PluginResult::PluginResultNA;
}
