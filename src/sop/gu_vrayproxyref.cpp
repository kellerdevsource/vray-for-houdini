//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "gu_vrayproxyref.h"
#include "vfh_log.h"
#include "vfh_vrayproxycache.h"

#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PackedContext.h>
#include <UT/UT_MemoryCounter.h>
#include <FS/UT_DSO.h>


using namespace VRayForHoudini;


namespace
{

class VRayProxyFactory:
		public GU_PackedFactory
{
public:
	VRayProxyFactory();
	virtual ~VRayProxyFactory()
	{ }

	virtual GU_PackedImpl* create() const VRAY_OVERRIDE
	{ return new VRayProxyRef(); }
};


VRayProxyFactory::VRayProxyFactory():
	GU_PackedFactory("VRayProxyRef", "VRayProxyRef")
{
	registerIntrinsic( VRayProxyUtils::theLODToken,
			IntGetterCast(&VRayProxyRef::getLOD) );

	registerIntrinsic( VRayProxyUtils::theFileToken,
			StringHolderGetterCast(&VRayProxyRef::getFilepath) );
}


static VRayProxyFactory *theFactory = nullptr;

}


GA_PrimitiveTypeId VRayProxyRef::theTypeId(-1);


void VRayProxyRef::install(GA_PrimitiveFactory *gafactory)
{
	UT_ASSERT( NOT(theFactory) );
	if (theFactory) {
		return;
	}

	theFactory = new VRayProxyFactory();
	GU_PrimPacked::registerPacked(gafactory, theFactory);
	if (theFactory->isRegistered()) {
		theTypeId = theFactory->typeDef().getId();
	}
	else {
		Log::getLog().error("Unable to register packed primitive %s from %s",
					theFactory->name(), UT_DSO::getRunningFile());
	}
}


VRayProxyRef::VRayProxyRef():
	GU_PackedImpl(),
	m_detail()
{
	GU_Detail *gdp = new GU_Detail();
	m_detail.allocateAndSet(gdp);
}


VRayProxyRef::VRayProxyRef(const VRayProxyRef &src):
	GU_PackedImpl(src),
	m_detail()
{
	GU_Detail *gdp = new GU_Detail();
	m_detail.allocateAndSet(gdp);
	updateFrom(src.m_options);
}


VRayProxyRef::~VRayProxyRef()
{ }


GU_PackedFactory* VRayProxyRef::getFactory() const
{
	return theFactory;
}


GU_PackedImpl* VRayProxyRef::copy() const
{
	return new VRayProxyRef(*this);
}


void VRayProxyRef::clearData()
{
	// This method is called when primitives are "stashed" during the cooking
	// process.  However, primitives are typically immediately "unstashed" or
	// they are deleted if the primitives aren't recreated after the fact.
	// We can just leave our data.
}


bool VRayProxyRef::isValid() const
{
	return m_detail.isValid();
}


bool VRayProxyRef::save(UT_Options &options, const GA_SaveMap &map) const
{
	options.setOptionS(VRayProxyUtils::theFileToken, getFilepath())
			.setOptionI(VRayProxyUtils::theLODToken, getLOD());

	return true;
}


bool VRayProxyRef::getBounds(UT_BoundingBox &box) const
{
	// All spheres are unit spheres with transforms applied
	box.initBounds(-1, -1, -1);
	box.enlargeBounds(1, 1, 1);
	// If computing the bounding box is expensive, you may want to cache the
	// box by calling setBoxCache(box)
	// SYSconst_cast(this)->setBoxCache(box);
	return true;
}


bool VRayProxyRef::getRenderingBounds(UT_BoundingBox &box) const
{
	// When geometry contains points or curves, the width attributes need to be
	// taken into account when computing the rendering bounds.
	return getBounds(box);
}


void VRayProxyRef::getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const
{
	// No velocity attribute on geometry
	min = 0;
	max = 0;
}


void VRayProxyRef::getWidthRange(fpreal &min, fpreal &max) const
{
	// Width is only important for curves/points.
	min = max = 0;
}


bool VRayProxyRef::unpack(GU_Detail &destgdp) const
{
	// This may allocate geometry for the primitive
	GU_DetailHandleAutoReadLock gdl(getPackedDetail());
	if (NOT(gdl.isValid())) {
		return false;
	}

	return unpackToDetail(destgdp, gdl.getGdp());
}


GU_ConstDetailHandle VRayProxyRef::getPackedDetail(GU_PackedContext *context) const
{
	return getDetail();
}


int64 VRayProxyRef::getMemoryUsage(bool inclusive) const
{
	int64 mem = inclusive ? sizeof(*this) : 0;
	// Don't count the (shared) GU_Detail, since that will greatly
	// over-estimate the overall memory usage.
	mem += getDetail().getMemoryUsage(false);
	return mem;
}


void VRayProxyRef::countMemory(UT_MemoryCounter &counter, bool inclusive) const
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


bool VRayProxyRef::updateFrom(const UT_Options &options)
{
	if (m_options == options) {
		return false;
	}

	m_options = options;
	GU_DetailHandleAutoWriteLock gdl(m_detail);
	if (gdl.isValid()) {
		GU_Detail *gdp = gdl.getGdp();
		gdp->clearAndDestroy();
		VRayProxyUtils::getVRayProxyDetail(m_options, *gdl.getGdp());
		getPrim()->getParent()->getPrimitiveList().bumpDataId();
	}

	// Notify base primitive that topology has changed
	topologyDirty();
}


UT_StringHolder VRayProxyRef::getFilepath() const
{
	return VRayProxyUtils::getFilepath(m_options);
}


exint VRayProxyRef::getLOD() const
{
	return VRayProxyUtils::getLOD(m_options);
}


fpreal64 VRayProxyRef::getFloatFrame() const
{
	return VRayProxyUtils::getFloatFrame(m_options);
}


exint VRayProxyRef::getAnimType() const
{
	return VRayProxyUtils::getAnimType(m_options);
}


fpreal64 VRayProxyRef::getAnimOffset() const
{
	return VRayProxyUtils::getAnimOffset(m_options);
}


fpreal64 VRayProxyRef::getAnimSpeed() const
{
	return VRayProxyUtils::getAnimSpeed(m_options);
}


bool VRayProxyRef::getAnimOverride() const
{
	return VRayProxyUtils::getAnimOverride(m_options);
}


exint VRayProxyRef::getAnimStart() const
{
	return VRayProxyUtils::getAnimStart(m_options);
}


exint VRayProxyRef::getAnimLength() const
{
	return VRayProxyUtils::getAnimLength(m_options);
}


/// DSO registration callback
void newGeometryPrim(GA_PrimitiveFactory *gafactory)
{
	VRayProxyRef::install(gafactory);
}
