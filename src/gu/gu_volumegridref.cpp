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
#include "vfh_attr_utils.h"
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

UT_Matrix4F getCacheTm(VRayForHoudini::VRayVolumeGridRef::CachePtr &cache, bool flipYZ)
{
	if (!cache)
		return UT_Matrix4F(1.f);

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

typedef VRayBaseRefFactory<VRayVolumeGridRef> VRayVolumeGridRefFactory;

static const int MAX_CHAN_MAP_LEN = 2048;
static const int MAX_RESOLUTION = 100;
static const QString framePattern("####");

static GA_PrimitiveTypeId theTypeId(-1);
static VRayVolumeGridRefFactory theFactory("VRayVolumeGridRef");

static bool pathContainFramePattern(const QString &path)
{
	return path.contains(framePattern);
}

static UT_String getDefaultMapping(const char *cachePath)
{
	UT_String defMapping("");

	char buff[MAX_CHAN_MAP_LEN];
	if (1 == aurGenerateDefaultChannelMappings(buff, MAX_CHAN_MAP_LEN, cachePath)) {
		defMapping = UT_String(buff, true);
	}

	return defMapping;
}

struct IAurPointerDeleter {
    static void cleanup(IAur *pointer) {
		deleteIAur(pointer);
	}
};

bool VolumeCacheKey::isValid() const
{
	return !path.isEmpty();
}

void VRayVolumeGridRef::install(GA_PrimitiveFactory *primfactory)
{
	theTypeId = VRayVolumeGridRefFactory::install(*primfactory, theFactory);

	VRayBaseRefCollect::install(theTypeId);
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
	if (key.map.isEmpty()) {
		aurPtr = newIAurMaxVox(_toChar(key.path), voxelCount, infoOnly);
	}
	else {
		aurPtr = newIAurWithChannelsMappingMaxVox(_toChar(key.path), _toChar(key.map), voxelCount, infoOnly);
	}
	data.aurPtr = VRayVolumeGridRef::CachePtr(aurPtr, [](IAur *ptr) { deleteIAur(ptr); });
	const time_point tEndCache = time_clock::now();

	memset(data.dataRange.data(), 0, DataRangeMapSize);

	if (data.aurPtr) {
		Log::getLog().debug("Loading cache took %dms", static_cast<int>(std::chrono::duration_cast<milliseconds>(tEndCache - tStart).count()));
	}
	else {
		data.detailHandle.clear();
		Log::getLog().error("Failed to load cache \"%s\"", _toChar(key.path));
		return;
	}

	int gridDimensions[3];
	data.aurPtr->GetDim(gridDimensions);
	data.tm = getCacheTm(data.aurPtr, key.flipYZ);

	// Load data ranges per channel.
	for (int c = 0; c < CHANNEL_COUNT; ++c) {
		const ChannelInfo &chan = chInfo[c];
		if (data.aurPtr->ChannelPresent(chan.type)) {
			const float *grid = data.aurPtr->ExpandChannel(chan.type);
			const i64 cacheVoxelsCount = gridDimensions[0] * gridDimensions[1] * gridDimensions[2];

			const std::pair<const float *, const float *> dataRange = std::minmax_element(grid, grid + cacheVoxelsCount);
			data.dataRange[chan.type].min = *dataRange.first;
			data.dataRange[chan.type].max = *dataRange.second;
		}
	}

	GU_Detail *gdp = new GU_Detail();

	// Generate smoke primitive.
	for (int c = 0; c < CHANNEL_COUNT; ++c) {
		const ChannelInfo &chan = chInfo[c];
		if (data.aurPtr->ChannelPresent(chan.type) &&
			chan.type == GridChannels::ChSm)
		{
			GU_PrimVolume *volumeGdp = static_cast<GU_PrimVolume*>(GU_PrimVolume::build(gdp));
			volumeGdp->setVisualization(GEO_VOLUMEVIS_SMOKE, volumeGdp->getVisIso(), volumeGdp->getVisDensity());

			UT_VoxelArrayWriteHandleF voxelHandle = volumeGdp->getVoxelWriteHandle();
			voxelHandle->size(gridDimensions[0], gridDimensions[1], gridDimensions[2]);

			const time_point tStartExtract = time_clock::now();

			const float *grid = data.aurPtr->ExpandChannel(chan.type);
			voxelHandle->extractFromFlattened(grid, gridDimensions[0], gridDimensions[1] * gridDimensions[0]);

			const time_point tEndExtract = time_clock::now();
			const int extractTime = std::chrono::duration_cast<milliseconds>(tEndExtract - tStartExtract).count();

			Log::getLog().debug("Extracting channel \"%s\" took %dms", chan.displayName, extractTime);

			volumeGdp->setTransform4(data.tm);

			data.bbox.initBounds();
			data.bbox.enlargeBounds(UT_Vector3(1.0f, 1.0f, 1.0f));
			data.bbox.enlargeBounds(UT_Vector3(-1.0f, -1.0f, -1.0f));

			break;
		}
	}

	data.detailHandle.allocateAndSet(gdp);
}

VRayVolumeGridRef::VRayVolumeGridRef()
	: m_channelDirty(false)
{
	memset(m_currentData.dataRange.data(), 0, DataRangeMapSize);
	initDataCache();
}

VRayVolumeGridRef::VRayVolumeGridRef(const VRayVolumeGridRef &src)
	: VRayVolumeGridRefBase(src)
	, m_currentData(src.m_currentData)
	, m_channelDirty(false)
{
	initDataCache();
}

VRayVolumeGridRef::~VRayVolumeGridRef()
{}

GA_PrimitiveTypeId VRayVolumeGridRef::typeId()
{
	return theTypeId;
}

void VRayVolumeGridRef::initDataCache() const
{
	m_dataCache.setFetchCallback(fetchData);

	// We don't need the evict callback, because the data is in RAII objects and will be freed when evicted from cache,
	// so just print info.
	m_dataCache.setEvictCallback([](const VolumeCacheKey &key, VolumeCacheData&) {
		Log::getLog().debug("Removing \"%s\" from cache", _toChar(key.path));
	});
}

VolumeCacheKey VRayVolumeGridRef::genKey() const
{
	return VolumeCacheKey(getCurrentPath(), getUsrchmap(), getFlipYz());
}

GU_PackedFactory* VRayVolumeGridRef::getFactory() const
{
	return &theFactory;
}

bool VRayVolumeGridRef::unpack(GU_Detail&) const
{
	// This will show error and indicate that we don't support unpacking.
	return false;
}

bool VRayVolumeGridRef::getBounds(UT_BoundingBox &box) const
{
	box = m_currentData.bbox;
	return true;
}

int VRayVolumeGridRef::detailRebuild()
{
	using namespace std;
	using namespace chrono;

	const VolumeCacheKey &key = genKey();
	if (key.isValid()) {
		fetchDataMaxVox(key, m_currentData, getCurrentCacheVoxelCount(), false);
	}

	const int res = m_detail != m_currentData.detailHandle;
	m_detail = m_currentData.detailHandle;

	return res;
}

UT_StringArray VRayVolumeGridRef::getCacheChannels() const
{
	UT_StringArray channels;
	if (!m_channelDirty) {
		getPhxChannelMap(channels);
		return channels;
	}

	const QString loadPath = getCurrentPath();

	int chanIndex = 0;
	int isChannelVector3D = 0;
	char chanName[MAX_CHAN_MAP_LEN];

	while (1 == aurGet3rdPartyChannelName(chanName, MAX_CHAN_MAP_LEN, &isChannelVector3D, _toChar(loadPath), chanIndex++)) {
		channels.append(chanName);
	}

	SYSconst_cast(this)->m_channelDirty = false;
	SYSconst_cast(this)->setPhxChannelMap(channels);

	return channels;
}

void VRayVolumeGridRef::buildMapping()
{
	const QString loadPath = getCurrentPath();
	if (loadPath.isEmpty())
		return;

	UT_String chanMap = "";

	// Aur caches don't need any mappings.
	if (loadPath.endsWith(".aur")) {
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

			const int64 res = getOptions().hasOption(chan.propName)
				? getOptions().getOptionI(chan.propName) - 1
				: -1;

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
			chanMap = getDefaultMapping(_toChar(loadPath));
		}
	}
	
	setUsrchmap(chanMap.buffer());
}

