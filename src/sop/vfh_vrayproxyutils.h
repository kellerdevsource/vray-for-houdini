//
// Copyright (c) 2015-2016, Chaos Software Ltd
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
#include <UT/UT_BoundingBox.h>
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
bool                   GetVRayProxyBounds(const VRayProxyParms &options, UT_BoundingBox &box);


class VRayProxyParms
{
public:
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


	static UT_StringHolder getFilepath(const UT_Options &options)
	{
		return ((options.hasOption(theFileToken))? options.getOptionS(theFileToken) : UT_StringHolder());
	}

	static exint getLOD(const UT_Options &options)
	{
		return ((options.hasOption(theLODToken))? options.getOptionI(theLODToken) : LOD_PREVIEW);
	}

	static fpreal64 getFloatFrame(const UT_Options &options)
	{
		return ((options.hasOption(theFrameToken))? options.getOptionF(theFrameToken) : 0.f);
	}

	static exint getAnimType(const UT_Options &options)
	{
		return ((options.hasOption(theAnimTypeToken))? options.getOptionI(theAnimTypeToken) : VUtils::MeshFileAnimType::Loop);
	}

	static fpreal64 getAnimOffset(const UT_Options &options)
	{
		return ((options.hasOption(theAnimOffsetToken))? options.getOptionF(theAnimOffsetToken) : 0.f);
	}

	static fpreal64 getAnimSpeed(const UT_Options &options)
	{
		return ((options.hasOption(theAnimSpeedToken))? options.getOptionF(theAnimSpeedToken) : 1.f);
	}

	static bool getAnimOverride(const UT_Options &options)
	{
		return ((options.hasOption(theAnimOverrideToken))? options.getOptionB(theAnimOverrideToken) : 0);
	}

	static exint getAnimStart(const UT_Options &options)
	{
		return ((options.hasOption(theAnimStartToken))? options.getOptionI(theAnimStartToken) : 0);
	}

	static exint getAnimLength(const UT_Options &options)
	{
		return ((options.hasOption(theAnimLengthToken))? options.getOptionI(theAnimLengthToken) : 0);
	}

	static fpreal64 getScale(const UT_Options &options)
	{
		return ((options.hasOption(theScaleToken))? options.getOptionF(theScaleToken) : 1.f);
	}

	static exint getFlipAxis(const UT_Options &options)
	{
		return ((options.hasOption(theFlipAxisToken))? options.getOptionI(theFlipAxisToken) : 0);
	}

public:
	VRayProxyParms()
	{ }
	VRayProxyParms(const UT_Options &options):
		m_options(options)
	{ }
	~VRayProxyParms()
	{ }

	VRayProxyParms& operator =(const UT_Options &options)
	{ m_options = options; return *this; }
	bool            operator ==(const UT_Options &options) const
	{ return (m_options == options); }
	bool            operator ==(const VRayProxyParms &other) const
	{ return (m_options == other.m_options); }

	inline const UT_Options & getOptions() const { return m_options; }
	inline UT_StringHolder    getFilepath() const { return getFilepath(m_options); }
	inline exint              getLOD() const { return getLOD(m_options); }
	inline fpreal64           getFloatFrame() const { return getFloatFrame(m_options); }
	inline exint              getAnimType() const { return getAnimType(m_options); }
	inline fpreal64           getAnimOffset() const { return getAnimOffset(m_options); }
	inline fpreal64           getAnimSpeed() const { return getAnimSpeed(m_options); }
	inline bool               getAnimOverride() const { return getAnimOverride(m_options); }
	inline exint              getAnimStart() const { return getAnimStart(m_options); }
	inline exint              getAnimLength() const { return getAnimLength(m_options); }
	inline fpreal64           getScale() const { return getScale(m_options); }
	inline exint              getFlipAxis() const { return getFlipAxis(m_options); }

private:
	UT_Options      m_options;
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
		inline bool     hasDetail(const LOD &lod) const { return (m_details.count(lod) > 0); }
		inline Item &   getDetail(const LOD &lod) { return m_details[lod]; }

		bool hasItemKeys(const LOD &lod) const { return (m_keys.count(lod) > 0); }
		ItemKeys &getItemKeys(const LOD &lod) { return m_keys[lod]; }

		std::unordered_map< LOD, Item, std::hash<int>, std::equal_to<int> >     m_details;
		std::unordered_map< LOD, ItemKeys, std::hash<int>, std::equal_to<int> > m_keys;
		VUtils::Box                                                             m_bbox;
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
	VUtils::ErrorCode    init(const VUtils::CharString &filepath);

/// @brief Clears cache and deletes current .vrmesh file, if any
	void                 reset();

/// @brief Clears cache
	void                 clearCache();

/// @brief Checks if a frame is cached
/// @param context - contains evaluation time information i.e. the frame
/// @param opParams - contains node parameters necessary to map the frame to a .vrmesh frame index
///	@return true - frame is cached(all geometry for that frame is present in the geometry cache)
///         false - otherwise
///         NOTE: if frame is cached but a geometry for that frame is missing
///               removes the cached frame and returns false
///               (could happen if the geometry was evicted from the geometry cache)
	int                  isCached(const VRayProxyParms &options) const;

/// @brief Merges the geometry for a frame into the GU_Detail passed
///        if the frame is not in cache loads the preview geometry for that frame
///        and caches it
/// @param context - contains evaluation time information i.e. the frame
/// @param opParams - contains node parameters necessary to map the frame to a .vrmesh frame index
/// @return VUtils::ErrorCode - no error if successful
///                           - DE_INVALID_FILE if cache is not initialized
///                           - DE_NO_GEOM if no preview geometry is found for that frame
///                           - DE_INVALID_GEOM if a cached geometry for that frame is invalid
	GU_ConstDetailHandle getDetail(const VRayProxyParms &options);
	bool                 getBounds(const VRayProxyParms &options, UT_BoundingBox &box) const;

private:
	int    contains(const FrameKey &frameIdx, const LOD &lod) const;
	int    insert(const FrameKey &frameIdx, const LOD &lod, const std::vector<Geometry> &geometry);
	int    erase(const FrameKey &frameIdx);
	void   evictFrame(const FrameKey &frameIdx, CachedFrame &frameData);

	FrameKey             getFrameIdx(const VRayProxyParms &options) const;
	VUtils::MeshVoxel*   getVoxel(const FrameKey &frameKey, int voxelIdx) const;
	void                 getPreviewGeometry(VUtils::MeshVoxel &voxel, std::vector<Geometry> &geometry) const;
	int                  createProxyGeometry(const Geometry &geom, GU_DetailHandle &gdpHndl) const;
	int                  createMeshProxyGeometry(VUtils::MeshVoxel &voxel, GU_DetailHandle &gdpHndl) const;
	int                  createHairProxyGeometry(VUtils::MeshVoxel &voxel, GU_DetailHandle &gdpHndl) const;
	bool                 createBBoxGeometry(const FrameKey &frameKey, GU_Detail &gdp) const;

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
