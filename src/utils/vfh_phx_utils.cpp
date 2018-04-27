//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//   https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_phx_utils.h"

#ifdef CGR_HAS_AUR

#include <utils.h>
#include <misc.h>

#include <vassert.h>

UT_StringArray VRayForHoudini::PhxChannelsUtils::loadChannelsNames(const char* cachePath)
{
	UT_StringArray channels;

	int chanIndex = 0;
	int isChannelVector3D = 0;
	char chanName[MAX_CHAN_MAP_LEN];
	while (1 == aurGet3rdPartyChannelName(chanName, MAX_CHAN_MAP_LEN, &isChannelVector3D, cachePath, chanIndex++)) {
		channels.append(chanName);
	}

	return channels;
}

UT_String VRayForHoudini::PhxChannelsUtils::loadDefaultMapping(const char *cachePath)
{
	UT_String defMapping("");

	char buff[MAX_CHAN_MAP_LEN];
	if (1 == aurGenerateDefaultChannelMappings(buff, MAX_CHAN_MAP_LEN, cachePath)) {
		defMapping = UT_String(buff, true);
	}

	return defMapping;
}

UT_String VRayForHoudini::PhxChannelsUtils::buildChannelsMapping(const char *cachePath, const int mappedChannels[MAX_CHAN_MAP_LEN])
{
	UT_StringArray channels = loadChannelsNames(cachePath);

	// will hold names so we can use pointers to them
	std::vector<const char*> names;
	std::vector<int> ids;
	// save mapped channels with their name and id
	for (int c = 0; c < PhxChannelsUtils::CHANNEL_COUNT; ++c) {
		const PhxChannelsUtils::ChannelInfo &chan = PhxChannelsUtils::chInfo[c];

		const int64 res = mappedChannels[c];
		// if a valid value is given 
		if (res >= 0 && res < channels.size()) {
			UT_StringHolder name(channels(res));
			if (name != "" && name != "0") {
				names.push_back(name);
				ids.push_back(static_cast<int>(chan));
			}
		}
	}

	UT_String usrchmap = "";

	char buffer[PhxChannelsUtils::MAX_CHAN_MAP_LEN] = { 0, };
	if (1 == aurComposeChannelMappingsString(buffer, PhxChannelsUtils::MAX_CHAN_MAP_LEN, ids.data(), names.data(), names.size())) {
		usrchmap = buffer;
	}

	// user made no mappings - get default
	if (usrchmap.equal("")) {
		usrchmap = PhxChannelsUtils::loadDefaultMapping(cachePath);
	}

	return usrchmap;
}

int VRayForHoudini::PhxAnimUtils::evalCacheFrame(fpreal frame, exint max_length, fpreal play_speed, exint anim_mode, fpreal t2f, exint play_at, exint load_nearest, exint read_offset)
{
	const exint animLen = max_length;
	const float fractionalLen = animLen * play_speed;

	switch (AnimMode(anim_mode)) {
	case AnimMode::directIndex: {
		frame = t2f;
		break;
	}
	case AnimMode::standard: {
		frame = play_speed * (frame - play_at);

		if (fractionalLen > 1e-4f) {
			if (frame < 0.f || frame > fractionalLen) {
				if (load_nearest) {
					frame = VUtils::clamp(frame, 0.f, fractionalLen);
				} else {
					frame = INT_MIN;
				}
			}
		}

		frame += read_offset;
		break;
	}
	case AnimMode::loop: {
		frame = play_speed * (frame - play_at);

		if (fractionalLen > 1e-4f) {
			while (frame < 0) {
				frame += fractionalLen;
			}
			while (frame > fractionalLen) {
				frame -= fractionalLen;
			}
		}

		frame += read_offset;
		break;
	}
	default:
		break;
	}

	return frame;
}

void VRayForHoudini::PhxAnimUtils::evalPhxPattern(QString &cachePath, exint frame)
{
	QString cacheFrameS = QString::number(frame);
	QRegularExpressionMatch m = phxFramePattern.match(cachePath);
	if (m.hasMatch()) {
		QString matched = m.captured();
		cacheFrameS = cacheFrameS.rightJustified(matched.size(), '0');
		cachePath.replace(phxFramePattern, cacheFrameS);
	}
}

void VRayForHoudini::PhxAnimUtils::hou2PhxPattern(QString &cachePath)
{
	QRegularExpressionMatch m = houFramePattern.match(cachePath);
	if (m.hasMatch()) {
		int padding = m.captured().remove(0, 2).toInt();
		QString phxPattern(padding, '#');
		cachePath.replace(houFramePattern, phxPattern);
	}
}

#endif // CGR_HAS_AUR
