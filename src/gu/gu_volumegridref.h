//
// Copyright (c) 2015-2018, Chaos Software Ltd
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
#include <QString>

namespace VRayForHoudini {

struct VolumeCacheKey {
	VolumeCacheKey()
		: flipYZ(false)
	{}
	VolumeCacheKey(const QString &path, const QString &map, bool flipYZ)
		: path(path)
		, map(map)
		, flipYZ(flipYZ)
	{}

	QString path;
	QString map;
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
		using namespace VRayForHoudini::Hash;

		MHash pathHash = 42;
		QByteArray path = volumeKey.path.toLocal8Bit();
		MurmurHash3_x86_32(path.constData(), path.length(), pathHash, &pathHash);

		MHash mapHash = 42;
		QByteArray map = volumeKey.map.toLocal8Bit();
		MurmurHash3_x86_32(map.constData(), map.length(), mapHash, &mapHash);

#pragma pack(push, 1)
		struct VolumeCacheKeyHash {
			MHash pathHash;
			MHash mapHash;
			int flipYZ;
		} volumeCacheKeyHash = { pathHash, mapHash, volumeKey.flipYZ };
#pragma pack(pop)

		MHash hash = 42;
		MurmurHash3_x86_32(&volumeCacheKeyHash, sizeof(VolumeCacheKeyHash), hash, &hash);

		return hash;
	}
};

} // namespace std

namespace VRayForHoudini {

/// Implementation for a volume grid packed primitive
class VRayVolumeGridRef
	: public VRayVolumeGridRefBase
{
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
		VolumeCacheData() {
			tm.identity();
		}

		CachePtr aurPtr;
		UT_Matrix4F tm;
		GU_DetailHandle detailHandle;
		UT_BoundingBox bbox;
		DataRangeMap dataRange;
	};

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
	bool getBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;

	const DataRangeMap &getChannelDataRanges() const { return m_currentData.dataRange; }

protected:
	// From VRayBaseRef.
#ifdef HDK_16_5
	int updateFrom(GU_PrimPacked *prim, const UT_Options &options) VRAY_OVERRIDE;
#else
	int updateFrom(const UT_Options &options) VRAY_OVERRIDE;
#endif

	int detailRebuild(GU_PrimPacked *prim) VRAY_OVERRIDE;

private:
	/// Get all channels present in the current cache
	UT_StringArray getCacheChannels() const;

	/// Generates VolumeCacheKey from current data
	VolumeCacheKey genKey() const;

	/// Get current cache frame based on current frame + cache play settings
	int evalCacheFrame() const;

	/// Returns fully resolved cache load path.
	QString getCurrentPath() const;

	/// Gets resolution of cache (from UI)
	fpreal64 getResolution() const;

	/// Gets count of voxels in cache (in full resolution)
	/// @return - -1 if generated key is invalid
	i64 getFullCacheVoxelCount() const;

	/// Gets count of voxels in cache (in current resolution)
	/// @return - -1 if in full resolution, negative not equal to -1 if invalid key
	i64 getCurrentCacheVoxelCount() const;

	mutable VolumeCacheData m_currentData;

	/// True if channel mapping have changed since we last built them.
	bool m_channelDirty;
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
