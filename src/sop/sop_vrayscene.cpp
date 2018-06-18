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

VRayScene::VRayScene(OP_Network *parent, const char *name, OP_Operator *entry)
	: NodePackedBase("VRaySceneRef", parent, name, entry)
{}

VRayScene::~VRayScene()
{}

void VRayScene::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID = SL("VRayScene");
}

void VRayScene::setTimeDependent()
{
	bool previewMeshAnimated = evalInt("animated_preview", 0, 0.0);
	if (previewMeshAnimated) {
		if (evalInt("anim_override", 0, 0.0)) {
			previewMeshAnimated = static_cast<VRaySceneAnimType>(evalInt("anim_type", 0, 0.0)) != VRaySceneAnimType::still;
		}
	}

	flags().setTimeDep(previewMeshAnimated);
}

PrimWithOptions& VRayScene::createPrimitive(const QString &name)
{
	PrimWithOptions &prim = prims[name];

	prim.prim = GU_PrimPacked::build(*gdp, m_primType);
	vassert(prim.prim);

	// Set the location of the packed primitive point.
	const UT_Vector3 pivot(0.0, 0.0, 0.0);
	prim.prim->setPivot(pivot);

	gdp->setPos3(prim.prim->getPointOffset(0), pivot);

	return prim;
}

#define VRAYSCENE_PRINT_HIERARCHY 0
#if VRAYSCENE_PRINT_HIERARCHY
static int indent = 0;

static void enumSceneObjectChildren(const VrsceneSceneObject &object)
{
	const ObjectBaseTable &plugins = object.getObjectPlugins();
	const VrsceneSceneObjects &children = object.getChildren();

	if (!children.empty()) {
		const QString indentStr(indent * 2, ' ');
		printf("%s\"%s\" [%s]:\n", qPrintable(indentStr), object.getName().ptr(), object.getPath().ptr());

		FOR_CONST_IT(VrsceneSceneObjects, it, children) {
			indent++;
			enumSceneObjectChildren(*it.data());
			indent--;
		}
	}
	else if (plugins.count()) {
		for (int i = 0; i < plugins.count(); ++i) {
			const VrsceneObjectBase *plugin = plugins[i];

			const QString indentStr(indent * 2, ' ');
			printf("%s  %s\n", qPrintable(indentStr), plugin->getName());
		}
	}
}
#endif

int VRayScene::process(const VrsceneSceneObject &object)
{
	const VUtils::CharString &objectName = object.getName();
	const VUtils::CharString &objectPath = object.getPath();

#if VRAYSCENE_PRINT_HIERARCHY
	enumSceneObjectChildren(object);
#endif

	PrimWithOptions &prim = createPrimitive(objectName.ptr());
	prim.options.setOptionS("object_name", objectName.ptr());
	prim.options.setOptionS("object_path", objectPath.ptr());

	GA_PrimitiveGroup *primGroup = gdp->newPrimitiveGroup(objectName.ptr());
	vassert(primGroup);

	primGroup->add(prim.prim);

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
		// Add top level object primitives.
		vrsceneDesc->enumSceneObjects(*this);

		// If no "scene_name" was found in the scene add a single primitive
		// for the whole scene.
		if (prims.empty()) {
			createPrimitive(SL("vrscene"));
		}
	}

	gdp->destroyStashed();
}

void VRayScene::updatePrimitiveFromOptions(const OP_Options &options)
{
	for (PrimWithOptions &prim : prims) {
		OP_Options primOptions;
		primOptions.merge(options);
		primOptions.merge(prim.options);

		GU_PackedImpl *primImpl = prim.prim->implementation();
		if (primImpl) {
#ifdef HDK_16_5
			primImpl->update(prim.prim, primOptions);
#else
			primImpl->update(primOptions);
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
			const VRaySceneFlipAxisMode flipAxis = getFlipAxisMode(*this);

			shouldFlip = flipAxis == VRaySceneFlipAxisMode::flipZY ||
			             (flipAxis == VRaySceneFlipAxisMode::automatic && sceneInfo.getUpAxis() == vrsceneUpAxisZ);
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
