//
// Copyright (c) 2015-2017, Chaos Software Ltd
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

/// Level of detail types for viewport display of .vrmesh geometry
/// Proxy cache manager will load and cache only the data needed
/// to display the corresponding level of detail
enum LOD {
	LOD_BBOX = 0, ///< Load and display bbox of the geometry
	LOD_PREVIEW, ///< Load and display geometry in the preview voxel
	LOD_FULL, ///< Load and display full geometry
};

class VRayProxyParms;
class VRayProxyCache;
typedef Caches::LRUCache< std::string, VRayProxyCache > VRayProxyCacheMan;


/// Get the global instance of proxy cache manager
/// @retval the proxy cache manager
VRayProxyCacheMan& GetVRayProxyCacheManager();

/// Get detail handle for a proxy packed primitive.
/// @note internally this will go through the proxy cache manager
/// @param[in] options - UT_Options for proxy packed primitive
/// @retval the detail handle for the primitive (based on file,
///         frame, LOD in options)
GU_ConstDetailHandle GetVRayProxyDetail(const VRayProxyParms &options);

/// Get the bounding box for a proxy packed primitive.
/// @note internally this will go through the proxy cache manager
///       bbox is cached on first read access for detail handle
/// @param[in] options - UT_Options for proxy packed primitive
/// @param[out] box - the bounding box for all geometry
/// @retval true if successful i.e this data is found cached
bool GetVRayProxyBounds(const VRayProxyParms &options, UT_BoundingBox &box);

/// Clear cached data for a given file
/// @note internally this will go through the proxy cache manager
/// @param[in] utfilepath - proxy filepath
/// @retval true if cached file is deleted
bool ClearVRayProxyCache(const UT_String &utfilepath);


/// Wrapper around UT_Options that provides interface to query options
/// specific to viewport display of VRayProxyRef primitives
class VRayProxyParms
{
public:
	static const UT_StringRef theLODToken;
	static const UT_StringRef theFileToken;
	static const UT_StringRef theAnimTypeToken;
	static const UT_StringRef theAnimOffsetToken;
	static const UT_StringRef theAnimSpeedToken;
	static const UT_StringRef theAnimOverrideToken;
	static const UT_StringRef theAnimStartToken;
	static const UT_StringRef theAnimLengthToken;
	static const UT_StringRef theScaleToken;
	static const UT_StringRef theFlipAxisToken;


	/// Get the proxy filepath stored in UT_Options
	/// @param[in] options - UT_Options for the proxy packed primitive
	static UT_StringHolder getFilepath(const UT_Options &options)
	{
		return ((options.hasOption(theFileToken))? options.getOptionS(theFileToken) : UT_StringHolder());
	}

	/// Get the LOD stored in UT_Options
	/// @param[in] options - UT_Options for the proxy packed primitive
	static exint getLOD(const UT_Options &options)
	{
		return ((options.hasOption(theLODToken))? options.getOptionI(theLODToken) : LOD_PREVIEW);
	}

	/// Get animation type stored in UT_Options - Loop,Once,PingPong,Still
	/// @param[in] options - UT_Options for the proxy packed primitive
	static exint getAnimType(const UT_Options &options)
	{
		return ((options.hasOption(theAnimTypeToken))? options.getOptionI(theAnimTypeToken) : VUtils::MeshFileAnimType::Loop);
	}

	/// Get animation offset stored in UT_Options
	/// @param[in] options - UT_Options for the proxy packed primitive
	static fpreal64 getAnimOffset(const UT_Options &options)
	{
		return ((options.hasOption(theAnimOffsetToken))? options.getOptionF(theAnimOffsetToken) : 0.f);
	}

	/// Get animation speed stored in UT_Options
	/// @param[in] options - UT_Options for the proxy packed primitive
	static fpreal64 getAnimSpeed(const UT_Options &options)
	{
		return ((options.hasOption(theAnimSpeedToken))? options.getOptionF(theAnimSpeedToken) : 1.f);
	}

	/// Check animation override stored in UT_Options
	/// if on anim start and anim length will overwrite the playback for the proxy
	/// @param[in] options - UT_Options for the proxy packed primitive
	static bool getAnimOverride(const UT_Options &options)
	{
		return ((options.hasOption(theAnimOverrideToken))? options.getOptionB(theAnimOverrideToken) : 0);
	}

	/// Get animation start offset stored in UT_Options
	/// @param[in] options - UT_Options for the proxy packed primitive
	static exint getAnimStart(const UT_Options &options)
	{
		return ((options.hasOption(theAnimStartToken))? options.getOptionI(theAnimStartToken) : 0);
	}