int VRayVolumeGridRef::getFrame() const
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

QString VRayVolumeGridRef::getCurrentPath() const
{
	QString loadPath = getCachePath();
	if (pathContainFramePattern(loadPath)) {
		loadPath = loadPath.replace(framePattern,
									QString::number(VUtils::fast_ceil(getCurrentFrame())));
	}
	return loadPath;
}

fpreal64 VRayVolumeGridRef::getResolution() const
{
	const int resMode = getResMode();
	const fpreal64 previewRes = getPreviewRes();

	return resMode == 0 ? MAX_RESOLUTION : previewRes;
}

i64 VRayVolumeGridRef::getFullCacheVoxelCount() const
{
	const VolumeCacheKey &key = genKey();
	if (!key.isValid())
		return -1;

	QScopedPointer<IAur, IAurPointerDeleter> aurPtr(newIAurMaxVox(_toChar(key.path), -1, true));
	if (!aurPtr)
		return -1;

	int gridDimensions[3];
	aurPtr->GetDim(gridDimensions);

	return static_cast<i64>(gridDimensions[0]) * gridDimensions[1] * gridDimensions[2];
}

i64 VRayVolumeGridRef::getCurrentCacheVoxelCount() const
{
	fpreal64 resolution = getResolution();
	i64 fullVoxelCount = getFullCacheVoxelCount();

	int64 count_digits = static_cast<int64>(log10(fullVoxelCount)) + 1;
	int64 relativity = count_digits <= 6 
		? 1
		: pow(10, count_digits - 6);

	return resolution == MAX_RESOLUTION
		? -1
		: fullVoxelCount * resolution / MAX_RESOLUTION /  relativity;
}

#ifdef HDK_16_5
int VRayVolumeGridRef::updateFrom(GU_PrimPacked *prim, const UT_Options &options)
#else
int VRayVolumeGridRef::updateFrom(const UT_Options &options)
#endif
{
	m_channelDirty =
		vutils_strcmp(getCachePath(), options.getOptionS(IntrinsicNames::cache_path)) != 0 ||
		pathContainFramePattern(getCachePath()) && !IsFloatEq(getCurrentFrame(), options.getOptionF(IntrinsicNames::current_frame));

#ifdef HDK_16_5
	const int updateRes = VRayBaseRef::updateFrom(prim, options);
#else
	const int updateRes = VRayBaseRef::updateFrom(options);
#endif
	if (updateRes || m_channelDirty) {
		buildMapping();

#ifdef HDK_16_5
		prim->transformDirty();
		prim->attributeDirty();
#else
		transformDirty();
		attributeDirty();
#endif
	}

	return updateRes;
}


#endif // CGR_HAS_AUR