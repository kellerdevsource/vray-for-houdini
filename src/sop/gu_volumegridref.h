//
// Copyright (c) 2015-2016, Chaos Software Ltd
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
#include "vfh_vray.h"
#include "vfh_primitives.h"

#include <aurinterface.h>

#include <GU/GU_PackedImpl.h>

namespace VRayForHoudini {

// these are the parameters (intrisics) that will be exposed to Houdini, all are filled from the associated PhxShaderCache node
// format for each one is (type, name, default_value)
#define VFH_VOLUME_GRID_PARAMS (\
	(exint,  cache_load,   1),\
	(exint,  anim_mode,    0),\
	(fpreal, t2f,          1),\
	(exint,  loop_overlap, 0),\
	(exint,  read_offset,  1),\
	(exint,  play_at,      1),\
	(exint,  max_length,   0),\
	(fpreal, play_speed,   1),\
	(exint,  blend_method, 0),\
	(exint,  load_nearest, 0),\
	(exint,  flip_yz,      0),\
	(const char *, cache_path,  ""),\
	(const char *, usrchmap,  ""),\
	(const char *, cache_path_prefix, ""),\
	(const char *, cache_path_suffix, ""),\
	(exint,        frame_number_width, 1),\
	(fpreal,       current_frame, 0),\
	(const char *, current_cache_path, "")\
	)

#define VFH_VOLUME_GRID_PARAMS_COUNT 18

/// Implementation for a volume grid packed primitive
class VRayVolumeGridRef:
		public GU_PackedImpl
{
	// These *must* match phx's values
	enum AnimationMode {
		Standard = 0,
		DirectIndex = 1,
		Loop = 2,
	};
public:
	typedef std::shared_ptr<IAur> CachePtr;

	VFH_MAKE_ACCESSORS(VFH_VOLUME_GRID_PARAMS, VFH_VOLUME_GRID_PARAMS_COUNT)

	/// Get the type ID for the VRayProxy primitive type.
	static GA_PrimitiveTypeId typeId() { return theTypeId; }
	/// Register this factory
	static void install(GA_PrimitiveFactory *gafactory);

private:
	static GA_PrimitiveTypeId theTypeId; ///< The type id for the primitive

public:
	VRayVolumeGridRef();
	VRayVolumeGridRef(const VRayVolumeGridRef &src);
	virtual ~VRayVolumeGridRef();

	/// @{
	/// Virtual interface from GU_PackedImpl interface
	virtual GU_PackedFactory* getFactory() const VRAY_OVERRIDE;
	virtual GU_PackedImpl*    copy() const VRAY_OVERRIDE { return new VRayVolumeGridRef(*this); }
	virtual bool              isValid() const VRAY_OVERRIDE { return m_detail.isValid(); }
	virtual void              clearData() VRAY_OVERRIDE;

	virtual bool   load(const UT_Options &options, const GA_LoadMap &) VRAY_OVERRIDE
	{ return updateFrom(options); }
	virtual void   update(const UT_Options &options) VRAY_OVERRIDE
	{ updateFrom(options); }
	virtual bool   save(UT_Options &options, const GA_SaveMap &map) const VRAY_OVERRIDE;

	virtual bool                   getLocalTransform(UT_Matrix4D &m) const VRAY_OVERRIDE;
	virtual bool                   getBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;
	virtual bool                   getRenderingBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;
	virtual bool                   saveCachedBBox() const VRAY_OVERRIDE { return true; }
	virtual void                   getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const VRAY_OVERRIDE;
	virtual void                   getWidthRange(fpreal &min, fpreal &max) const VRAY_OVERRIDE;
	virtual bool                   unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;
	virtual GU_ConstDetailHandle   getPackedDetail(GU_PackedContext *context = 0) const VRAY_OVERRIDE;
	/// Report memory usage (includes all shared memory)
	virtual int64                  getMemoryUsage(bool inclusive) const VRAY_OVERRIDE;
	/// Count memory usage using a UT_MemoryCounter in order to count
	/// shared memory correctly.
	virtual void                   countMemory(UT_MemoryCounter &counter, bool inclusive) const VRAY_OVERRIDE;
	/// @}

	/// Create a cache ptr from current settings
	CachePtr                       getCache() const;
	/// Get the world TM
	UT_Matrix4F                    toWorldTm(CachePtr cache) const;

	/// Get all channels present in the current cache
	UT_StringArray                 getCacheChannels() const;

	/// @{
	/// Member data accessors for intrinsics
	const GU_ConstDetailHandle &  getDetail() const { return m_detail; }
	VRayVolumeGridRef&            setDetail(const GU_ConstDetailHandle &h) { m_detail = h; return *this; }

	inline const UT_Options &     getOptions() const { return m_options; }


	exint                         getPhxChannelMapSize() const { return m_options.hasOption("phx_channel_map") ? m_options.getOptionSArray("phx_channel_map").size() : 0; }
	void                          getPhxChannelMap(UT_StringArray &map) const { map = m_options.hasOption("phx_channel_map") ? m_options.getOptionSArray("phx_channel_map") : map; }
	void                          setPhxChannelMap(const UT_StringArray &map) { m_options.setOptionSArray("phx_channel_map", map); }
	/// @}

private:
	/// updateFrom() will update from UT_Options only
	bool updateFrom(const UT_Options &options);
	void clearDetail() { m_detail = GU_ConstDetailHandle(); }

	/// Builds the cache path according to current settings
	/// @param toPhx - if true frame will be replaced with '#'s otherwise with current cache frame
	/// @return - replaced cache path
	std::string getConvertedPath(bool toPhx) const;

	/// Split the path if there is a frame number in it
	/// @path - the cache path
	/// @prefix[out] - everything up to the frame, equal to @path if there is no frame
	/// @suffix[out] - everything after the frame, empty if @path has no frame
	/// @return - the number of digits in the frame (0 if no frame)
	int splitPath(const UT_String & path, std::string & prefix, std::string & suffix) const;

	/// Get current cache frame based on current frame + cache play settings
	int getCurrentCacheFrame() const;

	/// Build channel mapping, should be called after update to cache or ui mappings
	void buildMapping();
private:
	GU_ConstDetailHandle   m_detail; ///< Const detail handle - passed to HDK
	GU_DetailHandle        m_handle; ///< Detail handle - passed to HDK

	UT_Options             m_options; ///< All params from VFH_VOLUME_GRID_PARAMS are defined in this map

	UT_BoundingBox         m_bBox; ///< The volume bounding box
	bool                   m_dirty; ///< True if any parameters that might affect preview are changed
	bool                   m_channelDirty; ///< True if channel mapping have changed since we last built them

	/// if true we will replace frame number with ### for PHX
	/// otherwise user hardcoded frame number and we should not change it
	bool                   m_doFrameReplace;
};


} // namespace VRayForHoudini

#else // CGR_HAS_AUR
#include <GU/GU_PackedImpl.h>
/// Define this empty class here so primitive exporters can be compiled without additional ifdefs there
namespace VRayForHoudini {
class VRayVolumeGridRef: public GU_PackedImpl {
public:
	static GA_PrimitiveTypeId typeId() { return GA_PrimitiveTypeId(); }
};
}
#endif // CGR_HAS_AUR

#endif // VRAY_FOR_HOUDINI_VOLUME_GRID_H
