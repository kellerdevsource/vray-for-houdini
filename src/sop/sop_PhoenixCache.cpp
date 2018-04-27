//
// Copyright (c) 2015-2018, Chaos Software Ltd
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
#include "vfh_phx_utils.h"

using namespace VRayForHoudini;
using namespace SOP;

void PhxShaderCache::channelsMenuGenerator(void *data, PRM_Name *choicenames, int listsize, const PRM_SpareData *spare, const PRM_Parm *parm)
{
	SOP_Node *sop = CAST_SOPNODE((OP_Node *)data);
	PhxShaderCache *phxCache = dynamic_cast<PhxShaderCache *>(sop);
	
	if (!phxCache) {
		choicenames[0].setTokenAndLabel(nullptr, nullptr);
		return;
	}

	UT_StringArray phxChannels = phxCache->getChannelsNames();
		
	choicenames[0].setTokenAndLabel("0", "None");
	for (int idx = 0; idx < phxChannels.size(); ++idx) {
		choicenames[idx + 1].setTokenAndLabel(phxChannels[idx], phxChannels[idx]);
	}
	choicenames[phxChannels.size() + 1].setTokenAndLabel(nullptr, nullptr);
}

PRM_Template *PhxShaderCache::getPrmTemplate()
{
	static PRM_Template* myPrmList = nullptr;
	if (myPrmList) {
		return myPrmList;
	}

	static PRM_ChoiceList channelChoices(PRM_CHOICELIST_SINGLE, PhxShaderCache::channelsMenuGenerator);

	myPrmList = Parm::getPrmTemplate("PhxShaderCache");

	PRM_Template* prmIt = myPrmList;
	while (prmIt && prmIt->getType() != PRM_LIST_TERMINATOR) {
		if (prmIt->getType() == PRM_ORD) {
			// Append choices to channel parms
			for (int i = 0; i < PhxChannelsUtils::CHANNEL_COUNT; ++i) {
				if (vutils_strcmp(prmIt->getToken(), PhxChannelsUtils::chInfo[i].propName) == 0) {
					prmIt->setChoiceListPtr(&channelChoices);
				}
			}
		}

		++prmIt;
	}

	return myPrmList;
}

PhxShaderCache::PhxShaderCache(OP_Network *parent, const char *name, OP_Operator *entry)
	: NodePackedBase("VRayVolumeGridRef", parent, name, entry)
	, m_pathChanged(true)
	, m_phxChannels()
{}

UT_StringArray & PhxShaderCache::getChannelsNames(fpreal t /*= -1.f*/) const
{
	// Channels depend on the file not the time
	if (!m_pathChanged || m_phxChannels.size() != 0) {
		return m_phxChannels;
	}
	
	// Default value is current time
	t = (t >= 0.f) ? t : OPgetDirector()->getTime();

	UT_StringHolder cachePath = evalCachePath(t, false);
	m_phxChannels = PhxChannelsUtils::loadChannelsNames(cachePath);

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

int PhxShaderCache::evalCacheFrame(fpreal t) const
{
	return PhxAnimUtils::evalCacheFrame(
		OPgetDirector()->getChannelManager()->getFrame(t),
		evalInt("max_length", 0, t),
		evalFloat("play_speed", 0, t),
		evalInt("anim_mode", 0, t),
		evalFloat("t2f", 0, t),
		evalInt("play_at", 0, t),
		evalInt("load_nearest", 0, t),
		evalInt("read_offset", 0, t)
	);
}

UT_StringHolder PhxShaderCache::evalCachePath(fpreal t, bool sequencePath) const
{
	UT_String rawLoadPath;
	evalStringRaw(rawLoadPath, "cache_path", 0, t);

	QString rawLoadPathQtS(rawLoadPath);
	PhxAnimUtils::hou2PhxPattern(rawLoadPathQtS);

	if (!sequencePath) {
		PhxAnimUtils::evalPhxPattern(rawLoadPathQtS, evalCacheFrame(t));
	}

	// Expand all the other variables.
	CH_Manager *chanMan = OPgetDirector()->getChannelManager();
	UT_String loadPath;
	chanMan->expandString(_toChar(rawLoadPathQtS), loadPath, t);
	

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
		UT_StringHolder cachePath = evalCachePath(t, true);
		primOptions.setOptionS("cache_path", cachePath);
	}

	if (!isSamePath(primOptions)) {
		m_pathChanged = true;
	}

	primOptions.setOptionF("current_frame", isTimeDependent ? context.getFloatFrame() : 0.0);

	UT_StringArray phxChanMap = getChannelsNames(t);
	primOptions.setOptionSArray("phx_channel_map", phxChanMap);

	updatePrimitiveFromOptions(primOptions);
}

#endif // CGR_HAS_AUR
