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
#include "gu_vraysceneref.h"

#include <GEO/GEO_Point.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PackedGeometry.h>
#include <OP/OP_Options.h>

using namespace VRayForHoudini;


VUtils::Vrscene::Preview::VrsceneDescManager  SOP::VRayScene::m_vrsceneMan;


void SOP::VRayScene::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID   = "VRayScene";
}


VUtils::Vrscene::Preview::VrsceneSettings SOP::VRayScene::getVrsceneSettings() const
{
	VUtils::Vrscene::Preview::VrsceneSettings settings;
	settings.usePreview = true;
	settings.previewFacesCount = 100000;
	settings.cacheSettings.cacheType = (VUtils::Vrscene::Preview::VrsceneCacheSettings::VrsceneCacheType)2;
	return settings;
}


OP_ERROR SOP::VRayScene::cookMySop(OP_Context &context)
{
	Log::getLog().debug("SOP::VRayScene::cookMySop()");

	const fpreal t = context.getTime();
	
	gdp->stashAll();

	UT_String path;
	evalString(path, "filepath", 0, t);
	if (path.equal("")) {
		gdp->destroyStashed();
		return error();
	}

	if (error() < UT_ERROR_ABORT) {
		UT_Interrupt *boss = UTgetInterrupt();

		const bool is_animated = evalInt("anim_type", 0, t) != 3;
		flags().setTimeDep(is_animated);

		if (boss) {
			if (boss->opStart("Building V-Ray Scene Preview Mesh")) {
				// Create a packed primitive
				GU_PrimPacked *pack = GU_PrimPacked::build(*gdp, "VRaySceneRef");
				if (NOT(pack)) {
					addWarning(SOP_MESSAGE, "Can't create packed primitive VRaySceneRef");
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
					pack->setPathAttribute(getFullPath());
				}
				boss->opEnd();
			}
		}
	}

	gdp->destroyStashed();
	return error();
}


OP::VRayNode::PluginResult SOP::VRayScene::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	UT_String path;
	evalString(path, "filepath", 0, 0.0f);
	if (NOT(path.isstring())) {
		Log::getLog().error("VRayScene \"%s\": \"Filepath\" is not set!",
							getName().buffer());
		return OP::VRayNode::PluginResultError;
	}

	const bool flipAxis = evalInt("flip_axis", 0, 0.0);

	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(parentContext->getTarget()->castToOBJNode());

	pluginDesc.addAttribute(Attrs::PluginAttr("filepath", path.buffer()));

	VUtils::TraceTransform tm = toVutilsTm(VRayExporter::getObjTransform(parentContext->getTarget()->castToOBJNode(), exporter.getContext()));
	if (flipAxis) {
		tm.m = tm.m * VUtils::Vrscene::Preview::flipMatrix;
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("transform", toAppSdkTm(tm)));

	exporter.setAttrsFromOpNodePrms(pluginDesc, this);

	return OP::VRayNode::PluginResultSuccess;
}

#endif // CGR_HAS_VRAYSCENE
