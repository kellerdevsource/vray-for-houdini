//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//
#ifdef CGR_HAS_AUR

#include "gu_volumegridref.h"
#include "vfh_log.h"

#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PackedContext.h>
#include <UT/UT_MemoryCounter.h>
#include <GU/GU_PrimVolume.h>
#include <GU/GU_PrimPacked.h>
#include <GEO/GEO_Primitive.h>

#include <FS/UT_DSO.h>

#include <aurinterface.h>
#include <aurloader.h>

#include <chrono>

namespace {
/// Info for a single channel, could be casted to int to obtain the channel type for PHX
struct ChannelInfo {
	const char * propName; ///< the name of the property in the plugin
	const char * displayName; ///< the displayed name for the channel
	GridChannels::Enum type; ///< Value of the channel for PHX

	/// Get channel type
	operator int() const {
		return static_cast<int>(type);
	}
};

const ChannelInfo chInfo[] = {
	{ "channel_smoke", "Smoke",       GridChannels::ChSm       },
	{ "channel_temp",  "Temperature", GridChannels::ChT        },
	{ "channel_fuel",  "Fuel",        GridChannels::ChFl       },
	{ "channel_vel_x", "Velocity X",  GridChannels::ChVx       },
	{ "channel_vel_y", "Velocity Y",  GridChannels::ChVy       },
	{ "channel_vel_z", "Velocity Z",  GridChannels::ChVz       },
	{ "channel_red",   "Color R",     GridChannels::ChU        },
	{ "channel_green", "Color G",     GridChannels::ChV        },
	{ "channel_blue",  "Color B",     GridChannels::ChW        },
	{ "INVALID",       "INVALID",     GridChannels::ChReserved },
};
/// number of valid channels
const int CHANNEL_COUNT = (sizeof(chInfo) / sizeof(chInfo[0])) - 1;

/// Get ChannelInfo from it's type
const ChannelInfo & fromType(int type) {
	for (int c = 0; c < CHANNEL_COUNT; ++c) {
		if (static_cast<int>(chInfo[c]) == type) {
			return chInfo[c];
		}
	}
	return chInfo[CHANNEL_COUNT];
}

/// Get ChannelInfo from it's name
const ChannelInfo & fromPropName(const char * name) {
	for (int c = 0; c < CHANNEL_COUNT; ++c) {
		if (!strcmp(name, chInfo[c].propName)) {
			return chInfo[c];
		}
	}
	return chInfo[CHANNEL_COUNT - 1];
}

}


using namespace VRayForHoudini;

GA_PrimitiveTypeId VRayVolumeGridRef::theTypeId(-1);
const int MAX_CHAN_MAP_LEN = 2048;

/// Implemetation for the factory creating VRayVolumeGridRef for HDK
class VRayVolumeGridFactory
	: public GU_PackedFactory
{
public:
	static VRayVolumeGridFactory &getInstance() {
		static VRayVolumeGridFactory theFactory;
		return  theFactory;
	}

	GU_PackedImpl* create() const VRAY_OVERRIDE {
		return new VRayVolumeGridRef();
	}

private:
	VRayVolumeGridFactory();

	VUTILS_DISABLE_COPY(VRayVolumeGridFactory);
};

VRayVolumeGridFactory::VRayVolumeGridFactory():
	GU_PackedFactory("VRayVolumeGridRef", "VRayVolumeGridRef")
{
	/// Register all properties from VFH_VOLUME_GRID_PARAMS
	VFH_MAKE_REGISTERS(VFH_VOLUME_GRID_PARAMS, VFH_VOLUME_GRID_PARAMS_COUNT, VRayVolumeGridRef)

	registerTupleIntrinsic(
			"phx_channel_map",
			IntGetterCast(&VRayVolumeGridRef::getPhxChannelMapSize),
			StringArrayGetterCast(&VRayVolumeGridRef::getPhxChannelMap),
			StringArraySetterCast(&VRayVolumeGridRef::setPhxChannelMap)
			);
}

