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
	pluginID   = "GeomPlane";
}

OP_ERROR SOP::GeomPlane::cookMySop(OP_Context &context)
{
	const fpreal t = context.getTime();

	gdp->stashAll();

	GU_PrimPacked *pack = GU_PrimPacked::build(*gdp, "GeomPlaneRef");
	if (NOT(pack)) {
		addWarning(SOP_MESSAGE, "Can't create packed primitive GeomPlaneRef");
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

		GU_PackedImpl *primImpl = pack->implementation();
		if (primImpl) {
#ifdef HDK_16_5
			primImpl->update(pack, options);
#else
			primImpl->update(options);
#endif
		}
	}

	gdp->destroyStashed();

	return error();
}
