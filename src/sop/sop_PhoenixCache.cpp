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

#include <UT/UT_String.h>

#include <aurloader.h>

using namespace VRayForHoudini;
using namespace SOP;


/// Info for a single channel, could be casted to int to obtain the channel type for PHX
struct ChannelInfo {
	/// The name of the property in the plugin.
	const char *propName;

	/// The displayed name for the channel.
	const char *displayName;

	/// Value of the channel for PHX.
	GridChannels::Enum type;

	/// Get channel type.
	explicit operator int() const {
		return static_cast<int>(type);
	}
};

static const ChannelInfo chInfo[] = {
	{ "channel_smoke", "Smoke",       GridChannels::ChSm },
	{ "channel_temp",  "Temperature", GridChannels::ChT },
	{ "channel_fuel",  "Fuel",        GridChannels::ChFl },
	{ "channel_vel_x", "Velocity X",  GridChannels::ChVx },
	{ "channel_vel_y", "Velocity Y",  GridChannels::ChVy },
	{ "channel_vel_z", "Velocity Z",  GridChannels::ChVz },
	{ "channel_red",   "Color R",     GridChannels::ChU },
	{ "channel_green", "Color G",     GridChannels::ChV },
	{ "channel_blue",  "Color B",     GridChannels::ChW },
	{ "INVALID",       "INVALID",     GridChannels::ChReserved },
};

/// Number of valid channels.
static const int CHANNEL_COUNT = (sizeof(chInfo) / sizeof(chInfo[0])) - 1;
static const int MAX_CHAN_MAP_LEN = 2048;

static UT_StringArray getPhxChannels(const char* loadPath)
{
	UT_StringArray channels;

	int chanIndex = 0;
	int isChannelVector3D = 0;
	char chanName[MAX_CHAN_MAP_LEN];

	while (1 == aurGet3rdPartyChannelName(chanName, MAX_CHAN_MAP_LEN, &isChannelVector3D, loadPath, chanIndex++)) {
		channels.append(chanName);
	}

	return channels;
}

static bool isSentinelPrmName(const PRM_Name &prmName) {
	return prmName.getLabel() == NULL && prmName.getToken() == NULL && prmName.getExpressionFlag() == NULL;
}

void PhxShaderCache::buildMenuPrmNames(void *data, PRM_Name *choicenames, int listsize, const PRM_SpareData *spare, const PRM_Parm *parm)
{
	SOP_Node *sop = CAST_SOPNODE((OP_Node *)data);
	PhxShaderCache *phxCache = dynamic_cast<PhxShaderCache *>(sop);

	if (phxCache)
	{
		UT_StringHolder cachePath = phxCache->m_primOptions.getOptionS("cache_path");
		UT_StringArray phxChannels = getPhxChannels(cachePath);

		for (size_t idx = 0; idx < phxChannels.size(); ++idx)
		{
			choicenames[idx].setTokenAndLabel(phxChannels[idx], phxChannels[idx]);
		}
		choicenames[phxChannels.size()].setTokenAndLabel(nullptr, nullptr);
	}
	else
	{
		choicenames[0].setTokenAndLabel(nullptr, nullptr);
	}
}

PRM_Template *PhxShaderCache::getPrmTemplate()
{
	PRM_Template *prmTemplate = Parm::getPrmTemplate("PhxShaderCache");
	while (prmTemplate && prmTemplate->getType() != PRM_LIST_TERMINATOR) {
		if (vutils_strcmp(prmTemplate->getToken(), "channel_smoke")    == 0
			|| vutils_strcmp(prmTemplate->getToken(), "channel_temp")  == 0
			|| vutils_strcmp(prmTemplate->getToken(), "channel_fuel")  == 0
			|| vutils_strcmp(prmTemplate->getToken(), "channel_vel_x") == 0
			|| vutils_strcmp(prmTemplate->getToken(), "channel_vel_y") == 0
			|| vutils_strcmp(prmTemplate->getToken(), "channel_vel_z") == 0
			|| vutils_strcmp(prmTemplate->getToken(), "channel_red")   == 0
			|| vutils_strcmp(prmTemplate->getToken(), "channel_green") == 0
			|| vutils_strcmp(prmTemplate->getToken(), "channel_blue")  == 0) {

			prmTemplate->setChoiceListPtr(new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, PhxShaderCache::buildMenuPrmNames));
		}

		++prmTemplate;
	}

	return prmTemplate;
}

PhxShaderCache::PhxShaderCache(OP_Network *parent, const char *name, OP_Operator *entry)
	: NodePackedBase("VRayVolumeGridRef", parent, name, entry)
	, m_channelNames(new PRM_Name[1])
{
	// sentinel
	m_channelNames[0] = PRM_Name(NULL);
}

VRayForHoudini::SOP::PhxShaderCache::~PhxShaderCache()
{
	delete m_channelNames;
}

PRM_Name *VRayForHoudini::SOP::PhxShaderCache::getChannelNames() const
{
	// check if list is empty
	if (isSentinelPrmName(m_channelNames[0])) {
		if (m_primOptions.hasOption("cache_path")) {
			UT_StringHolder cachePath = m_primOptions.getOptionS("cache_path");
			UT_StringArray phxChannels = getPhxChannels(cachePath);

			m_channelNames = new PRM_Name[phxChannels.size() + 1];
			for (size_t idx = 0; idx < phxChannels.size(); ++idx)
			{
				m_channelNames[idx].setTokenAndLabel(phxChannels[idx], phxChannels[idx]);
			}
			m_channelNames[phxChannels.size()].setTokenAndLabel(NULL, NULL);
		}
	}

	return m_channelNames;
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

	UT_StringArray phxChanMap = getPhxChannels(primOptions.getOptionS("cache_path"));
	primOptions.setOptionSArray("phx_channel_map", phxChanMap);

	updatePrimitiveFromOptions(primOptions);
}

#endif // CGR_HAS_AUR
