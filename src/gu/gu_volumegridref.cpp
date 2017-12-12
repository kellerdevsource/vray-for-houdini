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

#include "vfh_log.h"
#include "gu_volumegridref.h"

#include <GU/GU_PrimVolume.h>

#include <aurinterface.h>
#include <aurloader.h>

#include <chrono>
#include <QScopedPointer>

namespace {

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

const ChannelInfo chInfo[] = {
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
const int CHANNEL_COUNT = (sizeof(chInfo) / sizeof(chInfo[0])) - 1;

#if 0
/// Get ChannelInfo from it's type
const ChannelInfo &fromType(int type) {
	for (int c = 0; c < CHANNEL_COUNT; ++c) {
		if (static_cast<int>(chInfo[c]) == type) {
			return chInfo[c];
		}
	}
	return chInfo[CHANNEL_COUNT];
}

/// Get ChannelInfo from it's name
const ChannelInfo &fromPropName(const char *name) {
	for (int c = 0; c < CHANNEL_COUNT; ++c) {
		if (!strcmp(name, chInfo[c].propName)) {
			return chInfo[c];
		}
	}
	return chInfo[CHANNEL_COUNT - 1];
}
#endif

static UT_Matrix4F getCacheTm(VRayForHoudini::VRayVolumeGridRef::CachePtr cache, bool flipYZ)
{
	if (!cache) {
		return UT_Matrix4F(1.f);
	}

	float flTransform[12];
	cache->GetObject2GridTransform(flTransform);

	// houdini translations is in last row instead of last col
	UT_Matrix4F m4(
		flTransform[0], flTransform[1], flTransform[2], 0.f,
		flTransform[3], flTransform[4], flTransform[5], 0.f,
		flTransform[6], flTransform[7], flTransform[8], 0.f,
		flTransform[9], flTransform[10], flTransform[11], 1.0
	);

	if (flipYZ) {
		for (int c = 0; c < 4; ++c) {
			const fpreal32 old = m4(1, c);
			m4(1, c) = -m4(2, c);
			m4(2, c) = old;
		}
	}

	m4.invert();

	int gridDimensions[3];
	cache->GetDim(gridDimensions);

	// Houdini's simulations are 2x2x2 box from (-1,-1,-1) to (1,1,1).
	// This will transform Houdini box to grid dimentions.
	UT_Matrix4F hou2phx(1.f);

	for (int c = 0; c < 3; ++c) {
		hou2phx(3, c) = gridDimensions[c] * 0.5f;
		hou2phx(c, c) = gridDimensions[c] * 0.5f;
	}

	return hou2phx * m4;
}

}

using namespace VRayForHoudini;

bool VolumeCacheKey::isValid() const
{
	return !path.empty();
}

static const int MAX_CHAN_MAP_LEN = 2048;
static const int MAX_RESOLUTION = 255;

static GA_PrimitiveTypeId theTypeId(-1);
static VRayBaseRefFactory<VRayVolumeGridRef> theFactory("VRayVolumeGridRef");

static std::string getDefaultMapping(const char *cachePath) {
	char buff[MAX_CHAN_MAP_LEN];
	if (1 == aurGenerateDefaultChannelMappings(buff, MAX_CHAN_MAP_LEN, cachePath)) {
		return std::string(buff);
	}

	return "";
}

struct IAurPointerDeleter {
    static void cleanup(IAur *pointer) {
		deleteIAur(pointer);
	}
};

void VRayVolumeGridRef::install(GA_PrimitiveFactory *primfactory)
{
	theTypeId = theFactory.install(*primfactory, theFactory);

	SYSconst_cast(theFactory.typeDef()).setHasLocalTransform(true);
}

void VRayVolumeGridRef::fetchData(const VolumeCacheKey &key, VolumeCacheData &data)
{
	fetchDataMaxVox(key, data, -1, false);
}

void VRayVolumeGridRef::fetchDataMaxVox(const VolumeCacheKey &key, VolumeCacheData &data, const i64 voxelCount, const bool infoOnly)
{
	typedef std::chrono::high_resolution_clock::time_point time_point;
	typedef std::chrono::high_resolution_clock time_clock;
	typedef std::chrono::milliseconds milliseconds;

	const time_point tStart = time_clock::now();
	IAur *aurPtr;
	if (key.map.empty()) {
		aurPtr = newIAurMaxVox(key.path.c_str(), voxelCount, infoOnly);
	}
	else {
		aurPtr = newIAurWithChannelsMappingMaxVox(key.path.c_str(), key.map.c_str(), voxelCount, infoOnly);
	}
	data.aurPtr = VRayVolumeGridRef::CachePtr(aurPtr, [](IAur *ptr) { deleteIAur(ptr); });
	const time_point tEndCache = time_clock::now();

	memset(data.dataRange.data(), 0, DataRangeMapSize);

	if (data.aurPtr) {
		Log::getLog().debug("Loading cache took %dms", static_cast<int>(std::chrono::duration_cast<milliseconds>(tEndCache - tStart).count()));
	}
	else {
		Log::getLog().error("Failed to load cache \"%s\"", key.path.c_str());
		return;
	}

	GU_Detail *gdp = new GU_Detail();

	int gridDimensions[3];
	data.aurPtr->GetDim(gridDimensions);

	// TODO: maybe the flip YZ flag should be part of the key?
	// what if there are two instances having the same cache but one has the flag set and the other does not?
	const UT_Matrix4F tm = getCacheTm(data.aurPtr, key.flipYZ);

	// load each channel from the cache file
	for (int c = 0; c < CHANNEL_COUNT; ++c) {
		const ChannelInfo &chan = chInfo[c];
		if (!data.aurPtr->ChannelPresent(chan.type)) {
			continue;
		}

		// Get data range of channel.
		const float *grid = data.aurPtr->ExpandChannel(chan.type);
		const i64 cacheVoxelsCount = gridDimensions[0] * gridDimensions[1] * gridDimensions[2];

		const std::pair<const float *, const float *> dataRange = std::minmax_element(grid, grid + cacheVoxelsCount);
		data.dataRange[chan.type].min = *dataRange.first;
		data.dataRange[chan.type].max = *dataRange.second;

		// Do not save invisible channels.
		const GEO_VolumeVis visType = chan.type == GridChannels::ChSm
			? GEO_VOLUMEVIS_SMOKE
			: GEO_VOLUMEVIS_INVISIBLE;
		if (visType == GEO_VOLUMEVIS_INVISIBLE) {
			continue;
		}

		GU_PrimVolume *volumeGdp = static_cast<GU_PrimVolume *>(GU_PrimVolume::build(gdp));
		volumeGdp->setVisualization(visType, volumeGdp->getVisIso(), volumeGdp->getVisDensity());

		UT_VoxelArrayWriteHandleF voxelHandle = volumeGdp->getVoxelWriteHandle();
		voxelHandle->size(gridDimensions[0], gridDimensions[1], gridDimensions[2]);

		const time_point tStartExtract = time_clock::now();

		voxelHandle->extractFromFlattened(grid, gridDimensions[0], gridDimensions[1] * gridDimensions[0]);

		const time_point tEndExtract = time_clock::now();
		const int extractTime = std::chrono::duration_cast<milliseconds>(tEndExtract - tStartExtract).count();

		Log::getLog().debug("Extracting channel \"%s\" took %dms", chan.displayName, extractTime);

		volumeGdp->setTransform4(tm);
	}

	data.detailHandle.allocateAndSet(gdp);
}

VRayVolumeGridRef::VRayVolumeGridRef()
	: VRayVolumeGridRefOptions()
	, m_channelDirty(false)
	, m_doFrameReplace(false)
{
	memset(getChannelDataRanges().data(), 0, DataRangeMapSize);
	initDataCache();
}

GA_PrimitiveTypeId VRayVolumeGridRef::typeId()
{
	return theTypeId;
}

VRayVolumeGridRef::VRayVolumeGridRef(const VRayVolumeGridRef &src)
	: VRayVolumeGridRefOptions(src)
	, m_channelDirty(false)
	, m_doFrameReplace(src.m_doFrameReplace)
{
	initDataCache();
}

VRayVolumeGridRef::VRayVolumeGridRef(VRayVolumeGridRef &&src) noexcept
	: VRayVolumeGridRefOptions(std::move(src))
	, m_dataCache(std::move(src.m_dataCache))
	, m_channelDirty(std::move(src.m_channelDirty))
	, m_doFrameReplace(std::move(src.m_doFrameReplace))
{}

VRayVolumeGridRef::~VRayVolumeGridRef()
{}

void VRayVolumeGridRef::initDataCache() const
{
	m_dataCache.setFetchCallback(fetchData);

	// We don't need the evict callback, because the data is in RAII objects and will be freed when evicted from cache,
	// so just print info.
	m_dataCache.setEvictCallback([](const VolumeCacheKey &key, VolumeCacheData&) {
		Log::getLog().debug("Removing \"%s\" from cache", key.path.c_str());
	});
}

VolumeCacheKey VRayVolumeGridRef::genKey() const
{
	const char *path = getCurrentCachePath();
	const char *map = getUsrchmap();

	return VolumeCacheKey(path, map ? map : "", getFlipYz());
}

GU_PackedFactory* VRayVolumeGridRef::getFactory() const
{
	return &theFactory;
}

VRayVolumeGridRef::VolumeCacheData &VRayVolumeGridRef::getCache(const VolumeCacheKey &key) const
{
	if (getResolution() == MAX_RESOLUTION &&
		getFullCacheVoxelCount() != -1)
	{
		m_currentData = m_dataCache[key];
	}
	else {
		fetchDataMaxVox(key, m_currentData, getCurrentCacheVoxelCount(), false);
	}

	return m_currentData;
}

UT_Matrix4F VRayVolumeGridRef::toWorldTm(CachePtr cache) const
{
	return getCacheTm(cache, getFlipYz());
}

bool VRayVolumeGridRef::unpack(GU_Detail&) const
{
	return false;
}

void VRayVolumeGridRef::detailRebuild()
{
	using namespace std;
	using namespace chrono;

	memset(getChannelDataRanges().data(), 0, DataRangeMapSize);

	const VolumeCacheKey &key = genKey();
	if (!key.isValid()) {
		return;
	}

	VolumeCacheData &data = getCache(key);
	if (!data.aurPtr) {
		return;
	}

	m_detail = data.detailHandle;

	memcpy(getChannelDataRanges().data(), data.dataRange.data(), DataRangeMapSize);
}

UT_StringArray VRayVolumeGridRef::getCacheChannels() const {
	UT_StringArray channels;
	if (!m_channelDirty) {
		getPhxChannelMap(channels);
		return channels;
	}

	int chanIndex = 0, isChannelVector3D;
	char chanName[MAX_CHAN_MAP_LEN];
	while (1 == aurGet3rdPartyChannelName(chanName, MAX_CHAN_MAP_LEN, &isChannelVector3D, getCurrentCachePath(), chanIndex++)) {
		channels.append(chanName);
	}

	SYSconst_cast(this)->m_channelDirty = false;
	SYSconst_cast(this)->setPhxChannelMap(channels);
	return channels;
}

VRayVolumeGridRef::VolumeCache &VRayVolumeGridRef::getCachedData() const
{
	return m_dataCache;
}

void VRayVolumeGridRef::buildMapping()
{
	const char * path = getCurrentCachePath();
	if (!path) {
		return;
	}

	UT_String chanMap;
	// aur cache has no need for mappings
	if (UT_String(path).endsWith(".aur")) {
		chanMap = "";
		setPhxChannelMap(UT_StringArray());
	}
	else {
		UT_StringArray channels = getCacheChannels();

		// will hold names so we can use pointers to them
		std::vector<UT_String> names;
		std::vector<int> ids;
		for (int c = 0; c < CHANNEL_COUNT; ++c) {
			const ChannelInfo &chan = chInfo[c];

			UT_String value(UT_String::ALWAYS_DEEP);
			const int64 res = m_options.hasOption(chan.propName) ? m_options.getOptionI(chan.propName) - 1 : -1;
			if (res >= 0 && res < channels.size()) {
				value = channels(res);
				if (value != "" && value != "0") {
					names.push_back(value);
					ids.push_back(static_cast<int>(chan));
				}
			}
		}

		const char *inputNames[CHANNEL_COUNT] = { 0 };
		for (int c = 0; c < names.size(); ++c) {
			inputNames[c] = names[c].c_str();
		}

		char usrchmap[MAX_CHAN_MAP_LEN] = { 0, };
		if (1 == aurComposeChannelMappingsString(usrchmap, MAX_CHAN_MAP_LEN, ids.data(), const_cast<char * const *>(inputNames), names.size())) {
			chanMap = usrchmap;
		}

		// user made no mappings - get default
		if (chanMap.equal("")) {
			chanMap = getDefaultMapping(path);
		}
	}

	if (getUsrchmap()) {
		setUsrchmap(chanMap);
	}
}

int VRayVolumeGridRef::getCurrentCacheFrame() const
{
	float frame = getCurrentFrame();
	const float animLen = getMaxLength();
	const float fractionalLen = animLen * getPlaySpeed();

	switch (getAnimMode()) {
		case directIndex: {
			frame = getT2F();
			break;
		}
		case standard: {
			frame = getPlaySpeed() * (frame - getPlayAt());

			if (fractionalLen > 1e-4f) {
				if (frame < 0.f || frame > fractionalLen) {
					if (getLoadNearest()) {
						// clamp frame in [0, animLen]
						frame = std::max(0.f, std::min(fractionalLen, frame));
					}
					else {
						frame = INT_MIN;
					}
				}
			}

			frame += getReadOffset();
			break;
		}
		case loop: {
			frame = getPlaySpeed() * (frame - getPlayAt());

			if (fractionalLen > 1e-4f) {
				while (frame < 0) {
					frame += fractionalLen;
				}
				while (frame > fractionalLen) {
					frame -= fractionalLen;
				}
			}

			frame += getReadOffset();
			break;
		}
		default:
			break;
	}

	return frame;
}

int VRayVolumeGridRef::getResolution() const
{
	const int resMode = getResMode();
	const int previewRes = getPreviewRes();

	return resMode == 0 ? MAX_RESOLUTION : previewRes;
}

i64 VRayVolumeGridRef::getFullCacheVoxelCount() const
{
	const VolumeCacheKey &key = genKey();
	if (!key.isValid())
		return -1;

	QScopedPointer<IAur, IAurPointerDeleter> aurPtr(newIAurMaxVox(key.path.c_str(), -1, true));
	if (!aurPtr) {
		return -1;
	}

	int gridDimensions[3];
	aurPtr->GetDim(gridDimensions);

	return static_cast<i64>(gridDimensions[0]) * gridDimensions[1] * gridDimensions[2];
}

i64 VRayVolumeGridRef::getCurrentCacheVoxelCount() const
{
	return getResolution() == MAX_RESOLUTION
		? -1
		: static_cast<double>(getResolution()) / MAX_RESOLUTION * getFullCacheVoxelCount();
}

std::string VRayVolumeGridRef::getConvertedPath(bool toPhx) const
{
	const char * prefix = getCachePathPrefix();
	const char * suffix = getCachePathSuffix();

	if (!m_doFrameReplace) {
		return prefix;
	}

	const int width = getFrameNumberWidth();
	if (!prefix || !suffix || width < 1) {
		return "";
	}

	if (toPhx) {
		return prefix + std::string(width, '#') + suffix;
	}

	char frameStr[64];
	sprintf(frameStr, "%0*d", width, getCurrentCacheFrame());
	return std::string(prefix) + frameStr + suffix;
}

int VRayVolumeGridRef::splitPath(const UT_String &path, std::string &prefix, std::string &suffix) const
{
	UT_String hPrefix;
	UT_String frame;
	UT_String hSuffix;

	const int result = path.parseNumberedFilename(hPrefix, frame, hSuffix);
	if (result) {
		if (m_doFrameReplace) {
			prefix = hPrefix.toStdString();
			suffix = hSuffix.toStdString();
		}
		else {
			prefix = path.toStdString();
			suffix = "";
		}
	}
	else {
		prefix = suffix = "";
	}

	return frame.length();
}

int VRayVolumeGridRef::updateFrom(const UT_Options &options)
{
	UT_Options newOptions(options);

	m_doFrameReplace =
		newOptions.hasOption("literal_cache_path") &&
		!newOptions.getOptionB("literal_cache_path");

	bool pathChange = false;
	if (newOptions.hasOption(IntrinsicNames::cache_path)) {
		std::string prefix;
		std::string suffix;

		const int frameWidth = splitPath(newOptions.getOptionS(IntrinsicNames::cache_path).c_str(), prefix, suffix);

		pathChange =
			frameWidth != getFrameNumberWidth() ||
			prefix != getCachePathPrefix() ||
			suffix != getCachePathSuffix();

		m_options.setOptionS(IntrinsicNames::cache_path_prefix, prefix.c_str());
		m_options.setOptionS(IntrinsicNames::cache_path_suffix, suffix.c_str());
		m_options.setOptionI(IntrinsicNames::frame_number_width, frameWidth);
	}

	if (m_doFrameReplace) {
		// If we aren't replacing frame we don't care if frame changes
		// this means user hardcoded a path for a specific frame
		pathChange |=
			newOptions.hasOption(IntrinsicNames::current_frame) &&
			newOptions.getOptionI(IntrinsicNames::current_frame) != getCurrentFrame();
	}

	m_channelDirty |= pathChange;

	newOptions.setOptionS(IntrinsicNames::current_cache_path, getConvertedPath(false).c_str());
	newOptions.setOptionS(IntrinsicNames::cache_path, getConvertedPath(true).c_str());

	const int updateRes = VRayBaseRef::updateFrom(newOptions);
	if (updateRes || m_channelDirty) {
		buildMapping();

#ifdef HDK_16_5
		getPrim()->transformDirty();
		getPrim()->attributeDirty();
#else
		transformDirty();
		attributeDirty();
#endif
	}

	return updateRes;
}


#endif // CGR_HAS_AUR