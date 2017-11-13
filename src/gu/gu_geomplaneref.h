//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_GEOMPLANEREF_H
#define VRAY_FOR_HOUDINI_GEOMPLANEREF_H

#include "vfh_includes.h"
#include "vfh_GeomPlaneRefOptions.h"

#include <GU/GU_PackedImpl.h>

namespace VRayForHoudini {

class GeomPlaneRef
	: public GU_PackedImpl
	, public GeomPlaneRefOptions
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
		, m_dirty(false)
	{}
	GeomPlaneRef(const GeomPlaneRef &src)
		: GU_PackedImpl()
		, GeomPlaneRefOptions(src)
		, m_detail()
		, m_dirty(false)
	{}
	virtual ~GeomPlaneRef() {
		clearDetail();
	}

	/// Get the factory associated with this procedural
	GU_PackedFactory *getFactory() const VRAY_OVERRIDE;

	/// Create a copy of this resolver
	GU_PackedImpl *copy() const VRAY_OVERRIDE;

	/// Test whether the deferred load primitive data is valid
	bool isValid() const VRAY_OVERRIDE;

	/// The clearData() method is called when the primitives are stashed during
	/// the cook of a SOP.  See GA_Primitive::stashed().  This gives the
	/// primitive to optionally clear some data during the stashing process.
	void clearData() VRAY_OVERRIDE;

#if HDK_16_5
	void update(GU_PrimPacked *prim, const UT_Options &options) VRAY_OVERRIDE { updateFrom(options); }
	bool load(GU_PrimPacked *prim, const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE { return updateFrom(options); }
#else
	bool load(const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE { return updateFrom(options); }
	void update(const UT_Options &options) VRAY_OVERRIDE { updateFrom(options); }
#endif

	/// Copy the resolver data into the UT_Options for saving
	bool save(UT_Options &options, const GA_SaveMap &map) const VRAY_OVERRIDE;

	/// Get the bounding box for the geometry (not including transforms)
	bool getBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;

	/// Get the rendering bounding box for the geometry (not including
	/// transforms).  For curve and point geometry, this needs to include any
	/// "width" attributes.
	bool getRenderingBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;

	/// When rendering with velocity blur, the renderer needs to know the
	/// bounds on velocity to accurately compute the bounding box.
	void getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const VRAY_OVERRIDE;

	/// When rendering points or curves, the renderer needs to know the bounds
	/// on the width attribute to accurately compute the bounding box.
	void getWidthRange(fpreal &wmin, fpreal &wmax) const VRAY_OVERRIDE;

	bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;

	/// Get a reference to a const GU_Detail for the packed geometry to display in vewport.
	/// @param context[in/out] - read/write primitive shared data if necessary
	GU_ConstDetailHandle getPackedDetail(GU_PackedContext *context = 0) const VRAY_OVERRIDE;

	/// Report memory usage (includes all shared memory)
	int64 getMemoryUsage(bool inclusive) const VRAY_OVERRIDE;

	/// Count memory usage using a UT_MemoryCounter in order to count
	/// shared memory correctly.
	/// NOTE: There's nothing outside of sizeof(*this) to count in the
	///       base class, so it can be pure virtual.
	void countMemory(UT_MemoryCounter &counter, bool inclusive) const VRAY_OVERRIDE;

public:
	/// Get/set detail handle for this primitive
	const GU_ConstDetailHandle& getDetail() const { return m_detail; }
	GeomPlaneRef& setDetail(const GU_ConstDetailHandle &h) { m_detail = h; return *this; }
	
private:
	void clearDetail() {
		m_detail = GU_ConstDetailHandle();
	}

	bool updateFrom(const UT_Options &options);

	GU_ConstDetailHandle   m_detail;  ///< detail handle for viewport geometry
	int                    m_dirty;   ///< flags if the detail handle needs update
}; // !GeomPlaneRef

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_GEOMPLANEREF_H
