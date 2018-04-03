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

UT_StringArray VRayForHoudini::PhxChannelsUtils::loadChannelsNames(const char* loadPath)
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