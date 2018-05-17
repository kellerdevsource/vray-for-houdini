//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifdef CGR_HAS_AUR

#include "vfh_log.h"
#include "vfh_defines.h"
#include "vfh_attr_utils.h"
#include "vfh_phx_utils.h"
#include "gu_volumegridref.h"

#include <GU/GU_PrimVolume.h>
#include <GU/GU_PackedGeometry.h>

#include <chrono>
#include <QScopedPointer>

namespace {

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

} // namespace

using namespace VRayForHoudini;

typedef VRayBaseRefFactory<VRayVolumeGridRef> VRayVolumeGridRefFactory;

static const fpreal MAX_RESOLUTION = 100.0;
static const QString framePattern("####");

static GA_PrimitiveTypeId theTypeId(-1);
static VRayVolumeGridRefFactory theFactory("VRayVolumeGridRef");

static bool pathContainFramePattern(const QString &path)
{
	return path.contains(framePattern);
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
	using namespace PhxChannelsUtils;

	typedef std::chrono::high_resolution_clock::time_point time_point;
	typedef std::chrono::high_resolution_clock time_clock;
	typedef std::chrono::milliseconds milliseconds;

	const time_point tStart = time_clock::now();
	IAur *aurPtr;
	if (key.map.isEmpty()) {
		aurPtr = newIAurMaxVox(qPrintable(key.path), voxelCount, infoOnly);
	}
	else {
		aurPtr = newIAurWithChannelsMappingMaxVox(qPrintable(key.path), qPrintable(key.map), voxelCount, infoOnly);
	}
	data.aurPtr = VRayVolumeGridRef::CachePtr(aurPtr, [](IAur *ptr) { deleteIAur(ptr); });
	const time_point tEndCache = time_clock::now();

	memset(data.dataRange.data(), 0, DataRangeMapSize);

	if (data.aurPtr) {
		Log::getLog().debug("Loading cache took %dms", static_cast<int>(std::chrono::duration_cast<milliseconds>(tEndCache - tStart).count()));
	}
	else {
		data.detailHandle.clear();
		Log::getLog().error("Failed to load cache \"%s\"", qPrintable(key.path));
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

	GU_Detail *gdpPacked = new GU_Detail;
	GU_PackedGeometry::packGeometry(*gdpPacked, data.detailHandle);

	data.detailHandle.allocateAndSet(gdpPacked);
}

VRayVolumeGridRef::VRayVolumeGridRef()
	: m_channelDirty(false)
{
	memset(m_currentData.dataRange.data(), 0, DataRangeMapSize);
}

VRayVolumeGridRef::VRayVolumeGridRef(const VRayVolumeGridRef &src)
	: VRayVolumeGridRefBase(src)
	, m_currentData(src.m_currentData)
	, m_channelDirty(false)
{
}

VRayVolumeGridRef::~VRayVolumeGridRef()
{}

GA_PrimitiveTypeId VRayVolumeGridRef::typeId()
{
	return theTypeId;
}

VolumeCacheKey VRayVolumeGridRef::genKey() const
{
	return VolumeCacheKey(getCurrentPath(), getUsrchmap(), getFlipYz());
}

int VRayVolumeGridRef::evalCacheFrame() const
{
	return PhxAnimUtils::evalCacheFrame(
		getCurrentFrame(),
		getMaxLength(),
		getPlaySpeed(),
		getAnimMode(),
		getT2F(),
		getPlayAt(),
		getLoadNearest(),
		getReadOffset()
	);
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

QString VRayVolumeGridRef::getCurrentPath() const
{
	QString loadPath = getCachePath();
	PhxAnimUtils::evalPhxPattern(loadPath, this->evalCacheFrame());
	
	return loadPath;
}

fpreal64 VRayVolumeGridRef::getResolution() const
{
	const int resMode = getResMode();
	const fpreal64 previewRes = VUtils::clamp(getPreviewRes(), 1.0, MAX_RESOLUTION);

	return resMode == 0 ? MAX_RESOLUTION : previewRes;
}

i64 VRayVolumeGridRef::getFullCacheVoxelCount() const
{
	const VolumeCacheKey &key = genKey();
	if (!key.isValid())
		return -1;

	QScopedPointer<IAur, IAurPointerDeleter> aurPtr(newIAurMaxVox(qPrintable(key.path), -1, true));
	if (!aurPtr)
		return -1;

	int gridDimensions[3];
	aurPtr->GetDim(gridDimensions);

	return static_cast<i64>(gridDimensions[0]) * gridDimensions[1] * gridDimensions[2];
}

i64 VRayVolumeGridRef::getCurrentCacheVoxelCount() const
{
	const fpreal64 resolution = getResolution();
	const i64 fullVoxelCount = getFullCacheVoxelCount();

	const int64 count_digits = static_cast<int64>(log10(fullVoxelCount)) + 1;
	const int64 relativity = count_digits <= 6 ? 1 : pow(10, count_digits - 6);

	return resolution == MAX_RESOLUTION ? -1 : fullVoxelCount * resolution / MAX_RESOLUTION / relativity;
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