//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_BRDFScanned.h"
#include <OP/OP_NodeInfoParms.h>

using namespace VRayForHoudini;


namespace {

const char *const VFH_PLUGIN_ATTRIBUTE = "vray_pluginattr";

}


void VOP::BRDFScanned::getNodeSpecificInfoText(OP_Context &context, OP_NodeInfoParms &iparms)
{
	VOP::NodeBase::getNodeSpecificInfoText(context, iparms);

	VRay::ScannedMaterialLicenseError err;
	if (!VRayPluginRenderer::hasVRScansGUILicense(err)) {
		iparms.appendSprintf("Unable to obtain VRScans GUI license: %s", err.toString());
	}
}


bool VOP::BRDFScanned::updateParmsFlags()
{
	bool changed = VOP::NodeBase::updateParmsFlags();

	VRay::ScannedMaterialLicenseError err;
	bool hasGUI = VRayPluginRenderer::hasVRScansGUILicense(err);
	for (int i = 0; i < getParmList()->getEntries(); ++i) {
		const PRM_Parm &prm = getParm(i);
		const PRM_SpareData *spare = prm.getSparePtr();
		if (spare && !spare->getValue(VFH_PLUGIN_ATTRIBUTE)) {
			changed |= enableParm(i, hasGUI);
		}
	}

	return changed;
}


void VOP::BRDFScanned::setPluginType()
{
	pluginType = "BRDF";
	pluginID   = "BRDFScanned";
}


OP::VRayNode::PluginResult VOP::BRDFScanned::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	const fpreal t = exporter.getContext().getTime();

	VRay::ScannedMaterialParams parms;
	int ival = 0;
	fpreal fval = 0;

	evalParameterOrProperty("plain", 0, t, ival);
	parms.plain = static_cast<VRay::ScannedMaterialParams::Plain>(ival);
	evalParameterOrProperty("invgamma", 0, t, fval);
	parms.invgamma = fval;
	evalParameterOrProperty("saturation", 0, t, fval);
	parms.saturation = fval;
	evalParameterOrProperty("depthMul", 0, t, fval);
	parms.depthMul = fval;
	evalParameterOrProperty("disablewmap", 0, t, ival);
	parms.disablewmap = ival;
	// filter color
	evalParameterOrProperty("filter", 0, t, fval);
	parms.filter.rgb[0] = fval;
	evalParameterOrProperty("filter", 1, t, fval);
	parms.filter.rgb[1] = fval;
	evalParameterOrProperty("filter", 2, t, fval);
	parms.filter.rgb[2] = fval;
//	TODO: uvtrans
	evalParameterOrProperty("usemap", 0, t, ival);
	parms.usemap = ival;
	evalParameterOrProperty("bumpmul", 0, t, fval);
	parms.bumpmul = fval;
	evalParameterOrProperty("bumpstart", 0, t, fval);
	parms.bumpstart = fval;
	evalParameterOrProperty("subdivision", 0, t, ival);
	parms.nsamples = ival*ival;
	evalParameterOrProperty("cutoff", 0, t, fval);
	parms.cutoff = fval;
	evalParameterOrProperty("mapChannel", 0, t, ival);
	parms.mapChannel = ival;
	evalParameterOrProperty("dome", 0, t, ival);
	parms.dome = ival;
	evalParameterOrProperty("multdirect", 0, t, fval);
	parms.multdirect = fval;
	evalParameterOrProperty("multrefl", 0, t, fval);
	parms.multrefl = fval;
	evalParameterOrProperty("multgi", 0, t, fval);
	parms.multgi = fval;
	evalParameterOrProperty("traceDepth", 0, t, ival);
	parms.traceDepth = ival;
	evalParameterOrProperty("scrambleSize", 0, t, fval);
	parms.scrambleSize = fval;
	evalParameterOrProperty("sceneScale", 0, t, fval);
	parms.sceneScale = fval;
	evalParameterOrProperty("ccior", 0, t, fval);
	parms.ccior = fval;
	evalParameterOrProperty("cchlight", 0, t, ival);
	parms.cchlight = ival;
	evalParameterOrProperty("ccbump", 0, t, fval);
	parms.ccbump = fval;
	evalParameterOrProperty("unfRefl", 0, t, ival);
	parms.unfRefl = ival;
	evalParameterOrProperty("twoside", 0, t, ival);
	parms.twoside = ival;
	evalParameterOrProperty("displace", 0, t, ival);
	parms.displace = ival;
	evalParameterOrProperty("noPrimGI", 0, t, ival);
	parms.noPrimGI = ival;
	evalParameterOrProperty("retrace", 0, t, fval);
	parms.retrace = fval;
	evalParameterOrProperty("noTransp", 0, t, ival);
	parms.noTransp = ival;
	evalParameterOrProperty("transpMul", 0, t, fval);
	parms.transpMul = fval;

	VRay::IntList parmBlock;
	VRay::ScannedMaterialLicenseError err;
	if (VRay::encodeScannedMaterialParams(parms, parmBlock, err)) {
		pluginDesc.addAttribute(Attrs::PluginAttr("param_block", parmBlock));
	}
	else {
		pluginDesc.addAttribute(Attrs::PluginAttr("param_block", 0));
		const char *errMsg = (err.error())? err.toString() : "V-Ray AppSDK internal error";
		Log::getLog().info("Unable to obtain VRScans GUI license: %s", errMsg);
	}

	return PluginResult::PluginResultContinue;
}
