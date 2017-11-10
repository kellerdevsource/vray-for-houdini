//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAYSCENEREF_H
#define VRAY_FOR_HOUDINI_VRAYSCENEREF_H

#include "vfh_includes.h"
#include "vfh_VRaySceneRefOptions.h"

#include <GU/GU_PackedImpl.h>

namespace VUtils {
namespace Vrscene {
namespace Preview {
struct VrsceneDesc;
} // Preview
} // Vrscene
}

namespace VRayForHoudini {

enum FlipAxisMode {
	none = 0,  ///< No flipping
	automatic, ///< Gets the flipping from the vrscene description
	flipZY     ///< Force the scene to flip the Z and Y axis
};

/// VRayScene preview mesh implemented as a packed primitive.
class VRaySceneRef
	: public GU_PackedImpl
	, public VRaySceneRefOptions
{
public:
	static GA_PrimitiveTypeId typeId();
	static void install(GA_PrimitiveFactory *gafactory);

	VRaySceneRef();
	VRaySceneRef(const VRaySceneRef &src);
	~VRaySceneRef();

	GU_PackedFactory *getFactory() const VRAY_OVERRIDE;
	GU_PackedImpl *copy() const VRAY_OVERRIDE;

	bool isValid() const VRAY_OVERRIDE;
	void clearData() VRAY_OVERRIDE;
	bool save(UT_Options &options, const GA_SaveMap &map) const VRAY_OVERRIDE;
#if HDK_16_5
	void update(GU_PrimPacked *prim, const UT_Options &options) VRAY_OVERRIDE { updateFrom(options); }
	bool load(GU_PrimPacked *prim, const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE { return updateFrom(options); }
#else
	bool load(const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE { return updateFrom(options); }
	void update(const UT_Options &options) VRAY_OVERRIDE { updateFrom(options); }
#endif
	bool getBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;
	bool getRenderingBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;
	void getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const VRAY_OVERRIDE;
	void getWidthRange(fpreal &wmin, fpreal &wmax) const VRAY_OVERRIDE;
	bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;
	GU_ConstDetailHandle getPackedDetail(GU_PackedContext *context = 0) const VRAY_OVERRIDE;
	int64 getMemoryUsage(bool inclusive) const VRAY_OVERRIDE;
	void countMemory(UT_MemoryCounter &counter, bool inclusive) const VRAY_OVERRIDE;

	/// Returns mesh sample time based on animation overrides settings.
	/// @param t Current time.
	double getFrame(fpreal t) const;

private:
	/// Build packed detail.
	/// @param vrsceneDesc *.vrscene file preview data.
	/// @param flipAxis Flip axis Z-Y.
	void detailBuild(VUtils::Vrscene::Preview::VrsceneDesc *vrsceneDesc, int flipAxis);

	/// Clear detail.
	void detailClear();

	/// Update detail from options.
	/// @param options New options set.
	/// @returns True if detail was changed, false - otherwise.
	int updateFrom(const UT_Options &options);

	/// Preview mesh bbox.
	UT_BoundingBox m_bbox;

	/// Geometry detail.
	GU_ConstDetailHandle m_detail;
};

} // namespace VRayForHoudini

#endif // !VRAY_FOR_HOUDINI_VRAYSCENEREF_H
