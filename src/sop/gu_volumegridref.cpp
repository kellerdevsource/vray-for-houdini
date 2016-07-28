//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "gu_volumegridref.h"
#include "vfh_log.h"

#include <GT/GT_GEOPrimCollect.h>
#include <GT/GT_GEOAttributeFilter.h>
#include <GT/GT_GEODetail.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_PrimInstance.h>

#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PackedContext.h>
#include <UT/UT_MemoryCounter.h>
#include <FS/UT_DSO.h>

#include <OpenEXR/ImathLimits.h>
#include <OpenEXR/ImathMath.h>


using namespace VRayForHoudini;

GA_PrimitiveTypeId VRayVolumeGridRef::theTypeId(-1);

VFH_DEFINE_FACTORY_BASE(VRayVolumeGridFactoryBase, VRayVolumeGridRef, VFH_VOLUME_GRID_PARAMS, VFH_VOLUME_GRID_PARAMS_COUNT)

class VRayVolumeGridFactory:
		public VRayVolumeGridFactoryBase
{
public:
	static VRayVolumeGridFactory &getInstance()
	{
		static VRayVolumeGridFactory theFactory;
		return  theFactory;
	}

	virtual GU_PackedImpl* create() const VRAY_OVERRIDE
	{
		return new VRayVolumeGridRef();
	}

private:
	VRayVolumeGridFactory();
	virtual ~VRayVolumeGridFactory()
	{ }

	VRayVolumeGridFactory(const VRayVolumeGridFactory &other);
	VRayVolumeGridFactory& operator =(const VRayVolumeGridFactory &other);

};

VRayVolumeGridFactory::VRayVolumeGridFactory():
	VRayVolumeGridFactoryBase("VRayVolumeGridRef", "VRayVolumeGridRef")
{
	registerTupleIntrinsic(
			"phx_channel_map",
			IntGetterCast(&VRayVolumeGridRef::getPhxChannelMapSize),
			StringArrayGetterCast(&VRayVolumeGridRef::getPhxChannelMap),
			StringArraySetterCast(&VRayVolumeGridRef::setPhxChannelMap)
			);
}

void VRayVolumeGridRef::install(GA_PrimitiveFactory *gafactory)
{
	VRayVolumeGridFactory &theFactory = VRayVolumeGridFactory::getInstance();
	if (theFactory.isRegistered()) {
		Log::getLog().error("Multiple attempts to install packed primitive %s from %s",
					theFactory.name(), UT_DSO::getRunningFile());
		return;
	}

	GU_PrimPacked::registerPacked(gafactory, &theFactory);
	if (NOT(theFactory.isRegistered())) {
		Log::getLog().error("Unable to register packed primitive %s from %s",
					theFactory.name(), UT_DSO::getRunningFile());
		return;
	}

	theTypeId = theFactory.typeDef().getId();
}


VRayVolumeGridRef::VRayVolumeGridRef():
	VRayPackedImplBase(),
	m_detail(),
	m_dirty(false)
{ }


VRayVolumeGridRef::VRayVolumeGridRef(const VRayVolumeGridRef &src):
	VRayPackedImplBase(src),
	m_detail(),
	m_dirty(false)
{
	updateFrom(src.m_options);
}


VRayVolumeGridRef::~VRayVolumeGridRef()
{
	clearDetail();
}


GU_PackedFactory* VRayVolumeGridRef::getFactory() const
{
	return &VRayVolumeGridFactory::getInstance();
}


void VRayVolumeGridRef::clearData()
{
	// This method is called when primitives are "stashed" during the cooking
	// process.  However, primitives are typically immediately "unstashed" or
	// they are deleted if the primitives aren't recreated after the fact.
	// We can just leave our data.
}


bool VRayVolumeGridRef::save(UT_Options &options, const GA_SaveMap &map) const
{
	options.merge(m_options);
	return true;
}


bool VRayVolumeGridRef::getLocalTransform(UT_Matrix4D &m) const
{
	if (m_options.getOptionI("flip_yz")) {
		m.identity();
		VUtils::swap(m(1,0), m(2,0));
		VUtils::swap(m(1,1), m(2,1));
		VUtils::swap(m(1,2), m(2,2));
		m(2,0) = -m(2,0);
		m(2,1) = -m(2,1);
		m(2,2) = -m(2,2);
		return true;
	} else {
		return false;
	}
}


bool VRayVolumeGridRef::getBounds(UT_BoundingBox &box) const
{
	// TODO
	// If computing the bounding box is expensive, you may want to cache the
	// box by calling setBoxCache(box)
	// SYSconst_cast(this)->setBoxCache(box);
	return false;
}


bool VRayVolumeGridRef::getRenderingBounds(UT_BoundingBox &box) const
{
	// When geometry contains points or curves, the width attributes need to be
	// taken into account when computing the rendering bounds.
	return getBounds(box);
}


void VRayVolumeGridRef::getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const
{
	// No velocity attribute on geometry
	min = 0;
	max = 0;
}


void VRayVolumeGridRef::getWidthRange(fpreal &min, fpreal &max) const
{
	// Width is only important for curves/points.
	min = max = 0;
}


bool VRayVolumeGridRef::unpack(GU_Detail &destgdp) const
{
	// This may allocate geometry for the primitive
	GU_DetailHandleAutoReadLock gdl(getPackedDetail());
	if (NOT(gdl.isValid())) {
		return false;
	}

	return unpackToDetail(destgdp, gdl.getGdp());
}


GU_ConstDetailHandle VRayVolumeGridRef::getPackedDetail(GU_PackedContext *context) const
{
	if (m_dirty) {
		// Create geometry on demand. If the user only requests the
		// bounding box (i.e. the viewport LOD is set to "box"), then we never
		// have to actually create the proxy's geometry.
		// TODO
	}

	return getDetail();
}


int64 VRayVolumeGridRef::getMemoryUsage(bool inclusive) const
{
	int64 mem = inclusive ? sizeof(*this) : 0;
	// Don't count the (shared) GU_Detail, since that will greatly
	// over-estimate the overall memory usage.
	mem += getDetail().getMemoryUsage(false);
	return mem;
}


void VRayVolumeGridRef::countMemory(UT_MemoryCounter &counter, bool inclusive) const
{
	if (counter.mustCountUnshared()) {
		size_t mem = (inclusive)? sizeof(*this) : 0;
		mem += getDetail().getMemoryUsage(false);
		UT_MEMORY_DEBUG_LOG(theFactory->name(), int64(mem));
		counter.countUnshared(mem);
	}

	// The UT_MemoryCounter interface needs to be enhanced to efficiently count
	// shared memory for details. Skip this for now.
#if 0
	if (detail().isValid())
	{
		GU_DetailHandleAutoReadLock gdh(detail());
		gdh.getGdp()->countMemory(counter, true);
	}
#endif
}


template <typename T>
bool VRayVolumeGridRef::updateFrom(const T &options)
{
	m_options.merge(options);
	topologyDirty();
	attributeDirty();
	return true;
}