void VRayVolumeGridRef::install(GA_PrimitiveFactory *gafactory)
{
	VRayVolumeGridFactory &theFactory = VRayVolumeGridFactory::getInstance();
	if (theFactory.isRegistered()) {
		Log::getLog().debug("Multiple attempts to install packed primitive %s from %s",
							static_cast<const char *>(theFactory.name()), UT_DSO::getRunningFile());
		return;
	}

	GU_PrimPacked::registerPacked(gafactory, &theFactory);
	if (NOT(theFactory.isRegistered())) {
		Log::getLog().error("Unable to register packed primitive %s from %s",
							static_cast<const char *>(theFactory.name()), UT_DSO::getRunningFile());
		return;
	}

	theTypeId = theFactory.typeDef().getId();
}


VRayVolumeGridRef::VRayVolumeGridRef()
	: GU_PackedImpl()
	, m_detail()
	, m_dirty(false)
	, m_channelDirty(false)
	, m_doFrameReplace(false)
{
	GU_Detail *gdp = new GU_Detail;
	m_handle.allocateAndSet(gdp, true);
	m_detail = m_handle;
	memset(m_channelDataRange.data(), 0, m_channelDataRange.size() * sizeof(m_channelDataRange[0]));
}

VRayVolumeGridRef::VRayVolumeGridRef(const VRayVolumeGridRef &src):
	GU_PackedImpl(src),
	m_detail(),
	m_dirty(false),
	m_channelDataRange(src.m_channelDataRange)
{
	m_handle = src.m_handle;
	m_detail = m_handle;
	m_options = src.m_options;
}


VRayVolumeGridRef::~VRayVolumeGridRef()
{
	clearDetail();
}


GU_PackedFactory* VRayVolumeGridRef::getFactory() const
{
	return &VRayVolumeGridFactory::getInstance();
}


void VRayVolumeGridRef::clearData()
{
	// This method is called when primitives are "stashed" during the cooking
	// process.  However, primitives are typically immediately "unstashed" or
	// they are deleted if the primitives aren't recreated after the fact.
	// We can just leave our data.
}


bool VRayVolumeGridRef::save(UT_Options &options, const GA_SaveMap &map) const
{
	options.merge(m_options);
	return true;
}


bool VRayVolumeGridRef::getLocalTransform(UT_Matrix4D &m) const
{
	m = toWorldTm(getCache());
	return true;
}

VRayVolumeGridRef::CachePtr VRayVolumeGridRef::getCache() const
{
	auto path = this->get_current_cache_path();
	auto map = this->get_usrchmap();

	if (path != nullptr 
		&& map != nullptr 
		&& m_last_cache_path == path
		&& m_last_user_char_map == map) {
		return m_cachePtr;
	}

	if (!UTisstring(path) || !map) {
		return nullptr;
	}

	// remember last cache source
	m_last_cache_path = path;
	m_last_user_char_map = map;

	// load new cache
	auto ptr = *map ? newIAurWithChannelsMapping(path, map) : newIAur(path);
	m_cachePtr = CachePtr(ptr, [](IAur *ptr) {	deleteIAur(ptr); });
	return m_cachePtr;
}


UT_Matrix4F VRayVolumeGridRef::toWorldTm(CachePtr cache) const
{
	if (!cache) {
		return UT_Matrix4F(1.f);
	}

	float flTransform[12];
	cache->GetObject2GridTransform(flTransform);

	// houdini translations is in last row instead of last col
	UT_Matrix4F m4(
		flTransform[0], flTransform[1],  flTransform[2],  0.f,
		flTransform[3], flTransform[4],  flTransform[5],  0.f,
		flTransform[6], flTransform[7],  flTransform[8],  0.f,
		flTransform[9], flTransform[10], flTransform[11], 1.0
	);

	if (this->get_flip_yz()) {
		for (int c = 0; c < 4; ++c) {
			auto old = m4(1, c);
			m4(1, c) = -m4(2, c);
			m4(2, c) = old;
		}
	}

	m4.invert();

	int gridDimensions[3];
	cache->GetDim(gridDimensions);

	// houdini simulations are 2x2x2 box from (-1,-1,-1) to (1,1,1)
	// this will transform houdini box to grid dimentions
	UT_Matrix4F hou2phx(1.f);

	for(int c = 0; c < 3; ++c) {
		hou2phx(3, c) = gridDimensions[c] * 0.5f;
		hou2phx(c, c) = gridDimensions[c] * 0.5f;
	}

	return hou2phx * m4;
}


