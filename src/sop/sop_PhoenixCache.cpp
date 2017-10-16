//
// Copyright (c) 2015-2017, Chaos Software Ltd
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

#include <GU/GU_PrimVolume.h>
#include <GU/GU_PrimPacked.h>
#include <GEO/GEO_Primitive.h>

#include <OP/OP_Options.h>
#include <OP/OP_Node.h>
#include <OP/OP_Bundle.h>

using namespace VRayForHoudini;


PRM_Template* SOP::PhxShaderCache::GetPrmTemplate()
{
	static PRM_Template *AttrItems = nullptr;

	if (!AttrItems) {
		AttrItems = Parm::PRMList::loadFromFile(Parm::expandUiPath("CustomPhxShaderCache.ds").c_str(), true);
	}

	return AttrItems;
}

void SOP::PhxShaderCache::updateVRayVolumeGridRefPrim(VRayVolumeGridRef *gridRefPtr, GU_PrimPacked *pack, OP_Context &context, const float t)
{
	if (!gridRefPtr) {
		// if we don't have previous gridref use the new one
		gridRefPtr = UTverify_cast<VRayVolumeGridRef*>(pack->implementation());
	}
	else {
		// if we have previous gridref move its cache to the new one
		VRayVolumeGridRef::VolumeCache &newCachedData = UTverify_cast<VRayVolumeGridRef*>(pack->implementation())->getCachedData();
		newCachedData = std::move(gridRefPtr->getCachedData());
		delete gridRefPtr;

		gridRefPtr = UTverify_cast<VRayVolumeGridRef*>(pack->implementation());
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

	// Check if file contains frame pattern "$F". If it does,
	// then we need to replace it with Phoenix compatible pattern (####).
	UT_String raw, parsed;
	evalStringRaw(raw, "cache_path", 0, t);
	options.setOptionB("literal_cache_path", !raw.findString("$F", false, false));

	options.setOptionF("current_frame", context.getFloatFrame());

	gridRefPtr->update(options);
	pack->setPathAttribute(getFullPath());
}


void SOP::PhxShaderCache::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID   = "CustomPhxShaderCache";
}



OP_ERROR SOP::PhxShaderCache::cookMySop(OP_Context &context)
{
	flags().setTimeDep(true);

	const float t = context.getTime();
	
	const GA_PrimitiveTypeId vrayVolumeGridRefTypeId = GU_PrimPacked::lookupTypeId("VRayVolumeGridRef");

	// find existing VRayVolumeGridRef primitive
	VRayVolumeGridRef* gridRefPtr = nullptr;
	GA_Primitive *prim = nullptr;
	GA_FOR_ALL_PRIMITIVES(gdp, prim) {
		if (prim->getTypeId() == vrayVolumeGridRefTypeId) {
			GU_PrimPacked *primPacked = UTverify_cast<GU_PrimPacked*>(prim);
			VRayVolumeGridRef *oldGridRefPtr = UTverify_cast<VRayVolumeGridRef*>(primPacked->implementation());
			gridRefPtr = new VRayVolumeGridRef(std::move(*oldGridRefPtr));
		}
	}
	
	gdp->stashAll();
	
	// Create a packed primitive
	GU_PrimPacked *pack = GU_PrimPacked::build(*gdp, "VRayVolumeGridRef");
	if (pack) {
		updateVRayVolumeGridRefPrim(gridRefPtr, pack, context, t);
	}
	else {
		addWarning(SOP_MESSAGE, "Can't create packed primitive VRayVolumeGridRef");
	}

	gdp->destroyStashed();

	return error();
}


OP::VRayNode::PluginResult SOP::PhxShaderCache::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	return OP::VRayNode::PluginResultContinue;
}

#endif // CGR_HAS_AUR