	/// Get animation length stored in UT_Options
	/// @param[in] options - UT_Options for the proxy packed primitive
	static exint getAnimLength(const UT_Options &options)
	{
		return ((options.hasOption(theAnimLengthToken))? options.getOptionI(theAnimLengthToken) : 0);
	}

	/// Get scale stored in UT_Options for the proxy primitive
	/// @param[in] options - UT_Options for the proxy packed primitive
	static fpreal64 getScale(const UT_Options &options)
	{
		return ((options.hasOption(theScaleToken))? options.getOptionF(theScaleToken) : 1.f);
	}

	/// Check whether to flip the z axis for the proxy primitive
	/// @param[in] options - UT_Options for the proxy packed primitive
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

	bool operator ==(const UT_Options &options) const
	{ return (m_options == options); }

	bool operator ==(const VRayProxyParms &other) const
	{ return (m_options == other.m_options); }

	/// Get the actual options
	inline const UT_Options& getOptions() const { return m_options; }
	inline UT_Options& getOptions() { return m_options; }

	/// Get the proxy filepath
	inline UT_StringHolder getFilepath() const { return getFilepath(m_options); }

	/// Get the viewport LOD
	inline exint getLOD() const { return getLOD(m_options); }

	/// Get animation type - Loop,Once,PingPong,Still
	inline exint getAnimType() const { return getAnimType(m_options); }

	/// Get animation offset
	inline fpreal64 getAnimOffset() const { return getAnimOffset(m_options); }

	/// Get animation speed stored in UT_Options
	inline fpreal64 getAnimSpeed() const { return getAnimSpeed(m_options); }

	/// Check animation override
	/// if on anim start and anim length will overwrite the playback for the proxy
	inline bool getAnimOverride() const { return getAnimOverride(m_options); }

	/// Get animation start offset
	inline exint getAnimStart() const { return getAnimStart(m_options); }

	/// Get animation length
	inline exint getAnimLength() const { return getAnimLength(m_options); }

	/// Get scale for the proxy primitive
	inline fpreal64 getScale() const { return getScale(m_options); }

	/// Check whether to flip the z axis for the proxy primitive
	inline exint getFlipAxis() const { return getFlipAxis(m_options); }

private:
	UT_Options m_options; ///< UT_Options for the proxy packed primitive
};


/// VRayProxyCache caches in memory .vrmesh geometry and some meta data per frame for faster playback.
/// VRayProxyCache combines 2 related, but separate lru caches:
/// 1. a frame cache which stores some meta data about the geo available on that frame
/// 2. a geo cache for the actual geometry
/// When caching a single frame, its geometry is cached in a separate geo cache as
/// Houdini GU_Detail. Voxel geometry is decomposed into mesh, hair and particles details,
/// stored individually in the geo cache. These are then combined into single detail using
/// packed geometry primitives and the latter is also added to the geo cache. Detail unique id
/// is used as key in the geo cache to idetify the correct handle. This is done in order to be able to
/// store duplicate geometry and across different frames only once.
/// @note
///      1. currently geometry is hashed using vertex positions only(may change in future)
///      2. currently cache capacity is defined by playback range at the time of initialization(may change in future)
class VRayProxyCache
{
private:
	typedef unsigned             FrameKey;
	typedef exint                DetailKey;
	typedef Hash::MHash          HashKey;
	typedef std::vector<HashKey> HashKeys;

	typedef uint32                                     VoxelType;
	typedef std::pair<VoxelType, VUtils::MeshVoxel* >  Geometry;


	/// Helper structure to hash voxel geometry
	struct GeometryHash
	{
		typedef Hash::MHash result_type;
		typedef Geometry argument_type;

		result_type operator()(const argument_type &val) const;
	};

	/// Helper data structure that stores frame meta data
	struct CachedFrame
	{
		typedef std::unordered_map< LOD, DetailKey, std::hash<int>, std::equal_to<int> > LODToDetailMap;
		typedef std::unordered_map< LOD, HashKeys, std::hash<int>, std::equal_to<int> >  LODToVoxelMap;

		/// Check if we have cached detail for a given LOD
		/// @param lod[in] - the lod type
		inline bool hasDetailKey(const LOD &lod) const
		{ return (m_lodToDetail.count(lod) > 0); }

		/// Get detail unique id for a given LOD (geo cache key)
		/// @note throws an error if lod has not been cached before
		///       use hasDetailKey() to check for lod first
		/// @param lod[in] - the lod type
		/// @retval detail unique id
		inline const DetailKey& getDetailKey(const LOD &lod) const
		{ return m_lodToDetail.at(lod); }

		/// Set detail unique id for a given LOD
		/// @param lod[in] - the lod type
		/// @param key[in] - the detail unique id
		inline void setDetailKey(const LOD &lod, const DetailKey &key)
		{ m_lodToDetail[lod] = key; }

