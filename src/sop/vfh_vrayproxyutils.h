//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAYPROXYCACHE_H
#define VRAY_FOR_HOUDINI_VRAYPROXYCACHE_H

#include "vfh_vray.h"
#include "vfh_hashes.h" // For MurmurHash3_x86_32
#include "vfh_lru_cache.hpp"

#include <GU/GU_DetailHandle.h>
#include <UT/UT_Options.h>


namespace VRayForHoudini {


enum LOD {
	LOD_BBOX = 0,
	LOD_PREVIEW,
	LOD_FULL,
};

class VRayProxyParms;
class VRayProxyCache;
typedef Caches::LRUCache< std::string, VRayProxyCache > VRayProxyCacheMan;

VRayProxyCacheMan&     GetVRayProxyCacheManager();
GU_ConstDetailHandle   GetVRayProxyDetail(const VRayProxyParms &options);


class VRayProxyParms
{
public:
	static const UT_StringRef thePathToken;
	static const UT_StringRef theLODToken;
	static const UT_StringRef theFileToken;
	static const UT_StringRef theFrameToken;
	static const UT_StringRef theAnimTypeToken;
	static const UT_StringRef theAnimOffsetToken;
	static const UT_StringRef theAnimSpeedToken;
	static const UT_StringRef theAnimOverrideToken;
	static const UT_StringRef theAnimStartToken;
	static const UT_StringRef theAnimLengthToken;
	static const UT_StringRef theScaleToken;
	static const UT_StringRef theFlipAxisToken;

	inline static UT_StringHolder   getPath(const UT_Options &options);
	inline static UT_StringHolder   getFilepath(const UT_Options &options);
	inline static exint             getLOD(const UT_Options &options);
	inline static fpreal64          getFloatFrame(const UT_Options &options);
	inline static exint             getAnimType(const UT_Options &options);
	inline static fpreal64          getAnimOffset(const UT_Options &options);
	inline static fpreal64          getAnimSpeed(const UT_Options &options);
	inline static bool              getAnimOverride(const UT_Options &options);
	inline static exint             getAnimStart(const UT_Options &options);
	inline static exint             getAnimLength(const UT_Options &options);
	inline static fpreal64          getScale(const UT_Options &options);
	inline static exint             getFlipAxis(const UT_Options &options);

public:
	VRayProxyParms();
	VRayProxyParms(const UT_Options &options);
	~VRayProxyParms()
	{ }

	VRayProxyParms&        operator =(const UT_Options &options);
	bool                   operator ==(const UT_Options &options) const;
	bool                   operator ==(const VRayProxyParms &options) const;

	inline UT_StringHolder getPath() const { return m_path; }
	inline UT_StringHolder getFilepath() const { return m_filepath; }
	inline exint           getLOD() const { return m_lod; }
	inline fpreal64        getFloatFrame() const { return m_floatFrame; }
	inline exint           getAnimType() const { return m_animType; }
	inline fpreal64        getAnimOffset() const { return m_animOffset; }
	inline fpreal64        getAnimSpeed() const { return m_animSpeed; }
	inline bool            getAnimOverride() const { return m_animOverride; }
	inline exint           getAnimStart() const { return m_animStart; }
	inline exint           getAnimLength() const { return m_animLength; }
	inline fpreal64        getScale() const { return m_scale; }
	inline exint           getFlipAxis() const { return m_flipAxis; }

private:
	UT_StringHolder m_path;
	UT_StringHolder m_filepath;
	exint           m_lod;
	fpreal64        m_floatFrame;
	exint           m_animType;
	fpreal64        m_animOffset;
	fpreal64        m_animSpeed;
	bool            m_animOverride;
	exint           m_animStart;
	exint           m_animLength;
	fpreal64        m_scale;
	exint           m_flipAxis;
};


class VRayProxyCache
{
/// VRayProxyCache caches .vrmesh preview geometry in memory for faster playback
/// preview geometry for a frame is decomposed into mesh and hair geometry and
/// cached individually as GU_Detail (houdini geometry container) objects
/// VRayProxyCache uses 2 LRUCaches - one for cached frames and second for cached geometry
/// this allows storing the same geometry only once, if used in different frames
/// NOTE:
///      1) currently geometry is hashed using vertex positions only(might change in future)
///      2) currently cache capacity is defined by playback range at the time of initialization(might change in future)

private:
	typedef unsigned FrameKey;
	typedef Hash::MHash ItemKey;
	typedef std::vector<ItemKey> ItemKeys;
	typedef GU_DetailHandle Item;

	typedef uint32                                     VoxelType;
	typedef std::pair<VoxelType, VUtils::MeshVoxel* >  Geometry;