bool VRayVolumeGridRef::getBounds(UT_BoundingBox &box) const
{
	auto cache = getCache();
	if (!cache) {
		return false;
	}

	UT_Vector3F min(-1, -1, -1), max(1, 1, 1);
	box.initBounds(min, max);
	SYSconst_cast(this)->setBoxCache(box);

	return true;
}


bool VRayVolumeGridRef::getRenderingBounds(UT_BoundingBox &box) const
{
	// When geometry contains points or curves, the width attributes need to be
	// taken into account when computing the rendering bounds.
	return getBounds(box);
}


void VRayVolumeGridRef::getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const
{
	// No velocity attribute on geometry
	min = 0;
	max = 0;
}


void VRayVolumeGridRef::getWidthRange(fpreal &min, fpreal &max) const
{
	// Width is only important for curves/points.
	min = max = 0;
}


bool VRayVolumeGridRef::unpack(GU_Detail &destgdp) const
{
	// This may allocate geometry for the primitive
	GU_DetailHandleAutoReadLock gdl(getPackedDetail());
	if (NOT(gdl.isValid())) {
		return false;
	}

	return unpackToDetail(destgdp, gdl.getGdp());
}

GU_ConstDetailHandle VRayVolumeGridRef::getPackedDetail(GU_PackedContext *context) const
{
	if (!m_dirty) {
		return getDetail();
	}

	using namespace std;
	using namespace chrono;

	memset(SYSconst_cast(this)->m_channelDataRange.data(), 0, m_channelDataRange.size() * sizeof(m_channelDataRange[0]));

	auto tStart = high_resolution_clock::now();
	auto cache = getCache();
	auto tEndCache = high_resolution_clock::now();

	GU_DetailHandleAutoWriteLock rLock(SYSconst_cast(this)->m_handle);
	GU_Detail *gdp = rLock.getGdp();

	if (cache) {
		Log::getLog().info("Loading cache took %dms", (int)duration_cast<milliseconds>(tEndCache - tStart).count());
	} else {
		gdp->clearAndDestroy();
		SYSconst_cast(this)->m_dirty = false;
		return getDetail();
	}

	int gridDimensions[3];
	cache->GetDim(gridDimensions);
	const auto tm = toWorldTm(cache);

	gdp->stashAll();
	auto tBeginLoop = high_resolution_clock::now();
	// load each channel from the cache file
	for (int c = 0; c < CHANNEL_COUNT; ++c) {
		const auto & chan = chInfo[c];

		if (!cache->ChannelPresent(chan.type)) {
			continue;
		}
		GU_PrimVolume *volumeGdp = (GU_PrimVolume *)GU_PrimVolume::build(gdp);

		auto visType = chan.type == GridChannels::ChSm ? GEO_VOLUMEVIS_SMOKE : GEO_VOLUMEVIS_INVISIBLE;
		volumeGdp->setVisualization(visType, volumeGdp->getVisIso(), volumeGdp->getVisDensity());

		UT_VoxelArrayWriteHandleF voxelHandle = volumeGdp->getVoxelWriteHandle();
		voxelHandle->size(gridDimensions[0], gridDimensions[1], gridDimensions[2]);

		auto tStartExpand = high_resolution_clock::now();
		const float *grid = cache->ExpandChannel(chan.type);
		auto tEndExpand = high_resolution_clock::now();

		int expandTime = duration_cast<milliseconds>(tEndExpand - tStartExpand).count();

		auto tStartExtract = high_resolution_clock::now();
		voxelHandle->extractFromFlattened(grid, gridDimensions[0], gridDimensions[1] * gridDimensions[0]);
		SYSconst_cast(this)->m_channelDataRange[chan.type].min = volumeGdp->calcMinimum();
		SYSconst_cast(this)->m_channelDataRange[chan.type].max = volumeGdp->calcMaximum();
		auto tEndExtract = high_resolution_clock::now();

		int extractTime = duration_cast<milliseconds>(tEndExtract - tStartExtract).count();

		Log::getLog().info("Expanding channel '%s' took %dms, extracting took %dms", chan.displayName, expandTime, extractTime);

		volumeGdp->setTransform4(tm);
	}
	gdp->destroyStashed();
	auto tEndLoop = high_resolution_clock::now();

	Log::getLog().info("Generating packed prim %dms", (int)duration_cast<milliseconds>(tEndLoop - tBeginLoop).count());

	auto self = SYSconst_cast(this);
	self->m_dirty = false;

	return getDetail();
}


