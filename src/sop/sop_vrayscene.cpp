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

#include "sop_vrayscene.h"

using namespace VRayForHoudini;
using namespace SOP;

VRayScene::VRayScene(OP_Network *parent, const char *name, OP_Operator *entry)
	: NodePackedBase("VRaySceneRef", parent, name, entry)
{}

void VRayScene::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID = "VRayScene";
}

void VRayScene::setTimeDependent()
{
	enum class VRaySceneAnimType {
		loop = 0,
		once,
		pingPong,
		still,
	};

	bool previewMeshAnimated;
	if (evalInt("anim_override", 0, 0.0)) {
		previewMeshAnimated = static_cast<VRaySceneAnimType>(evalInt("anim_type", 0, 0.0)) != VRaySceneAnimType::still;
	}
	else {
		previewMeshAnimated = true;
	}

	previewMeshAnimated = evalInt("animated_preview", 0, 0.0) && previewMeshAnimated;

	flags().setTimeDep(previewMeshAnimated);
}

void VRayScene::updatePrimitive(const OP_Context &context)
{
	// Set the options on the primitive

	const int timeDependent = evalInt("animated_preview", 0, context.getTime());
	OP_Options primOptions;
	primOptions.setOptionI("cache", timeDependent);
	for (int i = 0; i < getParmList()->getEntries(); ++i) {
		const PRM_Parm &prm = getParm(i);
		primOptions.setOptionFromTemplate(this, prm, *prm.getTemplatePtr(), context.getTime());
	}

	UT_String pluginMappings = "";

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
	primOptions.setOptionF("current_frame", flags().getTimeDep() ? context.getFloatFrame() : 0.0f);

	updatePrimitiveFromOptions(primOptions);
}

#endif // CGR_HAS_VRAYSCENE
