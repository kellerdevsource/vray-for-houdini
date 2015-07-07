//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
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


OP::VRayNode::PluginResult SOP::GeomMayaHair::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = Attrs::PluginDesc::GetPluginName(this, "Hair@");

	GU_DetailHandleAutoReadLock gdl(getCookedGeoHandle(exporter->getContext()));

	const GU_Detail *gdp = gdl.getGdp();
	if (NOT(gdp)) {
		PRINT_ERROR("Node \"%s\": Detail is NULL!",
					getName().buffer());
	}
	else {
		exporter->exportGeomMayaHairGeom(this, gdp, pluginDesc);
		exporter->setAttrsFromOpNode(pluginDesc, this);
	}

	return OP::VRayNode::PluginResultContinue;
}
