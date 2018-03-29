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

#include "vfh_attr_utils.h"
#include "vfh_prm_templates.h"
#include "vfh_phx_channels_utils.h"

using namespace VRayForHoudini;
using namespace VRayForHoudini::PhxChannelsUtils;
using namespace SOP;

void PhxShaderCache::buildMenuPrmNames(void *data, PRM_Name *choicenames, int listsize, const PRM_SpareData *spare, const PRM_Parm *parm)
{
	SOP_Node *sop = CAST_SOPNODE((OP_Node *)data);
	PhxShaderCache *phxCache = dynamic_cast<PhxShaderCache *>(sop);
	
	if (phxCache)
	{
		UT_StringArray phxChannels = phxCache->getPhxChannels();

		int chCount = phxChannels.size();

		choicenames[0].setTokenAndLabel("0", "None");
		for (size_t idx = 0; idx < phxChannels.size(); ++idx)
		{
			choicenames[idx + 1].setTokenAndLabel(phxChannels[idx], phxChannels[idx]);
		}
		choicenames[phxChannels.size() + 1].setTokenAndLabel(nullptr, nullptr);
	}
	else
	{
		choicenames[0].setTokenAndLabel(nullptr, nullptr);
	}
}

static PRM_SpareData phxShaderCacheSpareData(
	PRM_SpareArgs()
		<< PRM_SpareToken("vray_custom_handling", " ")
		<< PRM_SpareToken("cook_dependent", "1")
);

PRM_Template *PhxShaderCache::getPrmTemplate()
{
	static Parm::PRMList myPrmList;
	if (!myPrmList.empty()) {
		return myPrmList.getPRMTemplate();
	}

	Parm::addPrmTemplateForPlugin("PhxShaderCache", myPrmList);

	myPrmList.switcherBegin("folder3");
	myPrmList.addFolder("Channel Mapping");
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_smoke", "Smoke")
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::buildMenuPrmNames))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_temp",  "Temperature")
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::buildMenuPrmNames))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_fuel",  "Fuel")       
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::buildMenuPrmNames))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_vel_x", "Velocity.x") 
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::buildMenuPrmNames))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_vel_y", "Velocity.y") 
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::buildMenuPrmNames))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_vel_z", "Velocity.z") 
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::buildMenuPrmNames))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_red",   "Red")        
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::buildMenuPrmNames))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_green", "Green")      
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::buildMenuPrmNames))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_blue",  "Blue")       
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::buildMenuPrmNames))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.switcherEnd();

	return myPrmList.getPRMTemplate();
}

PhxShaderCache::PhxShaderCache(OP_Network *parent, const char *name, OP_Operator *entry)
	: NodePackedBase("VRayVolumeGridRef", parent, name, entry)
{
}

UT_StringArray & PhxShaderCache::getPhxChannels() const
{
	// TODO: handle cache_path changes
	if (m_phxChannels.size() != 0) {
		return m_phxChannels;
	}

	UT_StringHolder cachePath = evalCachePath();
	const char* cp = cachePath.buffer();
	return m_phxChannels = PhxChannelsUtils::getPhxChannels(cachePath);
}

void PhxShaderCache::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID   = "PhxShaderCache";
}

void PhxShaderCache::setTimeDependent()
{
	// Check if file contains frame pattern "$F".
	UT_String raw;
	evalStringRaw(raw, "cache_path", 0, 0.0f);

	flags().setTimeDep(raw.findString("$F", false, false));
}

UT_StringHolder PhxShaderCache::evalCachePath(fpreal t /*= 0.f*/) const
{
	// Replace frame number with Phoenix compatible frame pattern.
	UT_String rawLoadPath;
	evalStringRaw(rawLoadPath, "cache_path", 0, t);

		rawLoadPath.changeWord("$F", "####");

		// Expand all the other variables.
		CH_Manager *chanMan = OPgetDirector()->getChannelManager();
		UT_String loadPath;
		fpreal tt = chanMan->getContext().getTime();
		chanMan->expandString(rawLoadPath.buffer(), loadPath, tt);

	return loadPath;
}

void PhxShaderCache::updatePrimitive(const OP_Context &context)
{
	const fpreal t = m_t = context.getTime();

	OP_Options primOptions;

	for (int i = 0; i < getParmList()->getEntries(); ++i) {
		const PRM_Parm &prm = getParm(i);
		primOptions.setOptionFromTemplate(this, prm, *prm.getTemplatePtr(), t);
	}

	const int isTimeDependent = flags().getTimeDep();
	if (isTimeDependent) {
		// Replace frame number with Phoenix compatible frame pattern.
		UT_String rawLoadPath;
		evalStringRaw(rawLoadPath, "cache_path", 0, t);
		rawLoadPath.changeWord("$F", "####");

		// Expand all the other variables.
		CH_Manager *chanMan = OPgetDirector()->getChannelManager();
		UT_String loadPath;
		chanMan->expandString(rawLoadPath.buffer(), loadPath, t);

		primOptions.setOptionS("cache_path", loadPath);
	}

	primOptions.setOptionF("current_frame", isTimeDependent ? context.getFloatFrame() : 0.0);

	UT_StringArray phxChanMap = PhxChannelsUtils::getPhxChannels(primOptions.getOptionS("cache_path"));
	primOptions.setOptionSArray("phx_channel_map", phxChanMap);

	updatePrimitiveFromOptions(primOptions);
}

#endif // CGR_HAS_AUR
