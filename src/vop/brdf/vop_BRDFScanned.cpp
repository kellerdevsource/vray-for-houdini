//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_BRDFScanned.h"
#include <OP/OP_NodeInfoParms.h>
#include <OP/OP_Options.h>


using namespace VRayForHoudini;


namespace {

const char *const VFH_SPARE_PLUGIN_ATTRIBUTE = "vray_pluginattr";

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
		if (!spare || !spare->getValue(VFH_SPARE_PLUGIN_ATTRIBUTE)) {
			changed |= enableParm(i, hasGUI);
		}
	}

	return changed;
}


void VOP::BRDFScanned::setPluginType()
{
	pluginType = VRayPluginType::BRDF;
	pluginID   = "BRDFScanned";
}


OP::VRayNode::PluginResult VOP::BRDFScanned::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	const fpreal t = exporter.getContext().getTime();

	OP_Options options;
	for (int i = 0; i < getParmList()->getEntries(); ++i) {
		const PRM_Parm &prm = getParm(i);
		options.setOptionFromTemplate(this, prm, *prm.getTemplatePtr(), t);
	}

	VRay::ScannedMaterialParams parms;
	parms.plain = static_cast<VRay::ScannedMaterialParams::Plain>(options.getOptionI("plain"));
	parms.invgamma = 1.0 / options.getOptionF("gamma");
	parms.saturation = options.getOptionF("saturation");
	parms.depthMul = options.getOptionF("depth_mult");
	parms.disablewmap = options.getOptionI("disablewmap");
	// filter color
	parms.filter.rgb[0] = options.getOptionV3("filter").r();
	parms.filter.rgb[1] = options.getOptionV3("filter").g();
	parms.filter.rgb[2] = options.getOptionV3("filter").b();
	// uvtrans
	OP_Node *uvtrans = VRayExporter::getConnectedNode(this, "uvtrans");
	if (uvtrans && uvtrans->getOpTypeID() == VOP_OPTYPE_ID) {
		parms.uvtrans = exporter.exportTransformVop(*(uvtrans->castToVOPNode()));
	}
	else {
		parms.uvtrans.makeIdentity();
	}

	parms.usemap = options.getOptionI("usemap");
	parms.bumpmul = options.getOptionF("bumpmul");
	parms.bumpstart = options.getOptionF("bumpstart");
	parms.nsamples = options.getOptionI("subdivs");
	parms.nsamples *= parms.nsamples * parms.nsamples;
	parms.cutoff = options.getOptionF("cutoff");
	parms.mapChannel = options.getOptionI("map_channel");
	parms.dome = options.getOptionI("dome");
	parms.multdirect = options.getOptionF("multdirect");
	parms.multrefl = options.getOptionF("multrefl");
	parms.multgi = options.getOptionF("multgi");
	parms.traceDepth = options.getOptionI("trace_depth");
	parms.scrambleSize = options.getOptionF("scramble_size");
	parms.sceneScale = options.getOptionF("scene_scale");
	parms.ccior = options.getOptionF("ccior");
	parms.cchlight = options.getOptionI("cchlight");
	parms.ccbump = options.getOptionF("ccbump");
	parms.unfRefl = options.getOptionI("unf_refl");
	parms.twoside = options.getOptionI("twoside");
	parms.displace = options.getOptionI("displace");
	parms.noPrimGI = options.getOptionI("no_prim_gi");
	parms.retrace = options.getOptionF("retrace");
	parms.noTransp = options.getOptionI("no_transparency");
	parms.transpMul = options.getOptionF("transparency_mult");

	VRay::IntList parmBlock;
	VRay::ScannedMaterialLicenseError err;
	if (VRay::encodeScannedMaterialParams(parms, parmBlock, err)) {
		pluginDesc.addAttribute(Attrs::PluginAttr("param_block", parmBlock));
	}
	else {
		pluginDesc.addAttribute(Attrs::PluginAttr("param_block", 0));
	}

	return PluginResult::PluginResultContinue;
}