int64 VRayVolumeGridRef::getMemoryUsage(bool inclusive) const
{
	int64 mem = inclusive ? sizeof(*this) : 0;
	// Don't count the (shared) GU_Detail, since that will greatly
	// over-estimate the overall memory usage.
	mem += getDetail().getMemoryUsage(false);
	return mem;
}


void VRayVolumeGridRef::countMemory(UT_MemoryCounter &counter, bool inclusive) const
{
	if (counter.mustCountUnshared()) {
		size_t mem = (inclusive)? sizeof(*this) : 0;
		mem += getDetail().getMemoryUsage(false);
		UT_MEMORY_DEBUG_LOG(theFactory->name(), int64(mem));
		counter.countUnshared(mem);
	}

	if (getDetail().isValid())
	{
		GU_DetailHandleAutoReadLock gdh(getDetail());
		gdh.getGdp()->countMemory(counter, true);
	}
}

std::string getDefaultMapping(const char *cachePath) {
	char buff[MAX_CHAN_MAP_LEN];
	if (1 == aurGenerateDefaultChannelMappings(buff, MAX_CHAN_MAP_LEN, cachePath)) {
		return std::string(buff);
	} else {
		return "";
	}
}

UT_StringArray VRayVolumeGridRef::getCacheChannels() const {
	UT_StringArray channels;
	if (!m_channelDirty) {
		getPhxChannelMap(channels);
		return channels;
	}

	int chanIndex = 0, isChannelVector3D;
	char chanName[MAX_CHAN_MAP_LEN];
	while(1 == aurGet3rdPartyChannelName(chanName, MAX_CHAN_MAP_LEN, &isChannelVector3D, this->get_current_cache_path(), chanIndex++)) {
		channels.append(chanName);
	}

	SYSconst_cast(this)->m_channelDirty = false;
	SYSconst_cast(this)->setPhxChannelMap(channels);
	return channels;
}

void VRayVolumeGridRef::buildMapping() {
	const char * path = this->get_current_cache_path();
	if (!path) {
		return;
	}

	UT_String chanMap;
	// aur cache has no need for mappings
	if (UT_String(path).endsWith(".aur")) {
		chanMap = "";
		this->setPhxChannelMap(UT_StringArray());
	} else {
		auto channels = getCacheChannels();

		// will hold names so we can use pointers to them
		std::vector<UT_String> names;
		std::vector<int> ids;
		for (int c = 0; c < CHANNEL_COUNT; ++c) {
			const auto & chan = chInfo[c];

			UT_String value(UT_String::ALWAYS_DEEP);
			auto res = m_options.hasOption(chan.propName) ? m_options.getOptionI(chan.propName) - 1 : -1;
			if (res >= 0 && res < channels.size()) {
				value = channels(res);
				if (value != "" && value != "0") {
					names.push_back(value);
					ids.push_back(static_cast<int>(chan));
				}
			}
		}

		const char * inputNames[CHANNEL_COUNT] = {0};
		for (int c = 0; c < names.size(); ++c) {
			inputNames[c] = names[c].c_str();
		}

		char usrchmap[MAX_CHAN_MAP_LEN] = {0,};
		if (1 == aurComposeChannelMappingsString(usrchmap, MAX_CHAN_MAP_LEN, ids.data(), const_cast<char * const *>(inputNames), names.size())) {
			chanMap = usrchmap;
		}

		// user made no mappings - get default
		if (chanMap.equal("")) {
			chanMap = getDefaultMapping(path);
		}
	}

	if(m_dirty || chanMap != this->get_usrchmap()) {
		this->set_usrchmap(chanMap);
	}
}

