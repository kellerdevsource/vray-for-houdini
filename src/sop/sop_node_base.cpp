//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "sop_node_base.h"

using namespace VRayForHoudini;
using namespace SOP;

void NodeBase::setTimeDependent()
{
	flags().setTimeDep(false);
}

void NodePackedBase::updatePrimitiveFromOptions(const OP_Options &options)
{
	if (m_primOptions == options)
		return;

	m_primOptions = options;

	GU_PackedImpl *primImpl = m_primPacked->implementation();
	if (primImpl) {
#ifdef HDK_16_5
		primImpl->update(m_primPacked, m_primOptions);
#else
		primImpl->update(m_primOptions);
#endif
	}
}

void NodePackedBase::updatePrimitive(const OP_Context &context)
{
	// Set the options on the primitive
	OP_Options primOptions;
	for (int i = 0; i < getParmList()->getEntries(); ++i) {
		const PRM_Parm &prm = getParm(i);
		primOptions.setOptionFromTemplate(this, prm, *prm.getTemplatePtr(), context.getTime());
	}

	updatePrimitiveFromOptions(primOptions);
}

void NodePackedBase::getCreatePrimitive()
{
	vassert(gdp);

	m_primPacked = nullptr;

	for (GA_Iterator jt(gdp->getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = gdp->getGEOPrimitive(*jt);
		if (prim && GU_PrimPacked::isPackedPrimitive(*prim)) {
			if (m_primType.equal(prim->getTypeName())) {
				m_primPacked = const_cast<GU_PrimPacked*>(static_cast<const GU_PrimPacked*>(prim));
				break;
			}
		}
	}

	if (!m_primPacked) {
		m_primPacked = GU_PrimPacked::build(*gdp, m_primType);
		vassert(m_primPacked);

		// Set the location of the packed primitive point.
		const UT_Vector3 pivot(0.0, 0.0, 0.0);
		m_primPacked->setPivot(pivot);

		gdp->setPos3(m_primPacked->getPointOffset(0), pivot);
	}
}

OP_ERROR NodePackedBase::cookMySop(OP_Context &context)
{
	Log::getLog().debug("NodePackedBase::cookMySop(\"%s\")", m_primType.buffer());

	getCreatePrimitive();

	setTimeDependent();

	updatePrimitive(context);

	return error();
}
