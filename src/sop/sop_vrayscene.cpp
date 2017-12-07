//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifdef CGR_HAS_VRAYSCENE

#include "vfh_log.h"
#include "sop_vrayscene.h"

using namespace VRayForHoudini;

void SOP::VRayScene::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID = "VRayScene";
}

void SOP::VRayScene::setTimeDependent()
{
	enum class VRaySceneAnimType {
		loop = 0,
		once,
		pingPong,
		still,
	};

	// TODO: Implement animation caching and enable this.
#if 0
	previewMeshAnimated =
		static_cast<VRaySceneAnimType>(evalInt("anim_type", 0, 0.0)) != VRaySceneAnimType::still;

	flags().setTimeDep(previewMeshAnimated);
#endif
}

void SOP::VRayScene::updatePrimitive(const OP_Context &context)
{
	vassert(m_primPacked);

	// Set the options on the primitive
	OP_Options primOptions;
	for (int i = 0; i < getParmList()->getEntries(); ++i) {
		const PRM_Parm &prm = getParm(i);
		primOptions.setOptionFromTemplate(this, prm, *prm.getTemplatePtr(), context.getTime());
	}

	UT_String pluginMappings;

	const int numMappings = evalInt("plugin_mapping", 0, 0.0);
	for (int i = 1; i <= numMappings; ++i) {
		UT_String opPath;
		evalStringInst("plugin_mapping#op_path", &i, opPath, 0, 0.0f);

		UT_String pluginName;
		evalStringInst("plugin_mapping#plugin_name", &i, pluginName, 0, 0.0f);

		pluginMappings.append(opPath.buffer());
		pluginMappings.append('=');
		pluginMappings.append(pluginName.buffer());
		pluginMappings.append(';');
	}

	primOptions.setOptionS("plugin_mapping", pluginMappings);
	primOptions.setOptionF("current_frame", previewMeshAnimated ? context.getFloatFrame() : 0.0f);

	if (m_primOptions != primOptions) {
		m_primOptions = primOptions;

		GU_PackedImpl *primImpl = m_primPacked->implementation();
		if (primImpl) {
#ifdef HDK_16_5
			primImpl->update(m_primPacked, m_primOptions);
#else
			primImpl->update(m_primOptions);
#endif
		}
	}
}

OP_ERROR SOP::VRayScene::cookMySop(OP_Context &context)
{
	Log::getLog().debug("SOP::VRayScene::cookMySop()");

	if (!m_primPacked) {
		m_primPacked = GU_PrimPacked::build(*gdp, "VRaySceneRef");

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

#endif // CGR_HAS_VRAYSCENE
