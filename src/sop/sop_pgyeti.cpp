//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_log.h"
#include "sop_pgyeti.h"

using namespace VRayForHoudini;
using namespace SOP;

void VRayPgYeti::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID = "VRayPgYeti";
}

void VRayPgYeti::setTimeDependent()
{
	flags().setTimeDep(false);
}

void VRayPgYeti::updatePrimitive(const OP_Context &context)
{
	vassert(m_primPacked);

	OP_Options primOptions;
	for (int i = 0; i < getParmList()->getEntries(); ++i) {
		const PRM_Parm &prm = getParm(i);
		primOptions.setOptionFromTemplate(this, prm, *prm.getTemplatePtr(), context.getTime());
	}

	if (m_primOptions != primOptions) {
		m_primOptions = primOptions;

		GU_PackedImpl *primImpl = m_primPacked->implementation();
		if (primImpl) {
			primImpl->update(m_primOptions);
		}
	}
}

OP_ERROR VRayPgYeti::cookMySop(OP_Context &context)
{
	Log::getLog().debug("VRayPgYeti::cookMySop()");

	if (!m_primPacked) {
		m_primPacked = GU_PrimPacked::build(*gdp, "VRayPgYetiRef");

		// Set the location of the packed primitive point.
		const UT_Vector3 pivot(0.0, 0.0, 0.0);
		m_primPacked->setPivot(pivot);
		gdp->setPos3(m_primPacked->getPointOffset(0), pivot);
	}

	vassert(m_primPacked);

	setTimeDependent();
	updatePrimitive(context);

	return error();
}
