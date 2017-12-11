//
// Copyright (c) 2015-2017, Chaos Software Ltd
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

using namespace VRayForHoudini;

VRayBaseRef::VRayBaseRef()
{
	detailClear();
}

VRayBaseRef::VRayBaseRef(const VRayBaseRef &other)
	: m_options(other.m_options)
	, m_detail(other.m_detail)
{}

VRayBaseRef::VRayBaseRef(VRayBaseRef &&other) noexcept
	: m_options(std::move(other.m_options))
	, m_detail(other.m_detail)
{}

bool VRayBaseRef::isValid() const
{
	return m_detail.isValid();
}

void VRayBaseRef::clearData()
{
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
	wmin = wmax = 0;
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

int64 VRayBaseRef::getMemoryUsage(bool inclusive) const
{
	int64 mem = inclusive ? sizeof(VRayBaseRef) : 0;
	mem += m_detail.getMemoryUsage(false);

	return mem;
}

void VRayBaseRef::countMemory(UT_MemoryCounter &counter, bool inclusive) const
{
	if (counter.mustCountUnshared()) {
		int64 mem = inclusive ? sizeof(VRayBaseRef) : 0;
		mem += m_detail.getMemoryUsage(false);
		UT_MEMORY_DEBUG_LOG(theFactory->name(), mem);
		counter.countUnshared(mem);
	}
}

void VRayBaseRef::detailClear()
{
	m_bbox.initBounds();
	m_detail.deleteGdp();
}

int VRayBaseRef::updateFrom(const UT_Options &options)
{
	if (m_options == options)
		return false;

	// Store new options
	m_options = options;

	detailClear();
	detailRebuild();

	topologyDirty();

	return true;
}
