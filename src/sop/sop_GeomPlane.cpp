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

#include "gu_geomplaneref.h"

#include <GU/GU_PrimPoly.h>
#include <OP/OP_Options.h>

using namespace VRayForHoudini;

void SOP::GeomPlane::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID   = "GeomInfinitePlane";
}

OP_ERROR SOP::GeomPlane::cookMySop(OP_Context &context)
{
	const fpreal t = context.getTime();

	gdp->stashAll();
	
	GU_PrimPacked *pack = GU_PrimPacked::build(*gdp, "GeomInfinitePlaneRef");
	if (NOT(pack)) {
		addWarning(SOP_MESSAGE, "Can't create packed primitive GeomInfinitePlaneRef");
	}
	else {
		// Set the location of the packed primitive's point.
		UT_Vector3 pivot(0, 0, 0);
		pack->setPivot(pivot);
		gdp->setPos3(pack->getPointOffset(0), pivot);

		// Set the options on the primitive
		OP_Options options;
		for (int i = 0; i < getParmList()->getEntries(); ++i) {
			const PRM_Parm &prm = getParm(i);
			options.setOptionFromTemplate(this, prm, *prm.getTemplatePtr(), t);
		}

		pack->implementation()->update(options);
	}

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
