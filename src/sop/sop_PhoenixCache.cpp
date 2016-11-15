//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifdef CGR_HAS_AUR

#include "sop_PhoenixCache.h"
#include "vfh_prm_templates.h"
#include "gu_volumegridref.h"

#include <GU/GU_PrimVolume.h>
#include <GU/GU_PrimPacked.h>
#include <GEO/GEO_Primitive.h>

#include <OP/OP_Options.h>
#include <OP/OP_Node.h>
#include <OP/OP_Bundle.h>

using namespace VRayForHoudini;

static PRM_Template * AttrItems = nullptr;

PRM_Template* SOP::PhxShaderCache::GetPrmTemplate()
{
	if (!AttrItems) {
		AttrItems = Parm::PRMList::loadFromFile(Parm::expandUiPath("CustomPhxShaderCache.ds").c_str(), true);
	}

	return AttrItems;
}


void SOP::PhxShaderCache::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "CustomPhxShaderCache";
}


OP_NodeFlags &SOP::PhxShaderCache::flags()
{
	OP_NodeFlags &flags = SOP_Node::flags();
	flags.setTimeDep(true);
	return flags;
}


OP_ERROR SOP::PhxShaderCache::cookMySop(OP_Context &context)
{
	Log::getLog().info("%s cookMySop(%.3f)",
					   getName().buffer(), context.getTime());

	gdp->stashAll();

	const float t = context.getTime();

	// Create a packed primitive
	GU_PrimPacked *pack = GU_PrimPacked::build(*gdp, "VRayVolumeGridRef");
	auto gridRefPtr = UTverify_cast<VRayVolumeGridRef*>(pack->implementation());
	if (NOT(pack)) {
		addWarning(SOP_MESSAGE, "Can't create packed primitive VRayVolumeGridRef");
		return error();
	}

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

	options.setOptionF("current_frame", context.getFloatFrame());

	gridRefPtr->update(options);
	pack->setPathAttribute(getFullPath());

	gdp->destroyStashed();

#if UT_MAJOR_VERSION_INT < 14
	gdp->notifyCache(GU_CACHE_ALL);
#endif

	return error();
}


OP::VRayNode::PluginResult SOP::PhxShaderCache::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	return OP::VRayNode::PluginResultContinue;
}

#endif // CGR_HAS_AUR
