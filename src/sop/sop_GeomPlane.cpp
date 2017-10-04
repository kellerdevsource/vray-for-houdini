//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "sop_GeomPlane.h"

#include <GU/GU_PrimPoly.h>

using namespace VRayForHoudini;

void SOP::GeomPlane::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID   = "GeomInfinitePlane";
}

OP_ERROR SOP::GeomPlane::cookMySop(OP_Context &context)
{
	gdp->stashAll();

	const float size = evalFloat("plane_size", 0, 0.0);

	GU_PrimPoly *poly = GU_PrimPoly::build(gdp, 4, GU_POLY_CLOSED, 0);

	GA_Offset pOff = gdp->appendPoint();
	gdp->setPos3(pOff, UT_Vector3(-size, 0.0f, -size));
	poly->setVertexPoint(0, pOff);

	pOff = gdp->appendPoint();
	gdp->setPos3(pOff, UT_Vector3(-size, 0.0f,  size));
	poly->setVertexPoint(1, pOff);

	pOff = gdp->appendPoint();
	gdp->setPos3(pOff, UT_Vector3( size, 0.0f,  size));
	poly->setVertexPoint(2, pOff);

	pOff = gdp->appendPoint();
	gdp->setPos3(pOff, UT_Vector3( size, 0.0f, -size));
	poly->setVertexPoint(3, pOff);

	poly->reverse();

	gdp->destroyStashed();

	return error();
}

OP::VRayNode::PluginResult SOP::GeomPlane::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);
	pluginDesc.addAttribute(Attrs::PluginAttr("normal", VRay::Vector(0.f,1.f,0.f)));

	return OP::VRayNode::PluginResultSuccess;
}
