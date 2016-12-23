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
	return PluginResult::PluginResultContinue;
}
