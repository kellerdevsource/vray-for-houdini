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

#include "sop_GeomPlane.h"


#include <GEO/GEO_Point.h>
#include <GU/GU_PrimPoly.h>


using namespace VRayForHoudini;


static PRM_Name prm_size_name("plane_size", "Viewport Size");


void SOP::GeomPlane::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "GeomPlane";
}


void SOP::GeomPlane::AddAttributes(Parm::VRayPluginInfo *pluginInfo)
{
	pluginInfo->prm_template.push_back(PRM_Template(PRM_FLT, 1, &prm_size_name, PRMoneDefaults));
}


OP_ERROR SOP::GeomPlane::cookMySop(OP_Context &context)
{
	PRINT_INFO("SOP::GeomPlane::cookMySop()");

	if(error() < UT_ERROR_ABORT) {
		gdp->clearAndDestroy();
	}

	const float size = evalFloat(prm_size_name.getToken(), 0, 0.0);

	GU_PrimPoly *poly = GU_PrimPoly::build(gdp, 4, GU_POLY_CLOSED, 0);

	GA_Offset pOff = gdp->appendPoint();
	gdp->setPos3(pOff, UT_Vector4F(-size, 0.0f, -size));
	poly->setVertexPoint(0, pOff);

	pOff = gdp->appendPoint();
	gdp->setPos3(pOff, UT_Vector4F(-size, 0.0f,  size));
	poly->setVertexPoint(1, pOff);

	pOff = gdp->appendPoint();
	gdp->setPos3(pOff, UT_Vector4F( size, 0.0f,  size));
	poly->setVertexPoint(2, pOff);

	pOff = gdp->appendPoint();
	gdp->setPos3(pOff, UT_Vector4F( size, 0.0f, -size));
	poly->setVertexPoint(3, pOff);

	poly->reverse();

#if UT_MAJOR_VERSION_INT < 14
	gdp->notifyCache(GU_CACHE_ALL);
#endif

	return error();
}


OP::VRayNode::PluginResult SOP::GeomPlane::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = Attrs::PluginDesc::GetPluginName(this);

	return OP::VRayNode::PluginResultSuccess;
}
