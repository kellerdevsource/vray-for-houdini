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

#ifndef VFH_PHX_CHANNELS_UTILS_H
#define VFH_PHX_CHANNELS_UTILS_H

#include <UT/UT_StringArray.h>

#include <aurloader.h>

#include "systemstuff.h"

namespace VRayForHoudini {
namespace PhxChannelsUtils {

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

/// Info for the channels
static constexpr ChannelInfo chInfo[] = {
	{ "channel_smoke", "Smoke",       GridChannels::ChSm },
	{ "channel_temp",  "Temperature", GridChannels::ChT },
	{ "channel_fuel",  "Fuel",        GridChannels::ChFl },
	{ "channel_vel_x", "Velocity X",  GridChannels::ChVx },
	{ "channel_vel_y", "Velocity Y",  GridChannels::ChVy },
	{ "channel_vel_z", "Velocity Z",  GridChannels::ChVz },
	{ "channel_red",   "Color R",     GridChannels::ChU },
	{ "channel_green", "Color G",     GridChannels::ChV },
	{ "channel_blue",  "Color B",     GridChannels::ChW },
	{ "INVALID",       "INVALID",     GridChannels::ChReserved }
};

/// Number of valid channels.
static constexpr int CHANNEL_COUNT = COUNT_OF(chInfo) - 1;
static constexpr int MAX_CHAN_MAP_LEN = 2048;

UT_StringArray loadChannelsNames(const char* loadPath);

} // namespace VRayForHoudini
} // namespace PhxChannelsUtils

#endif // VFH_PHX_CHANNELS_UTILS_H
