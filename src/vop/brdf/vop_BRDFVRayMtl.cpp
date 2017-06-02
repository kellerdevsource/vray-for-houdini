

#include "vop_BRDFVRayMtl.h"


VRayForHoudini::OP::VRayNode::PluginResult 
	BRDFVRayMtl::asPluginDesc(VRayForHoudini::Attrs::PluginDesc &pluginDesc, 
								VRayForHoudini::VRayExporter &exporter, 
								VRayForHoudini::ExportContext *parentContext){


	const fpreal &t = exporter.getContext().getTime();

	const int hilightLockVal = evalInt("hilight_glossiness_lock", 0, t);
	const fpreal hilightGlossinessValue = evalFloat("hilight_glossiness", 0,t);
	const fpreal reflectionColourValue = evalFloat("reflect_glossiness", 0,t);

	if(hilightLockVal){
		pluginDesc.addAttribute(VRayForHoudini::Attrs::PluginAttr("hilight_glossiness", reflectionColourValue));
		pluginDesc.addAttribute(VRayForHoudini::Attrs::PluginAttr("reflect_glossiness", reflectionColourValue));
	}
	else{
		pluginDesc.addAttribute(VRayForHoudini::Attrs::PluginAttr("hilight_glossiness", hilightGlossinessValue));
		pluginDesc.addAttribute(VRayForHoudini::Attrs::PluginAttr("reflect_glossiness", reflectionColourValue));
	}

	return PluginResult::PluginResultNA;
}