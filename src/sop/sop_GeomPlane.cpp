//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "sop_GeomPlane.h"


#include <GEO/GEO_Point.h>
#include <GU/GU_PrimPoly.h>


using namespace VRayForHoudini;


static PRM_Name prm_size_name("plane_size", "Viewport Size");


void SOP::GeomPlane::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "GeomInfinitePlane";
}


void SOP::GeomPlane::addPrmTemplate(Parm::PRMList &prmTemplate)
{
	prmTemplate.addPrm(PRM_Template(PRM_FLT, 1, &prm_size_name, PRMoneDefaults));
}


OP_ERROR SOP::GeomPlane::cookMySop(OP_Context &context)
{
	Log::getLog().info("SOP::GeomPlane::cookMySop()");

	if(error() < UT_ERROR_ABORT) {
		gdp->clearAndDestroy();
	}

	const float size = evalFloat(prm_size_name.getToken(), 0, 0.0);

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

	return error();
}


OP::VRayNode::PluginResult SOP::GeomPlane::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);
	pluginDesc.addAttribute(Attrs::PluginAttr("normal", VRay::Vector(0.f,1.f,0.f)));

	return OP::VRayNode::PluginResultSuccess;
}
