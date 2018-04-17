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

#include <QString>
#include <QRegularExpression>
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

/// Loads the channel names for @param cachePath
UT_StringArray loadChannelsNames(const char* cachePath);

/// The default channels mapping for the @param cachePath
UT_String loadDefaultMapping(const char *cachePath);

UT_String buildChannelsMapping(const char *cachePath, const int mappedChannels[MAX_CHAN_MAP_LEN]);

} // namespace PhxChannelsUtils


namespace PhxAnimUtils {

// These *must* match Phoenix values.
enum class AnimMode {
	standard = 0,
	directIndex = 1,
	loop = 2,
};

/// The pattern that Phoenix FD replaces with the current frame
static const QRegularExpression phxFramePattern("#+");
/// The pattern that Houdini replaces with the current frame
static const QRegularExpression houFramePattern("\\$F[0-9]+");

/// This is the formula used by Phoenix FD to get the frame number of the exported cache based on the animation options
/// @param frame The frame from Houdini UI
/// @param max_length Play Length
/// @param play_speed Play Speed
/// @param anim_mode Playback Mode, 0 - Linear, 1 - Frame Index, 2 - Loop
/// @param t2f Direct time to frame transformation.
/// @param play_at Play Start
/// @param load_nearest If there is no cache file with the desired frame number, the nearest cache is found and loaded.
/// @param read_offset Cache Start
/// @retval The frame number that is needed
int evalCacheFrame(fpreal frame, exint max_length, fpreal play_speed, exint anim_mode, fpreal t2f, exint play_at, exint load_nearest, exint read_offset);

/// From a cachePath with Phoenix frame pattern to a real file cachePath
/// for example: from './vdb/hou-####.vdb' './vdb/hou-0013.vdb'
/// @param cachePath Path with some '#'s in it
/// @param frame The number to replace the '#'s with
void evalPhxPattern(QString &cachePath, exint frame);

/// Replaces Houdini current frame pattern($F) with Phoenix FD current frame pattern(##) if found
/// @param cachePath Path with $F in it
void hou2PhxPattern(QString& cachePath);

} // namespace PhxAnimUtils
} // namespace VRayForHoudini

#endif // VFH_PHX_CHANNELS_UTILS_H
