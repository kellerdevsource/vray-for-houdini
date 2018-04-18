//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifdef CGR_HAS_VRAYSCENE

#include "vfh_vray.h"
#include "sop_vrayscene.h"
#include "gu_vraysceneref.h"

using namespace VRayForHoudini;
using namespace SOP;
using namespace VUtils::Vrscene::Preview;

enum class FlipAxisMode {
	none = 0,  ///< No flipping.
	automatic, ///< Gets the flipping from the vrscene description.
	flipZY,    ///< Force the scene to flip the Z and Y axis.
};

/// Converts "flip_axis" saved as a string parameter to its corresponding
/// FlipAxisMode enum value.
/// @flipAxisModeS The value of the flip_axis parameter
/// @returns The corresponding to flipAxisModeS enum value
static FlipAxisMode parseFlipAxisMode(const UT_String &flipAxisModeS)
{
	FlipAxisMode mode = FlipAxisMode::none;

	if (flipAxisModeS.isInteger()) {
		mode = static_cast<FlipAxisMode>(flipAxisModeS.toInt());
	}

	return mode;
}

VRayScene::VRayScene(OP_Network *parent, const char *name, OP_Operator *entry)
	: NodePackedBase("VRaySceneRef", parent, name, entry)
{}

void VRayScene::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID = SL("VRayScene");
}

void VRayScene::setTimeDependent()
{
	enum class VRaySceneAnimType {
		loop = 0,
		once,
		pingPong,
		still,
	};

	bool previewMeshAnimated = evalInt("animated_preview", 0, 0.0);
	if (previewMeshAnimated) {
		if (evalInt("anim_override", 0, 0.0)) {
			previewMeshAnimated = static_cast<VRaySceneAnimType>(evalInt("anim_type", 0, 0.0)) != VRaySceneAnimType::still;
		}
	}

	flags().setTimeDep(previewMeshAnimated);
}

static int offs = 0;

int VRayScene::process(const VrsceneSceneObject &object)
{
	const VUtils::CharString objectName = object.getName();

	PrimWithOptions &prim = prims[objectName.ptr()];
	prim.options.setOptionS("object_name", object.getName().ptr());

	prim.prim = GU_PrimPacked::build(*gdp, m_primType);
	vassert(prim.prim);

	GA_PrimitiveGroup *primGroup = gdp->newPrimitiveGroup(objectName.ptr());
	primGroup->add(prim.prim);

	// Set the location of the packed primitive point.
	const UT_Vector3 pivot(0.0, 0.0, 0.0);
	prim.prim->setPivot(pivot);

	gdp->setPos3(prim.prim->getPointOffset(0), pivot);

	return 1;
}

void VRayScene::getCreatePrimitive()
{
	prims.clear();

	gdp->stashAll();

	UT_String filePath;
	evalString(filePath, "filepath", 0, 0.0);

	VrsceneDesc *vrsceneDesc = vrsceneMan.getVrsceneDesc(filePath.buffer(), &getSettings());
	if (vrsceneDesc) {
		vrsceneDesc->enumSceneObjects(*this);
	}

	gdp->destroyStashed();
}

void VRayScene::updatePrimitiveFromOptions(const OP_Options &options)
{
	for (PrimWithOptions &prim : prims) {
		prim.options.merge(options);

		GU_PackedImpl *primImpl = prim.prim->implementation();
		if (primImpl) {
#ifdef HDK_16_5
			primImpl->update(prim.prim, prim.options);
#else
			primImpl->update(prim.options);
#endif
		}
	}
}

void VRayScene::updatePrimitive(const OP_Context &context)
{
	OP_Options primOptions;
	primOptions.setOptionI("cache", flags().getTimeDep());
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

	int shouldFlip = false;

	UT_String filePath;
	evalString(filePath, "filepath", 0, 0.0);
	if (filePath.isstring()) {
		VrsceneDesc *vrsceneDesc = vrsceneMan.getVrsceneDesc(filePath.buffer(), &getSettings());
		if (vrsceneDesc) {
			const VrsceneSceneInfo &sceneInfo = vrsceneDesc->getSceneInfo();

			UT_String flipAxisMode;
			evalString(flipAxisMode, "flip_axis", 0, 0.0);

			const FlipAxisMode flipAxis = parseFlipAxisMode(flipAxisMode);

			shouldFlip = flipAxis == FlipAxisMode::flipZY ||
			             (flipAxis == FlipAxisMode::automatic && sceneInfo.getUpAxis() == vrsceneUpAxisZ);
		}
	}

	primOptions.setOptionI("should_flip", shouldFlip);

	updatePrimitiveFromOptions(primOptions);
}

const VrsceneSettings &VRayScene::getSettings()
{
	// TODO: Fill settings from node attributes.
	return settings;
}

#endif // CGR_HAS_VRAYSCENE
