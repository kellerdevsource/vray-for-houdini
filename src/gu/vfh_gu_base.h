//
// Copyright (c) 2015-2018, Chaos Software Ltd
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
#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GT/GT_GEOPrimCollect.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_PrimInstance.h>
#include <UT/UT_Options.h>
#include <UT/UT_BoundingBox.h>

namespace VRayForHoudini {

class VRayBaseRef
	: public GU_PackedImpl
{
public:
	VRayBaseRef();
	VRayBaseRef(const VRayBaseRef &other);
	virtual ~VRayBaseRef() {}

	// From GU_PackedImpl.
	bool isValid() const VRAY_OVERRIDE;
	void clearData() VRAY_OVERRIDE;
	bool save(UT_Options &options, const GA_SaveMap &map) const VRAY_OVERRIDE;
	bool getBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;
	bool getRenderingBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;
	void getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const VRAY_OVERRIDE;
	void getWidthRange(fpreal &wmin, fpreal &wmax) const VRAY_OVERRIDE;
#ifdef HDK_16_5
	bool load(GU_PrimPacked *prim, const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE { return updateFrom(prim, options); }
	void update(GU_PrimPacked *prim, const UT_Options &options) VRAY_OVERRIDE { updateFrom(prim, options); }
#else
	bool load(const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE { return updateFrom(options); }
	void update(const UT_Options &options) VRAY_OVERRIDE { updateFrom(options); }
#endif
	bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;
	GU_ConstDetailHandle getPackedDetail(GU_PackedContext *context=nullptr) const VRAY_OVERRIDE;
	int64 getMemoryUsage(bool inclusive) const VRAY_OVERRIDE;
	void countMemory(UT_MemoryCounter &counter, bool inclusive) const VRAY_OVERRIDE;

private:
	/// Rebuilds detail geometry.
	/// @returns True if geometry changed.
	virtual int detailRebuild()=0;

protected:
	/// Clear detail.
	void detailClear();

	/// Updates detail from options if needed.
	/// @param options New options set.
	/// @returns True if detail was changed, false - otherwise.
#ifdef HDK_16_5
	virtual int updateFrom(GU_PrimPacked *prim, const UT_Options &options);
#else
	virtual int updateFrom(const UT_Options &options);
#endif

	/// Current options set.
	UT_Options m_options;

	/// Bounding box.
	UT_BoundingBox m_bbox;

	/// Geometry detail.
	GU_DetailHandle m_detail;
};

/// Primitive collection sharing the same underlying detail.
/// @tparam key Detail hash.
/// @tparam value Primitives list.
typedef QMap<uint, GT_GEOOffsetList> DetailToPrimitive;

struct VRayBaseRefCollectData
	: GT_GEOPrimCollectData
{
	explicit VRayBaseRefCollectData(const GA_PrimitiveTypeId &typeId)
		: myPrimTypeId(typeId)
	{}

	/// Returns VRayBaseRef packed primitive type ID.
	GA_PrimitiveTypeId getMyTypeID() const;

	/// Sets VRayBaseRef packed primitive.
	void addPrim(uint key, const GU_PrimPacked *value);

	/// Get primitive collection sharing the same underlying detail.
	const DetailToPrimitive& getPrimitives() const;

private:
	/// VRayBaseRef primitive type ID.
	const GA_PrimitiveTypeId &myPrimTypeId;

	/// Primitive collection based on sharing underlying detail.
	/// We'll instance primitives sharing the same detail.
	DetailToPrimitive detailInstances;

	VUTILS_DISABLE_COPY(VRayBaseRefCollectData)
};

struct VRayBaseRefCollect
	: GT_GEOPrimCollect
{
	static void install(const GA_PrimitiveTypeId &typeId);

	explicit VRayBaseRefCollect(const GA_PrimitiveTypeId &typeId);
	virtual ~VRayBaseRefCollect() = default;

	// From GT_GEOPrimCollect.
	GT_GEOPrimCollectData *beginCollecting(const GT_GEODetailListHandle &, const GT_RefineParms *) const VRAY_OVERRIDE;
	GT_PrimitiveHandle collect(const GT_GEODetailListHandle &,
	                           const GEO_Primitive* const* prim_list,
	                           int,
	                           GT_GEOPrimCollectData *data) const VRAY_OVERRIDE;
	GT_PrimitiveHandle endCollecting(const GT_GEODetailListHandle &geometry,
									 GT_GEOPrimCollectData *data) const VRAY_OVERRIDE;

private:
	const GA_PrimitiveTypeId &typeId;

	VUTILS_DISABLE_COPY(VRayBaseRefCollect)
};

template <typename T>
class VRayBaseRefFactory
	: public GU_PackedFactory
{
public:
	/// Registers the packed primitive factory.
	/// @param primFactory Global primitive factory.
	/// @param instance Factory instance.
	/// @returns Primitive type ID.
	static GA_PrimitiveTypeId install(GA_PrimitiveFactory &primFactory,
									  VRayBaseRefFactory &instance)
	{
		vassert(!instance.isRegistered() && "Multiple attempts to register packed primitive!");

		instance.registerIntrinsics();

		GU_PrimPacked::registerPacked(&primFactory, &instance);
		vassert(instance.isRegistered() && "Unable to register packed primitive!");

		return instance.typeDef().getId();
	}

	explicit VRayBaseRefFactory(const char *primTypeName)
		: GU_PackedFactory(primTypeName, primTypeName)
	{}

	GU_PackedImpl *create() const VRAY_OVERRIDE {
		return new T;
	}

	void registerIntrinsics() {
		T::template registerIntrinsics<T>(*this);
	}
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VFH_GU_BASE_H
