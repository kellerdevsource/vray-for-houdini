//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAYPROXYREF_H
#define VRAY_FOR_HOUDINI_VRAYPROXYREF_H

#include "vfh_vray.h"
#include "vfh_vrayproxyutils.h"
#include <GU/GU_PackedImpl.h>


namespace VRayForHoudini {


/// Custom primitive that represents V-Ray proxy file in Houdini
/// implemented as packed primitive
class VRayProxyRef:
		public GU_PackedImpl
{
public:
	/// Get the type ID for the VRayProxy primitive.
	static GA_PrimitiveTypeId typeId() { return theTypeId; }

	/// Register the primitive with gafactory
	/// @param gafactory[out] - Houdini primitive factory for custom plugin primitives
	static void install(GA_PrimitiveFactory *gafactory);

private:
	/// type id for this primitive
	/// custom primitive types receive their type id at runtime
	/// when registering the primitive as opposed to houdini native primitives
	/// which are known at compile time
	static GA_PrimitiveTypeId theTypeId;

public:
	VRayProxyRef();
	VRayProxyRef(const VRayProxyRef &src);
	virtual ~VRayProxyRef();

	/// Get our factory instance
	virtual GU_PackedFactory* getFactory() const VRAY_OVERRIDE;

	/// Create copy of this primitive
	virtual GU_PackedImpl* copy() const VRAY_OVERRIDE { return new VRayProxyRef(*this); }

	 /// Test whether the deferred load primitive data is valid
	virtual bool isValid() const VRAY_OVERRIDE { return m_detail.isValid(); }

	/// The clearData() method is called when the primitives are stashed during
	/// the cook of a SOP.  See GA_Primitive::stashed().  This gives the
	/// primitive to optionally clear some data during the stashing process.
	virtual void clearData() VRAY_OVERRIDE;

	/// Load proxy primitive from options passed as UT_Options
	/// @param options[in] - options for the proxy primitive
	/// @param map[in] - load options (for shared primititve data)
	///                  not used
	/// @retval true on success
	virtual bool load(const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE
	{ return updateFrom(options); }

	/// Update proxy primitive from options passed as UT_Options
	/// @param options[in] - options for the proxy primitive
	virtual void update(const UT_Options &options) VRAY_OVERRIDE
	{ updateFrom(options); }

	/// Save proxy primitive as UT_Options
	/// @param options[out] - options for the proxy primitive
	/// @param map[in] - save options (for shared primititve data) - not used
	/// @retval true on success
	virtual bool save(UT_Options &options, const GA_SaveMap &map) const VRAY_OVERRIDE;

	/// Proxy primitive has some options that modify its "intrinsic" transform.
	/// It is combined with the local transform on the primitive.
	/// @param m[out] - local trasform for the primitive
	/// @retval true if local transform is modified
	virtual bool getLocalTransform(UT_Matrix4D &m) const VRAY_OVERRIDE;

	/// Get bounding box for this primitive (not including transforms)
	/// @param box[out] - bounds
	/// @retval true if box has been modified
	virtual bool getBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;

	/// Get the rendering bounding box for the geometry (not including
	/// transforms).
	/// @param box[out] - bounds
	/// @retval true if box has been modified
	virtual bool getRenderingBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;

	/// Determines if we should save the bounding box to the metadata, this
	/// makes sense for fully embedded primitives, but if the primitive
	/// can be swapped out without the owner knowing, it should be
	/// avoided.
	virtual bool saveCachedBBox() const VRAY_OVERRIDE { return true; }

	/// When rendering with velocity blur, the renderer needs to know the
	/// bounds on velocity to accurately compute the bounding box
	/// @param min - min velocity
	/// @param max - max velocity
	virtual void getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const VRAY_OVERRIDE;

	/// When rendering points or curves, the renderer needs to know the bounds
	/// on the width attribute to accurately compute the bounding box.
	/// @param min - min value
	/// @param max - max value
	virtual void getWidthRange(fpreal &min, fpreal &max) const VRAY_OVERRIDE;

	/// Unpack the procedural into a GU_Detail.
	/// @param destgdp[out] - output geometry
	/// @retval true on success
	virtual bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;

	/// Get a reference to a const GU_Detail for the packed geometry to display in vewport.
	/// @param context[in/out] - read/write primitive shared data if necessary
	virtual GU_ConstDetailHandle   getPackedDetail(GU_PackedContext *context = 0) const VRAY_OVERRIDE;

	/// Report memory usage (includes all shared memory)
	/// @param inclusive[in] - if memory used by this primitive (i.e. sizeof(*this))
	///                        should be included
	virtual int64 getMemoryUsage(bool inclusive) const VRAY_OVERRIDE;

	/// Count memory usage using a UT_MemoryCounter in order to count
	/// shared memory correctly.
	/// @param conter[in] - counter for shared memory among all primitives of this type
	/// @param inclusive[in] - if memory used by this primitive (i.e. sizeof(*this))
	///                        should be included
	virtual void countMemory(UT_MemoryCounter &counter, bool inclusive) const VRAY_OVERRIDE;

	/// Get/set detail handle for this primitive
	const GU_ConstDetailHandle& getDetail() const { return m_detail; }
	VRayProxyRef& setDetail(const GU_ConstDetailHandle &h) { m_detail = h; return *this; }

	/// Get relevant proxy plugin params for this primitive
	inline const UT_Options& getOptions() const { return m_options.getOptions(); }

	/// Member data accessors for primitive intrinsics
	///
	inline exint getGeometryid() const;

	inline const char* getFilepath() const { return m_options.getFilepath(); }
	inline void setFilepath(const char *filepath);

	inline exint getLOD() const { return m_options.getLOD(); }
	inline void setLOD(exint lod);
	inline const char* getLODName() const;

	inline exint getAnimType() const { return m_options.getAnimType(); }
	inline void setAnimType(exint animType);
	inline const char* getAnimTypeName() const;

	inline fpreal64 getAnimOffset() const { return m_options.getAnimOffset(); }
	inline void setAnimOffset(fpreal64 offset);

	inline fpreal64 getAnimSpeed() const { return m_options.getAnimSpeed(); }
	inline void setAnimSpeed(fpreal64 speed);

	inline bool getAnimOverride() const { return m_options.getAnimOverride(); }
	inline void setAnimOverride(bool override);

	inline exint getAnimStart() const { return m_options.getAnimStart(); }
	inline void setAnimStart(exint start);

	inline exint getAnimLength() const { return m_options.getAnimLength(); }
	inline void setAnimLength(exint length);

	inline fpreal64 getScale() const { return m_options.getScale(); }
	inline void setScale(fpreal64 scale);

	inline exint getFlipAxis() const { return m_options.getFlipAxis(); }
	inline void setFlipAxis(exint flip);

private:
	/// Updates primitive options from UT_Options
	/// and flags detail for update if necessary
	template <typename T>
	bool   updateFrom(const T &options);

	/// Clear detail handle
	void   clearDetail() { m_detail = GU_ConstDetailHandle(); }

private:
	GU_ConstDetailHandle   m_detail; ///< detail handle for viewport geometry
	VRayProxyParms         m_options; ///< holds all relevant proxy plugin params
	int                    m_dirty; ///< flags if the detail handle needs update
};


} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAYPROXYREF_H
