//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_gu_base.h"
#include "vfh_log.h"

#include <UT/UT_MemoryCounter.h>
#include <GT/GT_PrimCollect.h>

using namespace VRayForHoudini;

VRayBaseRef::VRayBaseRef()
{
	detailClear();
}

VRayBaseRef::VRayBaseRef(const VRayBaseRef &other)
	: GU_PackedImpl(other)
	, m_options(other.m_options)
	, m_detail(other.m_detail)
{}

bool VRayBaseRef::isValid() const
{
	return m_detail.isValid();
}

void VRayBaseRef::clearData()
{
	// This method is called when primitives are "stashed" during the cooking
	// process.  However, primitives are typically immediately "unstashed" or
	// they are deleted if the primitives aren't recreated after the fact.
	// We can just leave our data.
}

bool VRayBaseRef::save(UT_Options &options, const GA_SaveMap&) const
{
	options.merge(m_options);
	return true;
}

bool VRayBaseRef::getBounds(UT_BoundingBox &box) const
{
	box = m_bbox;
	return true;
}

bool VRayBaseRef::getRenderingBounds(UT_BoundingBox &box) const
{
	return getBounds(box);
}

void VRayBaseRef::getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const
{
	// No velocity attribute on geometry
	min = 0;
	max = 0;
}

void VRayBaseRef::getWidthRange(fpreal &wmin, fpreal &wmax) const
{
	// Width is only important for curves/points.
	wmin = 0;
	wmax = 0;
}

bool VRayBaseRef::unpack(GU_Detail &destgdp) const
{
	GU_DetailHandleAutoReadLock gdl(getPackedDetail());
	if (!gdl.isValid())
		return false;
	return unpackToDetail(destgdp, gdl.getGdp());
}

GU_ConstDetailHandle VRayBaseRef::getPackedDetail(GU_PackedContext*) const
{
	return m_detail;
}

int64 VRayBaseRef::getMemoryUsage(bool) const
{
	return m_detail.getMemoryUsage(false);
}

void VRayBaseRef::countMemory(UT_MemoryCounter &counter, bool) const
{
	if (counter.mustCountUnshared()) {
		const int64 mem = m_detail.getMemoryUsage(false);
		counter.countUnshared(mem);
	}
}

void VRayBaseRef::detailClear()
{
	m_bbox.initBounds();
	m_detail.clear();
}

#ifdef HDK_16_5
int VRayBaseRef::updateFrom(GU_PrimPacked *prim, const UT_Options &options)
#else
int VRayBaseRef::updateFrom(const UT_Options &options)
#endif
{
	if (m_options == options)
		return false;

	m_options.merge(options);

	detailClear();

	if (detailRebuild()) {
#ifdef HDK_16_5
		prim->topologyDirty();
#else
		topologyDirty();
#endif
	}

	return true;
}

GA_PrimitiveTypeId VRayBaseRefCollectData::getMyTypeID() const
{
	return myPrimTypeId;
}


void VRayBaseRefCollectData::addPrim(uint key, const GU_PrimPacked *value)
{
	detailInstances[key].append(value->getMapOffset());
}

const DetailToPrimitive& VRayBaseRefCollectData::getPrimitives() const
{
	return detailInstances;
}

VRayBaseRefCollect::VRayBaseRefCollect(const GA_PrimitiveTypeId &typeId)
	: typeId(typeId)
{
	bind(typeId);
}

void VRayBaseRefCollect::install(const GA_PrimitiveTypeId &typeId)
{
	new VRayBaseRefCollect(typeId);
}

GT_GEOPrimCollectData *VRayBaseRefCollect::beginCollecting(const GT_GEODetailListHandle & /*geometry*/,
                                                           const GT_RefineParms * /*parms*/) const
{
	return new VRayBaseRefCollectData(typeId);
}

GT_PrimitiveHandle VRayBaseRefCollect::collect(const GT_GEODetailListHandle & /*geometry*/,
                                               const GEO_Primitive * const*prim_list,
                                               int /*nsegments*/,
                                               GT_GEOPrimCollectData *data) const
{
	VRayBaseRefCollectData &collectData =
		*data->asPointer<VRayBaseRefCollectData>();

	const GU_PrimPacked *curPrim = UTverify_cast<const GU_PrimPacked*>(prim_list[0]);
	if (curPrim->getTypeId() != collectData.getMyTypeID())
		return GT_PrimitiveHandle();

	if (curPrim->viewportLOD() == GEO_VIEWPORT_HIDDEN)
		return GT_PrimitiveHandle();

	GU_ConstDetailHandle packedHandle = curPrim->implementation()->getPackedDetail();
	if (!packedHandle.isValid())
		return GT_PrimitiveHandle();

	collectData.addPrim(packedHandle.hash(), curPrim);

	return GT_PrimitiveHandle();
}

GT_PrimitiveHandle VRayBaseRefCollect::endCollecting(const GT_GEODetailListHandle &geometry,
                                                     GT_GEOPrimCollectData *data) const
{
	const GU_ConstDetailHandle gdh(geometry->getGeometry(0));

	const GU_DetailHandleAutoReadLock rlock(gdh);
	const GU_Detail &gdp = *rlock;

	const VRayBaseRefCollectData &collectData =
		*data->asPointer<VRayBaseRefCollectData>();

	GT_PrimCollect *primCollect = new GT_PrimCollect;

	for (const GT_GEOOffsetList &offsets : collectData.getPrimitives()) {
		const GT_GEOAttributeFilter filter;

		const GT_AttributeListHandle uniform =
			geometry->getPrimitiveAttributes(filter, &offsets);
		const GT_AttributeListHandle detail =
			geometry->getDetailAttributes(filter);

		GT_TransformArrayHandle transforms(new GT_TransformArray);
		transforms->setEntries(offsets.entries());

		const GU_PrimPacked *prim = nullptr;

		for (int i = 0; i < offsets.entries(); ++i) {
			const GU_PrimPacked *currentPrim =
				UTverify_cast<const GU_PrimPacked*>(gdp.getGEOPrimitive(offsets(i)));
			if (!prim) {
				prim = currentPrim;
			}

			UT_Matrix4D m;
			currentPrim->getFullTransform4(m);

			GT_TransformHandle xform(new GT_Transform);
			xform->alloc(1);
			xform->setMatrix(m, 0);

			transforms->set(i, xform);
		}

		primCollect->appendPrimitive(GT_PrimitiveHandle(new GT_PrimInstance(new GT_GEOPrimPacked(gdh, prim),
		                                                                    transforms,
		                                                                    offsets,
		                                                                    uniform,
		                                                                    detail,
		                                                                    geometry)));
	}

	return GT_PrimitiveHandle(primCollect);
}
