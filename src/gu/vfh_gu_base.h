//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VFH_GU_BASE_H
#define VRAY_FOR_HOUDINI_VFH_GU_BASE_H

#include "vfh_includes.h"
#include "vfh_vray.h"

#include <GU/GU_DetailHandle.h>
#include <GU/GU_PackedImpl.h>
#include <UT/UT_Options.h>
#include <UT/UT_BoundingBox.h>

namespace VRayForHoudini {

class VRayBaseRef
	: public GU_PackedImpl
{
public:
	VRayBaseRef();
	VRayBaseRef(const VRayBaseRef &other);
	VRayBaseRef(VRayBaseRef &&other) noexcept;

	virtual ~VRayBaseRef() {}

	// From GU_PackedImpl.
	bool isValid() const VRAY_OVERRIDE;
	void clearData() VRAY_OVERRIDE;
	bool save(UT_Options &options, const GA_SaveMap &map) const VRAY_OVERRIDE;
	bool getBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;
	bool getRenderingBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;
	void getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const VRAY_OVERRIDE;
	void getWidthRange(fpreal &wmin, fpreal &wmax) const VRAY_OVERRIDE;
#if HDK_16_5
	void update(GU_PrimPacked *prim, const UT_Options &options) VRAY_OVERRIDE { updateFrom(options); }
	bool load(GU_PrimPacked *prim, const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE { return updateFrom(options); }
#else
	bool load(const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE { return updateFrom(options); }
	void update(const UT_Options &options) VRAY_OVERRIDE { updateFrom(options); }
#endif
	bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;
	GU_ConstDetailHandle getPackedDetail(GU_PackedContext *context = 0) const VRAY_OVERRIDE;
	int64 getMemoryUsage(bool inclusive) const VRAY_OVERRIDE;
	void countMemory(UT_MemoryCounter &counter, bool inclusive) const VRAY_OVERRIDE;

private:
	/// Rebuilds detail geometry.
	virtual void detailRebuild()=0;

protected:
	/// Clear detail.
	void detailClear();

	/// Updates detail from options if needed.
	/// @param options New options set.
	/// @returns True if detail was changed, false - otherwise.
	virtual int updateFrom(const UT_Options &options);

	/// Current options set.
	UT_Options m_options;

	/// Bounding box.
	UT_BoundingBox m_bbox;

	/// Geometry detail.
	GU_ConstDetailHandle m_detail;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VFH_GU_BASE_H
