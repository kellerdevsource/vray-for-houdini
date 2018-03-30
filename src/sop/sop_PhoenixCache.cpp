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

void PhxShaderCache::channelsMenuGenerator(void *data, PRM_Name *choicenames, int listsize, const PRM_SpareData *spare, const PRM_Parm *parm)
{
	SOP_Node *sop = CAST_SOPNODE((OP_Node *)data);
	PhxShaderCache *phxCache = dynamic_cast<PhxShaderCache *>(sop);
	
	if (phxCache)
	{
		UT_StringArray phxChannels = phxCache->getPhxChannels();
		
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
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::channelsMenuGenerator))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_temp",  "Temperature")
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::channelsMenuGenerator))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_fuel",  "Fuel")       
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::channelsMenuGenerator))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_vel_x", "Velocity.x") 
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::channelsMenuGenerator))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_vel_y", "Velocity.y") 
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::channelsMenuGenerator))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_vel_z", "Velocity.z") 
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::channelsMenuGenerator))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_red",   "Red")        
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::channelsMenuGenerator))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_green", "Green")      
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::channelsMenuGenerator))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD, "channel_blue",  "Blue")       
		.setChoiceList(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::channelsMenuGenerator))
		.setSpareData(&phxShaderCacheSpareData)
		.setDefault(new PRM_Default(0))
	);
	myPrmList.switcherEnd();

	return myPrmList.getPRMTemplate();
}

PhxShaderCache::PhxShaderCache(OP_Network *parent, const char *name, OP_Operator *entry)
	: NodePackedBase("VRayVolumeGridRef", parent, name, entry)
	, m_pathChanged(true)
	, m_phxChannels()
{}

UT_StringArray & PhxShaderCache::getPhxChannels(fpreal t /*= -1.f*/) const
{
	// Channels depend on the file not the time
	if (!m_pathChanged 
		|| m_phxChannels.size() != 0) {
		return m_phxChannels;
	}
	
	// Default value is current time
	t = (t >= 0.f) ? t :
		OPgetDirector()->getTime();

	UT_StringHolder cachePath = evalCachePath(t);
	m_phxChannels = PhxChannelsUtils::getPhxChannels(cachePath);

	m_pathChanged = false;
	return m_phxChannels;
}

bool PhxShaderCache::isSamePath(const OP_Options &options) const
{
	if (!m_primOptions.hasOption("cache_path") || !options.hasOption("cache_path")) {
		return false;
	}

	UT_StringHolder oldPath, newPath;
	m_primOptions.getOptionS("cache_path", oldPath);
	options.getOptionS("cache_path", newPath);

	return oldPath == newPath;
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

UT_StringHolder PhxShaderCache::evalCachePath(fpreal t) const
{
	UT_String rawLoadPath;
	evalStringRaw(rawLoadPath, "cache_path", 0, t);
	
	// Replace frame number with Phoenix compatible frame pattern.
	rawLoadPath.changeWord("$F", "####");

	// Expand all the other variables.
	CH_Manager *chanMan = OPgetDirector()->getChannelManager();
	UT_String loadPath;
	chanMan->expandString(rawLoadPath.buffer(), loadPath, t);

	return loadPath;
}

void PhxShaderCache::updatePrimitive(const OP_Context &context)
{
	const fpreal t = context.getTime();

	OP_Options primOptions;

	for (int i = 0; i < getParmList()->getEntries(); ++i) {
		const PRM_Parm &prm = getParm(i);
		primOptions.setOptionFromTemplate(this, prm, *prm.getTemplatePtr(), t);
	}

	const int isTimeDependent = flags().getTimeDep();
	if (isTimeDependent) {
		UT_StringHolder cachePath = evalCachePath(t);
		primOptions.setOptionS("cache_path", cachePath);
	}

	if (!isSamePath(primOptions)) {
		m_pathChanged = true;
	}

	primOptions.setOptionF("current_frame", isTimeDependent ? context.getFloatFrame() : 0.0);

	UT_StringArray phxChanMap = getPhxChannels(t);
	primOptions.setOptionSArray("phx_channel_map", phxChanMap);

	updatePrimitiveFromOptions(primOptions);
}

#endif // CGR_HAS_AUR