	struct GeometryHash
	{
		typedef Hash::MHash result_type;
		typedef Geometry argument_type;

		result_type operator()(const argument_type &val) const;
	};

	struct CachedFrame
	{
		bool hasDetail(const LOD &lod) const { return (m_details.count(lod) > 0); }
		Item &getDetail(const LOD &lod) { return m_details[lod]; }

		bool hasItemKeys(const LOD &lod) const { return (m_keys.count(lod) > 0); }
		ItemKeys &getItemKeys(const LOD &lod) { return m_keys[lod]; }

		std::unordered_map< LOD, Item, std::hash<int>, std::equal_to<int> >     m_details;
		std::unordered_map< LOD, ItemKeys, std::hash<int>, std::equal_to<int> > m_keys;
	};

	struct CachedItem
	{
		Item m_item;
		int m_refCnt;
	};

	typedef Caches::LRUCache< FrameKey, CachedFrame, std::hash<FrameKey>, std::equal_to<FrameKey> > FrameCache;
	typedef Caches::LRUCache< ItemKey, CachedItem, std::hash<ItemKey>, std::equal_to<ItemKey> > ItemCache;

public:
	typedef FrameCache::size_type size_type;

	VRayProxyCache();
	VRayProxyCache(VRayProxyCache&& other);
	VRayProxyCache& operator=(VRayProxyCache&& other);
	~VRayProxyCache();

	size_type   capacity() const { return m_frameCache->capacity(); }
	size_type   size() const { return m_frameCache->size(); }
	int         empty() const { return (size() == 0); }

/// @brief Clears previous cache, if any, and attempts to initialize the new .vrmesh file
///        cache capacity is defined by playback range at the time of initialization
/// @param filepath - path to the .vrmesh file
/// @return VUtils::ErrorCode - no error if initialized successfully
///                           - DE_INVALID_FILE if file initialization fails
	VUtils::ErrorCode init(const VUtils::CharString &filepath);

/// @brief Clears cache and deletes current .vrmesh file, if any
	void reset();

/// @brief Clears cache
	void clearCache();

/// @brief Checks if a frame is cached
/// @param context - contains evaluation time information i.e. the frame
/// @param opParams - contains node parameters necessary to map the frame to a .vrmesh frame index
///	@return true - frame is cached(all geometry for that frame is present in the geometry cache)
///         false - otherwise
///         NOTE: if frame is cached but a geometry for that frame is missing
///               removes the cached frame and returns false
///               (could happen if the geometry was evicted from the geometry cache)
	int checkFrameCached(const VRayProxyParms &options) const;

/// @brief Merges the geometry for a frame into the GU_Detail passed
///        if the frame is not in cache loads the preview geometry for that frame
///        and caches it
/// @param context - contains evaluation time information i.e. the frame
/// @param opParams - contains node parameters necessary to map the frame to a .vrmesh frame index
/// @return VUtils::ErrorCode - no error if successful
///                           - DE_INVALID_FILE if cache is not initialized
///                           - DE_NO_GEOM if no preview geometry is found for that frame
///                           - DE_INVALID_GEOM if a cached geometry for that frame is invalid
	GU_ConstDetailHandle getFrame(const VRayProxyParms &options);

private:
	int    checkCached(const FrameKey &frameIdx, const LOD &lod) const;
	int    insert(const FrameKey &frameIdx, const LOD &lod, const std::vector<Geometry> &geometry);
	int    erase(const FrameKey &frameIdx);
	void   evictFrame(const FrameKey &frameIdx, CachedFrame &frameData);

	FrameKey             getFrameIdx(const VRayProxyParms &options) const;
	VUtils::MeshVoxel*   getVoxel(const FrameKey &frameKey, int voxelIdx) const;
	void                 getPreviewGeometry(VUtils::MeshVoxel &voxel, std::vector<Geometry> &geometry) const;
	int                  createProxyGeometry(const Geometry &geom, GU_DetailHandle &gdpHndl) const;
	int                  createMeshProxyGeometry(VUtils::MeshVoxel &voxel, GU_DetailHandle &gdpHndl) const;
	int                  createHairProxyGeometry(VUtils::MeshVoxel &voxel, GU_DetailHandle &gdpHndl) const;
	void                 createBBoxGeometry(const FrameKey &frameKey, GU_Detail &gdp) const;

private:
	VRayProxyCache(const VRayProxyCache &other);
	VRayProxyCache & operator =(const VRayProxyCache &other);

private:
	VUtils::CharString m_filepath;
	VUtils::MeshFile *m_proxy;
	std::shared_ptr<FrameCache> m_frameCache;
	std::shared_ptr<ItemCache>  m_itemCache;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAYPROXYCACHE_H
