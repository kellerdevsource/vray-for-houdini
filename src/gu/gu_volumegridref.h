//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOLUME_GRID_H
#define VRAY_FOR_HOUDINI_VOLUME_GRID_H

#ifdef CGR_HAS_AUR

#include "vfh_includes.h"
#include "vfh_lru_cache.hpp"
#include "vfh_hashes.h"
#include "vfh_VRayVolumeGridRefBase.h"

#include <aurinterface.h>

namespace VRayForHoudini {

struct VolumeCacheKey {
	VolumeCacheKey()
		: flipYZ(false)
	{}
	VolumeCacheKey(const std::string &path, const std::string &map, bool flipYZ)
		: path(path)
		, map(map)
		, flipYZ(flipYZ)
	{}

	std::string path;
	std::string map;
	bool flipYZ;

	bool isValid() const;
};

inline bool operator==(const VolumeCacheKey &left, const VolumeCacheKey &right) {
	return left.path == right.path && left.map == right.map && left.flipYZ == right.flipYZ;
}

} // namespace VRayForHoudin

// extending namespace std with the proper specialization is the "correct" way according to the standard
namespace std {

template <> struct hash<VRayForHoudini::VolumeCacheKey> {
	size_t operator()(const VRayForHoudini::VolumeCacheKey &volumeKey) const {
		VRayForHoudini::Hash::MHash hash = 42;
		VRayForHoudini::Hash::MurmurHash3_x86_32(volumeKey.path.c_str(), volumeKey.path.length(), hash, &hash);
		VRayForHoudini::Hash::MurmurHash3_x86_32(volumeKey.map.c_str(), volumeKey.map.length(), hash, &hash);
		VRayForHoudini::Hash::MurmurHash3_x86_32(&volumeKey.flipYZ, sizeof(volumeKey.flipYZ), hash, &hash);
		return hash;
	}
};

} // namespace std

namespace VRayForHoudini {

/// Implementation for a volume grid packed primitive
class VRayVolumeGridRef
	: public VRayVolumeGridRefBase
{
	// These *must* match Phoenix values.
	enum AnimationMode {
		standard = 0,
		directIndex = 1,
		loop = 2,
	};

public:
	typedef std::shared_ptr<IAur> CachePtr;

	struct MinMaxPair {
		float min;
		float max;
	};

	typedef std::array<MinMaxPair, GridChannels::Ch_Count> DataRangeMap;

	enum {
		DataRangeMapSize = sizeof(DataRangeMap),
	};

	struct VolumeCacheData {
		CachePtr aurPtr;
		GU_DetailHandle detailHandle;
		DataRangeMap dataRange;
	};
	typedef Caches::LRUCache<VolumeCacheKey, VolumeCacheData, std::hash<VolumeCacheKey>, std::equal_to<VolumeCacheKey>, 10> VolumeCache;

	/// Returns the primitive type ID.
	static GA_PrimitiveTypeId typeId();

	/// Registers the primitive factory.
	static void install(GA_PrimitiveFactory *gafactory);

private:
	/// Fetch data from key.
	static void fetchData(const VolumeCacheKey &key, VolumeCacheData &data);

	/// Fetch data (or only info) from key downsampled to voxelsCount.
	static void fetchDataMaxVox(const VolumeCacheKey &key, VolumeCacheData &data, const i64 voxelCount, const bool infoOnly);

public:
	VRayVolumeGridRef();
	VRayVolumeGridRef(const VRayVolumeGridRef &src);
	virtual ~VRayVolumeGridRef();

	// From GU_PackedImpl.
	GU_PackedFactory *getFactory() const VRAY_OVERRIDE;
	GU_PackedImpl *copy() const VRAY_OVERRIDE { return new VRayVolumeGridRef(*this); }
	bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;

	/// Load or get from cache VolumeCacheData with given key
	/// REQUIRES: key to be valid
	VolumeCacheData &getCache(const VolumeCacheKey &key) const;

	/// Get the world TM
	UT_Matrix4F toWorldTm(CachePtr cache) const;

	/// Get all channels present in the current cache
	UT_StringArray getCacheChannels() const;
	VolumeCache &getCachedData() const;

	const DataRangeMap &getChannelDataRanges() const { return m_currentData.dataRange; }
	DataRangeMap &getChannelDataRanges() { return m_currentData.dataRange; }

protected:
	// From VRayBaseRef.
#ifdef HDK_16_5
	int updateFrom(GU_PrimPacked *prim, const UT_Options &options) VRAY_OVERRIDE;
#else
	int updateFrom(const UT_Options &options) VRAY_OVERRIDE;
#endif

	int detailRebuild() VRAY_OVERRIDE;

private:
	/// Sets fetch and evict callback.
	void initDataCache() const;

	/// Generates VolumeCacheKey from current data
	VolumeCacheKey genKey() const;

	/// Builds the cache path according to current settings
	/// @param toPhx - if true frame will be replaced with '#'s otherwise with current cache frame
	/// @return - replaced cache path
	std::string getConvertedPath(bool toPhx) const;

	/// Split the path if there is a frame number in it
	/// @path - the cache path
	/// @prefix[out] - everything up to the frame, equal to @path if there is no frame
	/// @suffix[out] - everything after the frame, empty if @path has no frame
	/// @return - the number of digits in the frame (0 if no frame)
	int splitPath(const UT_String &path, std::string &prefix, std::string &suffix) const;

	/// Get current cache frame based on current frame + cache play settings
	int getCurrentCacheFrame() const;

	/// Gets resolution of cache (from UI)
	int getResolution() const;

	/// Gets count of voxels in cache (in full resolution)
	/// @return - -1 if generated key is invalid
	i64 getFullCacheVoxelCount() const;

	/// Gets count of voxels in cache (in current resolution)
	/// @return - -1 if in full resolution, negative not equal to -1 if invalid key
	i64 getCurrentCacheVoxelCount() const;

	/// Build channel mapping, should be called after update to cache or ui mappings
	void buildMapping();

	/// Data cache used to cache last 10 volumes loaded, mutable (needs to be updated from const functions not changing other (immutable)members)
	mutable VolumeCache m_dataCache;
	mutable VolumeCacheData m_currentData;

	/// True if channel mapping have changed since we last built them.
	bool m_channelDirty;

	/// If true we will replace frame number with ### for PHX otherwise user hardcoded frame number and we should not change it.
	bool m_doFrameReplace;
};

} // namespace VRayForHoudini

#else // CGR_HAS_AUR

#include <GU/GU_PackedImpl.h>
/// Define this empty class here so primitive exporters can be compiled without additional ifdefs there
namespace VRayForHoudini {
class VRayVolumeGridRef
	: public GU_PackedImpl
{
	public:
		static GA_PrimitiveTypeId typeId() { return GA_PrimitiveTypeId(); }
};
} // namespace VRayForHoudini

#endif // CGR_HAS_AUR
#endif // VRAY_FOR_HOUDINI_VOLUME_GRID_H
