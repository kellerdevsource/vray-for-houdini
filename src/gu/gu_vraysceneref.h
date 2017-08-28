//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAYSCENEREF_H
#define VRAY_FOR_HOUDINI_VRAYSCENEREF_H

#include "systemstuff.h"
#include "GU/GU_PackedImpl.h"

namespace VRayForHoudini {

// Represents VRayScene file
// Implemented as a packed primitive
class VRaySceneRef :
	public GU_PackedImpl
{
public:
	VRaySceneRef();
	VRaySceneRef(const VRaySceneRef &src);
	virtual ~VRaySceneRef();

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
	virtual bool load(const UT_Options &options,
		const GA_LoadMap &map) VRAY_OVERRIDE;
	
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
	virtual void getVelocityRange(UT_Vector3 &min,
		UT_Vector3 &max) const VRAY_OVERRIDE;

	/// When rendering points or curves, the renderer needs to know the bounds
	/// on the width attribute to accurately compute the bounding box.
	virtual void getWidthRange(fpreal &wmin, fpreal &wmax) const VRAY_OVERRIDE;

	virtual bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;

	/// Report memory usage (includes all shared memory)
	virtual int64 getMemoryUsage(bool inclusive) const VRAY_OVERRIDE;

	/// Count memory usage using a UT_MemoryCounter in order to count
	/// shared memory correctly.
	/// NOTE: There's nothing outside of sizeof(*this) to count in the
	///       base class, so it can be pure virtual.
	virtual void countMemory(UT_MemoryCounter &counter, bool inclusive) const VRAY_OVERRIDE;

private:
	void clearDetail();
	bool updateFrom(const UT_Options &options);

private:
	GU_ConstDetailHandle   m_detail; ///< detail handle for viewport geometry
	UT_Options             m_options; ///< holds params for vrscene
	int                    m_dirty; ///< flags if the detail handle needs update
}; // !class VRaySceneRef

} // !namespace VRayForHoudini

#endif // !VRAY_FOR_HOUDINI_VRAYSCENEREF_H