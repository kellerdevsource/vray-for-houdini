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
	pluginType = "BRDF";
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
	parms.invgamma = options.getOptionF("invgamma");
	parms.saturation = options.getOptionF("saturation");
	parms.depthMul = options.getOptionF("depthMul");
	parms.disablewmap = options.getOptionI("disablewmap");
	// filter color
	parms.filter.rgb[0] = options.getOptionV3("filter").r();
	parms.filter.rgb[1] = options.getOptionV3("filter").g();
	parms.filter.rgb[2] = options.getOptionV3("filter").b();
	// uvtrans
	UT_DMatrix4 m4;
	OP_Node::buildXform(options.getOptionI("trs"),
						options.getOptionI("xyz"),
						options.getOptionV3("trans").x(), options.getOptionV3("trans").y(), options.getOptionV3("trans").z(),
						options.getOptionV3("rot").x(), options.getOptionV3("rot").y(), options.getOptionV3("rot").z(),
						options.getOptionV3("scale").x(), options.getOptionV3("scale").y(), options.getOptionV3("scale").z(),
						options.getOptionV3("pivot").x(), options.getOptionV3("pivot").y(), options.getOptionV3("pivot").z(),
						m4);
	parms.uvtrans = VRayExporter::Matrix4ToTransform(m4);

	parms.usemap = options.getOptionI("usemap");
	parms.bumpmul = options.getOptionF("bumpmul");
	parms.bumpstart = options.getOptionF("bumpstart");
	parms.nsamples = options.getOptionI("subdivisions");
	parms.nsamples *= parms.nsamples * parms.nsamples;
	parms.cutoff = options.getOptionF("cutoff");
	parms.mapChannel = options.getOptionI("mapChannel");
	parms.dome = options.getOptionI("dome");
	parms.multdirect = options.getOptionF("multdirect");
	parms.multrefl = options.getOptionF("multrefl");
	parms.multgi = options.getOptionF("multgi");
	parms.traceDepth = options.getOptionI("traceDepth");
	parms.scrambleSize = options.getOptionF("scrambleSize");
	parms.sceneScale = options.getOptionF("sceneScale");
	parms.ccior = options.getOptionF("ccior");
	parms.cchlight = options.getOptionI("cchlight");
	parms.ccbump = options.getOptionF("ccbump");
	parms.unfRefl = options.getOptionI("unfRefl");
	parms.twoside = options.getOptionI("twoside");
	parms.displace = options.getOptionI("displace");
	parms.noPrimGI = options.getOptionI("noPrimGI");
	parms.retrace = options.getOptionF("retrace");
	parms.noTransp = options.getOptionI("noTransp");
	parms.transpMul = options.getOptionF("transpMul");

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
