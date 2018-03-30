//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//   https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_phx_channels_utils.h"

const VRayForHoudini::PhxChannelsUtils::ChannelInfo VRayForHoudini::PhxChannelsUtils::chInfo[] = {
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

UT_StringArray VRayForHoudini::PhxChannelsUtils::getPhxChannels(const char* loadPath)
{
	using namespace VRayForHoudini::PhxChannelsUtils;

	UT_StringArray channels;

	int chanIndex = 0;
	int isChannelVector3D = 0;
	char chanName[MAX_CHAN_MAP_LEN];
	while (1 == aurGet3rdPartyChannelName(chanName, MAX_CHAN_MAP_LEN, &isChannelVector3D, loadPath, chanIndex++)) {
		channels.append(chanName);
	}

	return channels;
}