		/// Check if we have cached geometry hashes for this frame
		/// @param lod[in] - the lod type
		inline bool hasVoxelKeys(const LOD &lod) const
		{ return (m_voxelkeys.count(lod) > 0); }

		/// Get geometry hashes for this frame
		/// @note mapping from geometry hash to detail id is saved in VoxelToDetailMap
		/// @param lod[in] - the lod type
		/// @retval list of geometry hashes
		inline HashKeys& getVoxelKeys(const LOD &lod)
		{ return m_voxelkeys[lod]; }

		/// Erase geometry meta data for a given lod
		/// @param lod[in] - the lod type
		inline void eraseLOD(const LOD &lod)
		{ m_lodToDetail.erase(lod); m_voxelkeys.erase(lod); }


		/// Bounding box for the geometry on this frame
		VUtils::Box    m_bbox;
		/// Map of LOD type to detail unique id. The latter is used to query
		/// the geo cache for correct detail handle
		LODToDetailMap m_lodToDetail;
		/// Map of LOD to list of geometry hashes (calc. from GeometryHash)
		/// These are hashes for the geometry on this frame.
		/// When caching geometry for a frame, it can be split into several details
		/// usually one per voxel when lod == LOD_FULL and/or mesh, hair, particle
		/// details if a voxel contains more that 1 type of geo (as sometimes does the
		/// preview voxel). Each hash corresponds to geometry detail in the geo cache.
		/// Mapping from geometry hash to detail id is saved in VoxelToDetailMap.
		LODToVoxelMap  m_voxelkeys;
	};

	/// Helper data structure to store ref counted geo details
	struct CachedDetail
	{
		DetailKey m_detailKey; ///< detail unique id for the geo cache
		int       m_refCnt; ///< detail ref count
	};

	typedef Caches::LRUCache< FrameKey, CachedFrame, std::hash<FrameKey>, std::equal_to<FrameKey> >        FrameCache;
	typedef Caches::LRUCache< DetailKey, GU_DetailHandle, std::hash<DetailKey>, std::equal_to<DetailKey> > DetailCache;
	typedef std::unordered_map< HashKey, CachedDetail, std::hash<HashKey>, std::equal_to<HashKey> >        VoxelToDetailMap;

public:
	typedef FrameCache::size_type size_type;

	VRayProxyCache();

	/// We can move but not copy V-Ray proxy cache
	VRayProxyCache(VRayProxyCache&& other);
	VRayProxyCache& operator=(VRayProxyCache&& other);

	~VRayProxyCache();

	/// Get cache capacity
	size_type capacity() const { return m_frameCache->capacity(); }

	/// Get number of cached frames
	size_type size() const { return m_frameCache->size(); }

	/// Check if cache is empty
	int empty() const { return (size() == 0); }

	/// Clear previous cache (if any) and try to initialize a new .vrmesh file.
	/// Cache capacity is determined based on the length of the animation saved in file
	/// or Houdini playback range at the time of initialization, whichever is less
	/// @param filepath[in] - path to the .vrmesh file
	/// @return VUtils::ErrorCode - no error if successful
	///                           - DE_INVALID_FILE if file initialization fails
	VUtils::ErrorCode init(const VUtils::CharString &filepath);

	/// Clear cache and reset current .vrmesh file to none
	void reset();

	/// Clear cache
	void clearCache();

	/// Check if geometry for a primitive is cached
	/// @param options[in] - proxy primitive options
	///	@return true if frame is cached (and all geometry for that frame is present in cache)
	///         false - otherwise
	/// @note: if frame is cached but a detail for that frame is missing
	///        cached data will be automatically deleted
	///        (could happen if the detail was evicted from the geo cache)
	int isCached(const VRayProxyParms &options);

	/// Return the detail handle for a proxy packed primitive (based on frame, LOD)
	/// Caches the detail if not present in cache
	/// @param options[in] - proxy primitive options
	/// @return detail handle  if successful or emty one on error
	GU_ConstDetailHandle getDetail(const VRayProxyParms &options);

	/// Return bounding box for a proxy packed primitive
	/// @param options[in] - proxy primitive options
	/// @param bbox[out] - primitive bbox
	/// @return true on success i.e data is found in cache
	bool getBounds(const VRayProxyParms &options, UT_BoundingBox &box) const;

private:
	/// Cache geometry for frame
	/// @param frameIdx[in] - actual .vrmesh frame to cache
	/// @param lod[in] - level of detail for the geometry
	/// @retval true if successful
	bool cache(const FrameKey &frameIdx, const LOD &lod);

