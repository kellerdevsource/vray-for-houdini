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

#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PackedContext.h>
#include <UT/UT_MemoryCounter.h>
#include <FS/UT_DSO.h>

#include <OpenEXR/ImathLimits.h>
#include <OpenEXR/ImathMath.h>


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
	registerIntrinsic( "geometryid",
			IntGetterCast(&VRayProxyRef::getGeometryid) );

	registerIntrinsic( VRayProxyParms::thePathToken,
			StringGetterCast(&VRayProxyRef::getPath) );

	registerIntrinsic( VRayProxyParms::theFileToken,
			StringGetterCast(&VRayProxyRef::getFilepath) );

	registerIntrinsic( VRayProxyParms::theLODToken,
			StringGetterCast(&VRayProxyRef::getLOD) );

	registerIntrinsic( VRayProxyParms::theFrameToken,
			FloatGetterCast(&VRayProxyRef::getFloatFrame) );

	registerIntrinsic( VRayProxyParms::theAnimTypeToken,
			IntGetterCast(&VRayProxyRef::getAnimType) );

	registerIntrinsic( "animtypename",
			StringGetterCast(&VRayProxyRef::getAnimTypeName) );

	registerIntrinsic( VRayProxyParms::theAnimOffsetToken,
			FloatGetterCast(&VRayProxyRef::getAnimOffset) );

	registerIntrinsic( VRayProxyParms::theAnimSpeedToken,
			FloatGetterCast(&VRayProxyRef::getAnimSpeed) );

	registerIntrinsic( VRayProxyParms::theAnimOverrideToken,
			BoolGetterCast(&VRayProxyRef::getAnimOverride) );

	registerIntrinsic( VRayProxyParms::theAnimStartToken,
			IntGetterCast(&VRayProxyRef::getAnimStart) );

	registerIntrinsic( VRayProxyParms::theAnimLengthToken,
			IntGetterCast(&VRayProxyRef::getAnimLength) );

	registerIntrinsic( VRayProxyParms::theScaleToken,
			FloatGetterCast(&VRayProxyRef::getScale) );

	registerIntrinsic( VRayProxyParms::theFlipAxisToken,
			IntGetterCast(&VRayProxyRef::getFlipAxis) );
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
{ }


VRayProxyRef::VRayProxyRef(const VRayProxyRef &src):
	GU_PackedImpl(src),
	m_detail(src.m_detail),
	m_options(src.m_options)
{ }


VRayProxyRef::~VRayProxyRef()
{
	clearDetail();
}


GU_PackedFactory* VRayProxyRef::getFactory() const
{
	return theFactory;
}


void VRayProxyRef::clearData()
{
	// This method is called when primitives are "stashed" during the cooking
	// process.  However, primitives are typically immediately "unstashed" or
	// they are deleted if the primitives aren't recreated after the fact.
	// We can just leave our data.
}


bool VRayProxyRef::save(UT_Options &options, const GA_SaveMap &map) const
{
	options.setOptionS(VRayProxyParms::theFileToken, m_options.getFilepath())
			.setOptionI(VRayProxyParms::theLODToken, m_options.getLOD());

	return true;
}


bool VRayProxyRef::getLocalTransform(UT_Matrix4D &m) const
{
	fpreal64 scale = m_options.getScale();
	if (   Imath::equalWithAbsError((float)scale, 1.f, Imath::limits<float>::epsilon())
		&& NOT(m_options.getFlipAxis()) )
	{
		return false;
	}

	m.identity();
	m.scale(scale, scale, scale);

	if (m_options.getFlipAxis()) {
		VUtils::swap(m(1,0), m(2,0));
		VUtils::swap(m(1,1), m(2,1));
		VUtils::swap(m(1,2), m(2,2));
		m(2,0) = -m(2,0);
		m(2,1) = -m(2,1);
		m(2,2) = -m(2,2);
	}

	return true;
}


bool VRayProxyRef::getBounds(UT_BoundingBox &box) const
{
	// If computing the bounding box is expensive, you may want to cache the
	// box by calling setBoxCache(box)
	// SYSconst_cast(this)->setBoxCache(box);
	return GetVRayProxyBounds(m_options, box);
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


template <typename T>
bool VRayProxyRef::updateFrom(const T &options)
{
	bool res = false;
	if (m_options == options) {
		return res;
	}

	m_options = options;
	GU_ConstDetailHandle dtl = GetVRayProxyDetail(m_options);
	if (dtl != getDetail()) {
		setDetail(dtl);
		getPrim()->getParent()->getPrimitiveList().bumpDataId();
		res = true;

		// Notify base primitive that topology has changed
		topologyDirty();
	}

	return res;
}


const char * VRayProxyRef::getPath() const
{
	return m_options.getPath();
}


const char * VRayProxyRef::getFilepath() const
{
	return m_options.getFilepath();
}


const char * VRayProxyRef::getLOD() const
{
	switch (m_options.getLOD()) {
		case LOD_PREVIEW:
		{
			return "preview";
		}
		case LOD_FULL:
		{
			return "full";
		}
	}

	return "";
}


fpreal64 VRayProxyRef::getFloatFrame() const
{
	return m_options.getFloatFrame();
}


exint VRayProxyRef::getAnimType() const
{
	return m_options.getAnimType();
}


const char * VRayProxyRef::getAnimTypeName() const
{
	switch (m_options.getAnimType()) {
		case VUtils::MeshFileAnimType::Loop:
		{
			return "loop";
		}
		case VUtils::MeshFileAnimType::Once:
		{
			return "once";
		}
		case VUtils::MeshFileAnimType::PingPong:
		{
			return "pingpong";
		}
		case VUtils::MeshFileAnimType::Still:
		{
			return "still";
		}
	}

	return "";
}


fpreal64 VRayProxyRef::getAnimOffset() const
{
	return m_options.getAnimOffset();
}


fpreal64 VRayProxyRef::getAnimSpeed() const
{
	return m_options.getAnimSpeed();
}


bool VRayProxyRef::getAnimOverride() const
{
	return m_options.getAnimOverride();
}


exint VRayProxyRef::getAnimStart() const
{
	return m_options.getAnimStart();
}


exint VRayProxyRef::getAnimLength() const
{
	return m_options.getAnimLength();
}


exint VRayProxyRef::getGeometryid() const
{
	GU_DetailHandleAutoReadLock gdl(m_detail);
	return (gdl.isValid())? gdl.getGdp()->getUniqueId() : -1;
}


/// DSO registration callback
void newGeometryPrim(GA_PrimitiveFactory *gafactory)
{
	VRayProxyRef::install(gafactory);
}