int VRayVolumeGridRef::getCurrentCacheFrame() const
{
	float frame = get_current_frame();
	const float animLen = get_max_length();
	const float fractionalLen = animLen * get_play_speed();

	switch (get_anim_mode())
	{
	case AnimationMode::DirectIndex:
		frame = get_t2f();
		break;
	case AnimationMode::Standard:
		frame = get_play_speed() * (frame - get_play_at());

		if (fractionalLen > 1e-4f) {
			if (frame < 0.f || frame > fractionalLen) {
				if (get_load_nearest()) {
					// clamp frame in [0, animLen]
					frame = std::max(0.f, std::min(fractionalLen, frame));
				} else {
					frame = INT_MIN;
				}
			}
		}

		frame += get_read_offset();
		break;
	case AnimationMode::Loop:
		frame = get_play_speed() * (frame - get_play_at());

		if (fractionalLen > 1e-4f) {
			while (frame < 0) {
				frame += fractionalLen;
			}
			while (frame > fractionalLen) {
				frame -= fractionalLen;
			}
		}

		frame += get_read_offset();
		break;
	default:
		break;
	}

	return frame;
}

std::string VRayVolumeGridRef::getConvertedPath(bool toPhx) const
{
	const char * prefix = this->get_cache_path_prefix();
	const char * suffix = this->get_cache_path_suffix();

	if (!m_doFrameReplace) {
		return prefix;
	}

	const int width = this->get_frame_number_width();
	if (!prefix || !suffix || width < 1) {
		return "";
	}

	if (toPhx) {
		return prefix + std::string(width, '#') + suffix;
	} else {
		char frameStr[64];
		sprintf(frameStr, "%0*d", width, getCurrentCacheFrame());
		return std::string(prefix) + frameStr + suffix;
	}
}

int VRayVolumeGridRef::splitPath(const UT_String & path, std::string & prefix, std::string & suffix) const
{
	UT_String hPrefix, frame, hSuffix;
	int result = path.parseNumberedFilename(hPrefix, frame, hSuffix);

	if (result) {
		if (m_doFrameReplace) {
			prefix = hPrefix.toStdString();
			suffix = hSuffix.toStdString();
		} else {
			prefix = path.toStdString();
			suffix = "";
		}
	} else {
		prefix = suffix = "";
	}

	return frame.length();
}

bool VRayVolumeGridRef::updateFrom(const UT_Options &options)
{
	const float frameBefore = getCurrentCacheFrame();
	const auto hashBefore = m_options.hash();

	// flag set to true if parsed cache_path and the value for cache_path are the same - there are no
	// expressions inside (like $F)
	m_doFrameReplace = options.hasOption("literal_cache_path") && !options.getOptionB("literal_cache_path");
	bool pathChange = false;

	// parse the path
	if (options.hasOption("cache_path")) {
		std::string prefix, suffix;
		int frameWidth = splitPath(options.getOptionS("cache_path").c_str(), prefix, suffix);

		pathChange = frameWidth != this->get_frame_number_width() ||
		             prefix     != this->get_cache_path_prefix() ||
		             suffix     != this->get_cache_path_suffix();

		this->set_cache_path_prefix(prefix.c_str());
		this->set_cache_path_suffix(suffix.c_str());
		this->set_frame_number_width(frameWidth);
	}

	if (m_doFrameReplace) {
		// if we aren't replacing frame we don't care if frame changes
		// this means user hardcoded a path for a specific frame 
		pathChange = pathChange || (options.hasOption("current_frame") && options.getOptionI("current_frame") != this->get_current_frame());
	}

	m_channelDirty = m_channelDirty || pathChange;

	m_dirty = pathChange || m_dirty || options.hasOption("flip_yz") && options.getOptionI("flip_yz") != this->get_flip_yz();

	m_options.merge(options);

	this->set_current_cache_path(getConvertedPath(false).c_str());
	this->set_cache_path(getConvertedPath(true).c_str());

	buildMapping();

	const bool diffHash = hashBefore != m_options.hash();
	if (m_doFrameReplace) {
		// skip frame check if frame is hardcoded
		m_dirty = m_dirty || (frameBefore != getCurrentCacheFrame());
	}

	if (m_dirty) {
		transformDirty();
	}

	if (diffHash) {
		attributeDirty();
	}

	return true;
}

#endif // CGR_HAS_AUR
