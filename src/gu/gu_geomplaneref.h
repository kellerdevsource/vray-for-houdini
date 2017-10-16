//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_GEOMPLANEREF_H
#define VRAY_FOR_HOUDINI_GEOMPLANEREF_H

#include <GU/GU_PackedImpl.h>
#include "vfh_vray.h"

namespace VRayForHoudini
{

class GeomPlaneRef
	: public GU_PackedImpl
{
public:
	/// Get the type ID for the VRaySceneRef primitive.
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
	GeomPlaneRef()
		: GU_PackedImpl()
		, m_detail()
		, m_options()
		, m_dirty(false)
	{}
	GeomPlaneRef(const GeomPlaneRef &src)
		: GU_PackedImpl()
		, m_detail()
		, m_options()
		, m_dirty(false)
	{}
	virtual ~GeomPlaneRef()
	{
		clearDetail();
	}

public:
	/// Get the factory associated with this procedural
	virtual GU_PackedFactory *getFactory() const VRAY_OVERRIDE;

	/// Create a copy of this resolver
	virtual GU_PackedImpl *copy() const VRAY_OVERRIDE;

	/// Test whether the deferred load primitive data is valid
	virtual bool isValid() const VRAY_OVERRIDE;

	/// The clearData() method is called when the primitives are stashed during
	/// the cook of a SOP.  See GA_Primitive::stashed().  This gives the
	/// primitive to optionally clear some data during the stashing process.
	virtual void clearData() VRAY_OVERRIDE;

	/// Give a UT_Options of load data, create resolver data for the primitive
	virtual bool load(const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE;

	/// Depending on the update, the procedural should call one of:
	///	- transformDirty()
	///	- attributeDirty()
	///	- topologyDirty()
	virtual void update(const UT_Options &options) VRAY_OVERRIDE;

	/// Copy the resolver data into the UT_Options for saving
	virtual bool save(UT_Options &options, const GA_SaveMap &map) const VRAY_OVERRIDE;

	/// Get the bounding box for the geometry (not including transforms)
	virtual bool getBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;

	/// Get the rendering bounding box for the geometry (not including
	/// transforms).  For curve and point geometry, this needs to include any
	/// "width" attributes.
	virtual bool getRenderingBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;

	/// When rendering with velocity blur, the renderer needs to know the
	/// bounds on velocity to accurately compute the bounding box.
	virtual void getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const VRAY_OVERRIDE;

	/// When rendering points or curves, the renderer needs to know the bounds
	/// on the width attribute to accurately compute the bounding box.
	virtual void getWidthRange(fpreal &wmin, fpreal &wmax) const VRAY_OVERRIDE;

	virtual bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;

	/// Get a reference to a const GU_Detail for the packed geometry to display in vewport.
	/// @param context[in/out] - read/write primitive shared data if necessary
	virtual GU_ConstDetailHandle getPackedDetail(GU_PackedContext *context = 0) const VRAY_OVERRIDE;


	/// Report memory usage (includes all shared memory)
	virtual int64 getMemoryUsage(bool inclusive) const VRAY_OVERRIDE;

	/// Count memory usage using a UT_MemoryCounter in order to count
	/// shared memory correctly.
	/// NOTE: There's nothing outside of sizeof(*this) to count in the
	///       base class, so it can be pure virtual.
	virtual void countMemory(UT_MemoryCounter &counter, bool inclusive) const VRAY_OVERRIDE;

public:
	fpreal get_plane_size() const
	{
		return m_options.hasOption("plane_size") ? m_options.getOptionF("plane_size") : 0;
	}
	void set_plane_size(fpreal plane_size)
	{
		m_options.setOptionF("plane_size", plane_size);
	}

	const UT_Options& getOptions() const
	{
		return m_options;
	}

public:
	/// Get/set detail handle for this primitive
	const GU_ConstDetailHandle& getDetail() const { return m_detail; }
	GeomPlaneRef& setDetail(const GU_ConstDetailHandle &h) { m_detail = h; return *this; }
	
private:
	void clearDetail()
	{
		m_detail = GU_ConstDetailHandle();
	}
	bool updateFrom(const UT_Options &options);

private:
	GU_ConstDetailHandle   m_detail;  ///< detail handle for viewport geometry
	UT_Options             m_options; ///< holds params for plane
	int                    m_dirty;   ///< flags if the detail handle needs update
}; // !GeomPlaneRef

} // !VRayForHoudini

#endif //!VRAY_FOR_HOUDINI_GEOMPLANEREF_H