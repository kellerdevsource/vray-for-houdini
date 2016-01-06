//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "sop_GeomMayaHair.h"


using namespace VRayForHoudini;


void SOP::GeomMayaHair::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "GeomMayaHair";
}


OP_NodeFlags& SOP::GeomMayaHair::flags()
{
	OP_NodeFlags &flags = SOP_Node::flags();

	// This is a fake node for settings only
	flags.setBypass(true);

	return flags;
}


OP_ERROR SOP::GeomMayaHair::cookMySop(OP_Context &context)
{
	return error();
}


OP::VRayNode::PluginResult SOP::GeomMayaHair::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this, "Hair@");

	GU_DetailHandleAutoReadLock gdl(getCookedGeoHandle(exporter.getContext()));

	const GU_Detail *gdp = gdl.getGdp();
	if (NOT(gdp)) {
		Log::getLog().error("Node \"%s\": Detail is NULL!",
					getName().buffer());
	}
	else {
		exporter.exportGeomMayaHairGeom(this, gdp, pluginDesc);
		exporter.setAttrsFromOpNodePrms(pluginDesc, this);
	}

	return OP::VRayNode::PluginResultContinue;
}
