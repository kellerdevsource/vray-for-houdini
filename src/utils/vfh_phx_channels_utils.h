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

extern const ChannelInfo chInfo[];

/// Number of valid channels.
const int CHANNEL_COUNT = 9;
const int MAX_CHAN_MAP_LEN = 2048;

UT_StringArray getPhxChannels(const char* loadPath);

} // namespace VRayForHoudini
} // namespace PhxChannelsUtils

#endif // VFH_PHX_CHANNELS_UTILS_H
