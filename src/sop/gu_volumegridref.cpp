//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "gu_volumegridref.h"
#include "vfh_log.h"

#include <GT/GT_GEOPrimCollect.h>
#include <GT/GT_GEOAttributeFilter.h>
#include <GT/GT_GEODetail.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_PrimInstance.h>

#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PackedContext.h>
#include <UT/UT_MemoryCounter.h>
#include <GU/GU_PrimVolume.h>
#include <GU/GU_PrimPacked.h>
#include <GEO/GEO_Primitive.h>
#include <FS/UT_DSO.h>

#include <aurinterface.h>
#include <aurloader.h>

#include <OpenEXR/ImathLimits.h>
#include <OpenEXR/ImathMath.h>


using namespace VRayForHoudini;

GA_PrimitiveTypeId VRayVolumeGridRef::theTypeId(-1);
const int MAX_CHAN_MAP_LEN = 2048;

VFH_DEFINE_FACTORY_BASE(VRayVolumeGridFactoryBase, VRayVolumeGridRef, VFH_VOLUME_GRID_PARAMS, VFH_VOLUME_GRID_PARAMS_COUNT)

class VRayVolumeGridFactory:
		public VRayVolumeGridFactoryBase
{
public:
	static VRayVolumeGridFactory &getInstance()
	{
		static VRayVolumeGridFactory theFactory;
		return  theFactory;
	}

	virtual GU_PackedImpl* create() const VRAY_OVERRIDE
	{
		return new VRayVolumeGridRef();
	}

private:
	VRayVolumeGridFactory();
	virtual ~VRayVolumeGridFactory()
	{ }

	VRayVolumeGridFactory(const VRayVolumeGridFactory &other);
	VRayVolumeGridFactory& operator =(const VRayVolumeGridFactory &other);

};

VRayVolumeGridFactory::VRayVolumeGridFactory():
	VRayVolumeGridFactoryBase("VRayVolumeGridRef", "VRayVolumeGridRef")
{
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
		Log::getLog().error("Multiple attempts to install packed primitive %s from %s",
					theFactory.name(), UT_DSO::getRunningFile());
		return;
	}

	GU_PrimPacked::registerPacked(gafactory, &theFactory);
	if (NOT(theFactory.isRegistered())) {
		Log::getLog().error("Unable to register packed primitive %s from %s",
					theFactory.name(), UT_DSO::getRunningFile());
		return;
	}

	theTypeId = theFactory.typeDef().getId();
}


VRayVolumeGridRef::VRayVolumeGridRef():
	VRayPackedImplBase(),
	m_detail(),
	m_dirty(false)
{ }


VRayVolumeGridRef::VRayVolumeGridRef(const VRayVolumeGridRef &src):
	VRayPackedImplBase(src),
	m_detail(),
	m_dirty(false)
{
	updateFrom(src.m_options);
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
	auto path = this->get_cache_path();
	auto map = this->get_usrchmap();
	if (!map || !path || !*path) {
		return nullptr;
	}

	return CachePtr(
		*map ? newIAurWithChannelsMapping(path, map) : newIAur(path),
		[](IAur *ptr) {
			deleteIAur(ptr);
		}
	);
}


UT_Matrix4F VRayVolumeGridRef::toWorldTm(std::shared_ptr<IAur> cache) const
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

	return m4;
}