	/// Check if geometry for a primitive is cached
	/// @param frameIdx[in] - actual .vrmesh frame
	/// @param lod[in] - level of detail for the geometry
	/// @retval true if found
	bool contains(const FrameKey &frameIdx, const LOD &lod);

	/// Insert geometry in cache for the given frame and lod
	/// @note called from cache()
	/// @param frameIdx[in] - actual .vrmesh frame
	/// @param lod[in] - level of detail for the geometry
	/// @param geometry[in] - list of pairs (voxel type, voxel)
	/// @retval true on success
	bool insert(const FrameKey &frameIdx, const LOD &lod, const std::vector<Geometry> &geometry);

	/// Erase cached data for the given frame
	/// @param frameIdx[in] - actual .vrmesh frame
	/// @retval true if something was deleted
	bool erase(const FrameKey &frameIdx);

	/// Callback when frame is deleted or evicted from cache,
	/// for example when the cache reaches full capacity
	/// @param frameIdx[in] - actual .vrmesh frame
	/// @param frameData[in] - frame meta data
	void evictFrame(const FrameKey &frameIdx, CachedFrame &frameData);

	/// Helper function called when removing frame data from cache
	/// It updates the ref count for details that correspond to the given hashes
	/// and removes details with ref count <= 0  from the geo cache
	/// @param voxelKeys[in] - list of geometry hash keys
	void updateDetailCacheForKeys(const HashKeys &voxelKeys);

	/// Helper function to get the actual frame in the .vrmesh file based on
	/// proxy primitive options
	/// @param options[in] - proxy primitive options
	/// @retval integer frame in the range [0, animation length]
	FrameKey getFrameIdx(const VRayProxyParms &options) const;

	/// Helper function to get voxel at a given frame and index
	/// Used when caching new geometry
	/// @param frameIdx[in] - frame in the range [0, animation length]
	/// @param voxelIdx[int] - voxel index
	/// @retval pointer to the mesh voxel
	VUtils::MeshVoxel* getVoxel(const FrameKey &frameIdx, int voxelIdx) const;

	/// Helper function used to determine what type of geometry a given voxel contains
	/// Used when caching new geometry
	/// @param voxel[in] - mesh voxel
	/// @param geometry[out] - list of (voxel type, voxel) pairs, where
	///                        voxel type is one of
	///                        MVF_GEOMETRY_VOXEL, MVF_HAIR_GEOMETRY_VOXEL,
	///                        MVF_PARTICLE_GEOMETRY_VOXEL, MVF_INSTANCE_VOXEL
	///                        and voxel is the mesh voxel. A pair will be appended
	///                        to the list for every type of geo the voxel contains
	void getPreviewGeometry(VUtils::MeshVoxel &voxel, std::vector<Geometry> &geometry) const;

	/// Add geometry from a given voxel to gdp
	/// @param geoml[in] - (voxel type,voxel) containing the geometry
	/// @param gdp[out] - destination detail for the primitives
	/// @retval true if successful
	bool createProxyGeometry(const Geometry &geom, GU_Detail &gdp) const;

	/// Add mesh geometry from a given voxel to gdp
	/// @note called from createProxyGeometry()
	/// @param voxel[in] - voxel containing mesh geometry
	/// @param gdp[out] - destination detail for the poly primitives
	/// @retval true if successful
	bool createMeshProxyGeometry(VUtils::MeshVoxel &voxel, GU_Detail &gdp) const;

	/// Add hair geometry from a given voxel to gdp
	/// @note called from createProxyGeometry()
	/// @param voxel[in] - voxel containing hair geometry
	/// @param gdp[out] - destination detail for the curve/polyline primitives
	/// @retval true if successful
	bool createHairProxyGeometry(VUtils::MeshVoxel &voxel, GU_Detail &gdp) const;

	/// Add box geometry to gdp
	/// @param bbox[in] - box bounds
	/// @param gdp[out] - destination detail for the box primitive
	/// @retval true if successful
	bool createBBoxGeometry(const VUtils::Box &bbox, GU_Detail &gdp) const;

private:
	/// Disable copy and assignment
	VRayProxyCache(const VRayProxyCache &other);
	VRayProxyCache & operator =(const VRayProxyCache &other);

private:
	VUtils::CharString                m_filepath; ///< proxy filepath
	VUtils::MeshFile                 *m_proxy; ///< actual file interface to read data from
	std::shared_ptr<FrameCache>       m_frameCache; ///< cache for frame meta data
	std::shared_ptr<DetailCache>      m_detailCache; ///< cache for actual geometry
	std::shared_ptr<VoxelToDetailMap> m_voxelToDetail; /// map of geometry hash to detail id used as key in the geo cache
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAYPROXYCACHE_H
