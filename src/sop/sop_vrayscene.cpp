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
#include "gu_vraysceneref.h"

#include <OP/OP_Options.h>

using namespace VRayForHoudini;

void SOP::VRayScene::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID   = "VRayScene";
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

	const bool is_animated = evalInt("anim_type", 0, t) != 3;
	flags().setTimeDep(is_animated);

	GU_PrimPacked *pack = GU_PrimPacked::build(*gdp, "VRaySceneRef");
	if (!pack) {
		addWarning(SOP_MESSAGE, "Can't create packed primitive VRaySceneRef");
	}
	else {
		// Set the location of the packed primitive point.
		const UT_Vector3 pivot(0.0, 0.0, 0.0);
		pack->setPivot(pivot);
		gdp->setPos3(pack->getPointOffset(0), pivot);

		// Set the options on the primitive
		OP_Options options;
		for (int i = 0; i < getParmList()->getEntries(); ++i) {
			const PRM_Parm &prm = getParm(i);
			options.setOptionFromTemplate(this, prm, *prm.getTemplatePtr(), t);
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

		options.setOptionS("plugin_mapping", pluginMappings);

		pack->implementation()->update(options);

		pack->setPathAttribute(getFullPath());
	}

	gdp->destroyStashed();

	return error();
}

#endif // CGR_HAS_VRAYSCENE