bool VRayVolumeGridRef::getBounds(UT_BoundingBox &box) const
{
	auto cache = getCache();
	if (!cache) {
		return false;
	}

	auto tm = toWorldTm(cache);

	int gridDimensions[4] = {1, 1, 1, 1};
	cache->GetDim(gridDimensions);

	UT_Vector4 min(0.f, 0.f, 0.f), max(gridDimensions);

	min.rowVecMult(tm);
	max.rowVecMult(tm);

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

	auto cache = getCache();
	if (!cache) {
		return getDetail();
	}

	int gridDimensions[3];
	cache->GetDim(gridDimensions);
	auto tm = toWorldTm(cache);

	// houdini simulations are 2x2x2 box from (-1,-1,-1) to (1,1,1)
	// this will transform houdini box to grid dimentions
	UT_Matrix4F hou2phx(1.f);

	for(int c = 0; c < 3; ++c) {
		hou2phx(3, c) = gridDimensions[c] * 0.5f;
		hou2phx(c, c) = gridDimensions[c] * 0.5f;
	}

	auto gridTm = hou2phx * tm;

	GU_Detail *gdp = new GU_Detail();
	auto GetCellIndex = [&gridDimensions](int x, int y, int z) {
		return x + y * gridDimensions[0] + z * gridDimensions[1] * gridDimensions[0];
	};

	const char *chNames[10] = { "Temperature", "Smoke", "Spped", "Velocity X", "Velocity Y", "Velocity Z", "Color R", "Color G", "Color B", "Fuel"};

	for (auto chan = GridChannels::ChT; chan <= GridChannels::ChFl; ++reinterpret_cast<int&>(chan)) {
		if (!cache->ChannelPresent(chan)) {
			continue;
		}
		GU_PrimVolume *volumeGdp = (GU_PrimVolume *)GU_PrimVolume::build(gdp);

		auto visType = GEO_VOLUMEVIS_INVISIBLE;
		switch (chan)
		{
		case GridChannels::ChT: visType = GEO_VOLUMEVIS_RAINBOW; break;
		case GridChannels::ChSm: visType = GEO_VOLUMEVIS_SMOKE; break;
		}

		volumeGdp->setVisualization(visType, volumeGdp->getVisIso(), volumeGdp->getVisDensity());

		UT_VoxelArrayWriteHandleF voxelHandle = volumeGdp->getVoxelWriteHandle();

		voxelHandle->size(gridDimensions[0], gridDimensions[1], gridDimensions[2]);
		const float *grid = cache->ExpandChannel(chan);

		for (int i = 0; i < gridDimensions[0]; ++i) {
			for (int j = 0; j < gridDimensions[1]; ++j) {
				for (int k = 0; k < gridDimensions[2]; ++k) {
					voxelHandle->setValue(i, j, k, grid[GetCellIndex(i, j, k)]);
				}
			}
		}

		volumeGdp->setTransform4(gridTm);
	}

	auto self = SYSconst_cast(this);
	self->m_handle.clear();
	self->m_handle.allocateAndSet(gdp, true);
	self->m_detail = self->m_handle;

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

	// The UT_MemoryCounter interface needs to be enhanced to efficiently count
	// shared memory for details. Skip this for now.
#if 0
	if (detail().isValid())
	{
		GU_DetailHandleAutoReadLock gdh(detail());
		gdh.getGdp()->countMemory(counter, true);
	}
#endif
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
	if (!m_channelDirty) {
		return m_cacheChannels;
	}
	SYSconst_cast(this)->m_cacheChannels.clear();
	int chanIndex = 0, isChannelVector3D;
	char chanName[MAX_CHAN_MAP_LEN];
	while(1 == aurGet3rdPartyChannelName(chanName, MAX_CHAN_MAP_LEN, &isChannelVector3D, this->get_cache_path(), chanIndex++)) {
		SYSconst_cast(this)->m_cacheChannels.append(chanName);
	}

	SYSconst_cast(this)->m_channelDirty = false;
	return m_cacheChannels;
}

void VRayVolumeGridRef::buildMapping() {
	auto path = this->get_cache_path();

	UT_String chanMap;

	if (UT_String(path).endsWith(".aur")) {
		chanMap = "";
	} else {
		auto channels = getCacheChannels();
		const int chCount = 9;
		static const char *chNames[chCount] = {"channel_smoke", "channel_temp", "channel_fuel", "channel_vel_x", "channel_vel_y", "channel_vel_z", "channel_red", "channel_green", "channel_blue"};
		static const int   chIDs[chCount] = {2, 1, 10, 4, 5, 6, 7, 8, 9};

		// will hold names so we can use pointers to them
		std::vector<UT_String> names;
		std::vector<int> ids;
		for (int c = 0; c < chCount; ++c) {
			UT_String value(UT_String::ALWAYS_DEEP);
			auto res = m_options.hasOption(chNames[c]) ? m_options.getOptionI(chNames[c]) - 1 : -1;
			if (res >= 0 && res < channels.size()) {
				value = channels(res);
				if (value != "" && value != "0") {
					names.push_back(value);
					ids.push_back(chIDs[c]);
				}
			}
		}

		const char * inputNames[chCount] = {0};
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

	if(m_dirty = chanMap != this->get_usrchmap()) {
		this->set_usrchmap(chanMap);
	}
}

bool VRayVolumeGridRef::updateFrom(const UT_Options &options)
{
	// difference in cache or mapping raises dirty flag
	const bool pathChange = options.hasOption("cache_path") && options.getOptionS("cache_path") != this->get_cache_path();
	m_channelDirty = m_channelDirty || pathChange;

	m_dirty = pathChange || m_dirty || options.hasOption("flip_yz") && options.getOptionI("flip_yz") != this->get_flip_yz();

	const bool diffHash = options.hash() != m_options.hash();
	m_options.merge(options);
	buildMapping();

	if (m_dirty) {
		transformDirty();
	}

	if (diffHash) {
		attributeDirty();
	}

	return true;
}