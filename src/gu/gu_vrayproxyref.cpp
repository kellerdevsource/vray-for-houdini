//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "gu_vrayproxyref.h"
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

#include <unordered_set>


using namespace VRayForHoudini;


static const UT_StringRef theGeometryidToken   = "geometryid";
static const UT_StringRef theAnimTypeNameToken = "animtypename";
static const UT_StringRef theLODNameToken      = "lodname";


GA_PrimitiveTypeId VRayProxyRef::theTypeId(-1);


class VRayProxyFactory
	: public GU_PackedFactory
{
public:
	static VRayProxyFactory &getInstance() {
		static VRayProxyFactory theFactory;
		return theFactory;
	}

	GU_PackedImpl* create() const VRAY_OVERRIDE {
		return new VRayProxyRef();
	}

private:
	VRayProxyFactory();

	VUTILS_DISABLE_COPY(VRayProxyFactory);
};


VRayProxyFactory::VRayProxyFactory():
	GU_PackedFactory("VRayProxyRef", "VRayProxyRef")
{
	registerIntrinsic(
				theGeometryidToken,
				IntGetterCast(&VRayProxyRef::getGeometryid)
				);

	registerIntrinsic(
				VRayProxyParms::theFileToken,
				StringGetterCast(&VRayProxyRef::getFilepath),
				StringSetterCast(&VRayProxyRef::setFilepath)
				);

	registerIntrinsic(
				VRayProxyParms::theLODToken,
				IntGetterCast(&VRayProxyRef::getLOD),
				IntSetterCast(&VRayProxyRef::setLOD)
				);

	registerIntrinsic(
				theLODNameToken,
				StringGetterCast(&VRayProxyRef::getLODName)
				);

	registerIntrinsic(
				VRayProxyParms::theAnimTypeToken,
				IntGetterCast(&VRayProxyRef::getAnimType),
				IntSetterCast(&VRayProxyRef::setAnimType)
				);

	registerIntrinsic(
				theAnimTypeNameToken,
				StringGetterCast(&VRayProxyRef::getAnimTypeName)
				);

	registerIntrinsic(
				VRayProxyParms::theAnimOffsetToken,
				FloatGetterCast(&VRayProxyRef::getAnimOffset),
				FloatSetterCast(&VRayProxyRef::setAnimOffset)
				);

	registerIntrinsic(
				VRayProxyParms::theAnimSpeedToken,
				FloatGetterCast(&VRayProxyRef::getAnimSpeed),
				FloatSetterCast(&VRayProxyRef::setAnimSpeed)
				);

	registerIntrinsic(
				VRayProxyParms::theAnimOverrideToken,
				BoolGetterCast(&VRayProxyRef::getAnimOverride),
				BoolSetterCast(&VRayProxyRef::setAnimOverride)
				);

	registerIntrinsic(
				VRayProxyParms::theAnimStartToken,
				IntGetterCast(&VRayProxyRef::getAnimStart),
				IntSetterCast(&VRayProxyRef::setAnimStart)
				);

	registerIntrinsic(
				VRayProxyParms::theAnimLengthToken,
				IntGetterCast(&VRayProxyRef::getAnimLength),
				IntSetterCast(&VRayProxyRef::setAnimLength)
				);

	registerIntrinsic(
				VRayProxyParms::theScaleToken,
				FloatGetterCast(&VRayProxyRef::getScale),
				FloatSetterCast(&VRayProxyRef::setScale)
				);

	registerIntrinsic(
				VRayProxyParms::theFlipAxisToken,
				IntGetterCast(&VRayProxyRef::getFlipAxis),
				IntSetterCast(&VRayProxyRef::setFlipAxis)
				);
}


/// Hook to handle tesselation of proxy primitives
///
/// When rendering proxy primitives, we collect all
/// primitives using same detail together based on detail id
/// For each different detail id a separate GT primitive is generated
class GT_PrimVRayProxyCollect : public GT_GEOPrimCollect
{
public:
	/// Register the GT collector
	static void registerPrimitive(const GA_PrimitiveTypeId &id);

public:
	/// Constructor.  The @c id is used to bind the collector to the proper
	/// primitive type.
	GT_PrimVRayProxyCollect(const GA_PrimitiveTypeId &id);
	/// Destructor
	virtual ~GT_PrimVRayProxyCollect();

	/// @{
	/// Virtual interface from GU_PackedImpl interface
	///
	/// Return a structure to capture similar proxy primitives
	virtual GT_GEOPrimCollectData * beginCollecting(const GT_GEODetailListHandle &geometry,
													const GT_RefineParms *parms) const;

	/// When refining a single proxy primitive, we check to see
	/// if a GT primitive for it has already beed generated
	virtual GT_PrimitiveHandle collect(const GT_GEODetailListHandle &geometry,
									   const GEO_Primitive *const* prim_list,
									   int nsegments,
									   GT_GEOPrimCollectData *data) const;

	/// At the end of collecting, do nothing
	virtual GT_PrimitiveHandle endCollecting(const GT_GEODetailListHandle &geometry,
											 GT_GEOPrimCollectData *data) const;
	/// @}

	inline GT_PrimitiveHandle getGTPrimPoints(const GU_PrimPacked &prim) const;

private:
	GA_PrimitiveTypeId  m_primTypeId;
};


void VRayProxyRef::install(GA_PrimitiveFactory *gafactory)
{
	VRayProxyFactory &theFactory = VRayProxyFactory::getInstance();
	if (theFactory.isRegistered()) {
		Log::getLog().debug("Multiple attempts to install packed primitive %s from %s",
							static_cast<const char *>(theFactory.name()), UT_DSO::getRunningFile());
		return;
	}

	GU_PrimPacked::registerPacked(gafactory, &theFactory);
	if (NOT(theFactory.isRegistered())) {
		Log::getLog().error("Unable to register packed primitive %s from %s",
							static_cast<const char *>(theFactory.name()), UT_DSO::getRunningFile());
		return;
	}

	theTypeId = theFactory.typeDef().getId();

	// Register the GT tesselation too (now we know what type id we have)
	GT_PrimVRayProxyCollect::registerPrimitive(theTypeId);
}


VRayProxyRef::VRayProxyRef():
	GU_PackedImpl(),
	m_detail(),
	m_dirty(false)
{ }


VRayProxyRef::VRayProxyRef(const VRayProxyRef &src):
	GU_PackedImpl(src),
	m_detail(),
	m_dirty(false)
{
	updateFrom(src.m_options);
}


VRayProxyRef::~VRayProxyRef()
{
	clearDetail();
}


GU_PackedFactory* VRayProxyRef::getFactory() const
{
	return &VRayProxyFactory::getInstance();
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
	options.merge(m_options.getOptions());
	return true;
}


bool VRayProxyRef::getLocalTransform(UT_Matrix4D &m) const
{
	const fpreal64 scale = m_options.getScale();

	if (IsFloatEq(static_cast<float>(scale), 1.0f) &&
		!m_options.getFlipAxis())
	{
		return false;
	}

	m.identity();
	m.scale(scale, scale, scale);

	if (m_options.getFlipAxis()) {
		UT_Matrix4D flipTm(0.0);
		flipTm(0,0) =  1.0;
		flipTm(1,2) = -1.0;
		flipTm(2,1) =  1.0;
		flipTm(3,3) =  1.0;

		m = flipTm * m;
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
	if (m_dirty) {
		// Create geometry on demand. If the user only requests the
		// bounding box (i.e. the viewport LOD is set to "box"), then we never
		// have to actually create the proxy's geometry.
		VRayProxyRef *me = const_cast< VRayProxyRef * >(this);
		GU_ConstDetailHandle dtl = GetVRayProxyDetail(m_options);
		if (dtl != getDetail()) {
			me->setDetail(dtl);
			getPrim()->getParent()->getPrimitiveList().bumpDataId();
		}

		me->m_dirty = false;
	}

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
	if (m_options == options) {
		return false;
	}

	m_options = options;
	m_dirty = true;
	// Notify base primitive that topology has changed
	topologyDirty();

	return true;
}


void VRayProxyRef::setFilepath(const char *filepath)
{
	m_options.getOptions().setOptionS(VRayProxyParms::theFileToken, filepath);
	m_dirty = true;
	topologyDirty();
}


void VRayProxyRef::setLOD(exint lod)
{
	m_options.getOptions().setOptionI(VRayProxyParms::theLODToken, lod);
	m_dirty = true;
	topologyDirty();
}


void VRayProxyRef::setAnimType(exint animType)
{
	m_options.getOptions().setOptionI(VRayProxyParms::theAnimTypeToken, animType);
	m_dirty = true;
	topologyDirty();
}


void VRayProxyRef::setAnimOffset(fpreal64 offset)
{
	m_options.getOptions().setOptionF(VRayProxyParms::theAnimOffsetToken, offset);
	m_dirty = true;
	topologyDirty();
}


void VRayProxyRef::setAnimSpeed(fpreal64 speed)
{
	m_options.getOptions().setOptionF(VRayProxyParms::theAnimSpeedToken, speed);
	m_dirty = true;
	topologyDirty();
}


void VRayProxyRef::setAnimOverride(bool override)
{
	m_options.getOptions().setOptionB(VRayProxyParms::theAnimOverrideToken, override);
	m_dirty = true;
	topologyDirty();
}


void VRayProxyRef::setAnimStart(exint start)
{
	m_options.getOptions().setOptionI(VRayProxyParms::theAnimStartToken, start);
	m_dirty = true;
	topologyDirty();
}


void VRayProxyRef::setAnimLength(exint length)
{
	m_options.getOptions().setOptionI(VRayProxyParms::theAnimLengthToken, length);
	m_dirty = true;
	topologyDirty();
}


void VRayProxyRef::setScale(fpreal64 scale)
{
	m_options.getOptions().setOptionF(VRayProxyParms::theScaleToken, scale);
	transformDirty();
}


void VRayProxyRef::setFlipAxis(exint flip)
{
	m_options.getOptions().setOptionI(VRayProxyParms::theFlipAxisToken, flip);
	transformDirty();
}


const char * VRayProxyRef::getLODName() const
{
	switch (m_options.getLOD()) {
		case LOD_BBOX:
		{
			return "box";
		}
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


exint VRayProxyRef::getGeometryid() const
{
	GU_DetailHandleAutoReadLock gdl(getPackedDetail());
	return (gdl.isValid())? gdl.getGdp()->getUniqueId() : -1;
}


//////////
/// \brief The GT_GEOPrimCollectGeoIDData class
///
class GT_GEOPrimCollectGeoIDData : public GT_GEOPrimCollectData
{
public:
	GT_GEOPrimCollectGeoIDData()
	{ }
	virtual ~GT_GEOPrimCollectGeoIDData()
	{ }

	bool hasPrim(const GA_Primitive *prim) const
	{
		int geoid = -1;
		prim->getIntrinsic(prim->findIntrinsic(theGeometryidToken), geoid);
		return m_geoidset.count(geoid);
	}

	int insert(const GA_Primitive *prim)
	{
		int geoid = -1;
		prim->getIntrinsic(prim->findIntrinsic(theGeometryidToken), geoid);
		if (geoid == -1) {
			return geoid;
		}

		bool isInserted = m_geoidset.insert(geoid).second;
		return ((isInserted)? geoid : -1);
	}

private:
	std::unordered_set< int > m_geoidset;
};


void GT_PrimVRayProxyCollect::registerPrimitive(const GA_PrimitiveTypeId &id)
{
	// Just construct.  The constructor registers itself.
	new GT_PrimVRayProxyCollect(id);
}


GT_PrimVRayProxyCollect::GT_PrimVRayProxyCollect(const GA_PrimitiveTypeId &id)
	: m_primTypeId(id)
{
	// Bind this collector to the given primitive id.  When GT refines
	// primitives and hits the given primitive id, this collector will be
	// invoked.
	bind(m_primTypeId);
}


GT_PrimVRayProxyCollect::~GT_PrimVRayProxyCollect()
{ }



GT_GEOPrimCollectData * GT_PrimVRayProxyCollect::beginCollecting(const GT_GEODetailListHandle &geometry, const GT_RefineParms *parms) const
{
	// Collect the different detail ids
	return new GT_GEOPrimCollectGeoIDData();
}


GT_PrimitiveHandle GT_PrimVRayProxyCollect::collect(const GT_GEODetailListHandle &geometry,
												const GEO_Primitive *const* prim_list,
												int nsegments,
												GT_GEOPrimCollectData *data) const
{
	auto primpacked = UTverify_cast< const GU_PrimPacked * >(prim_list[0]);
	if (primpacked->viewportLOD() == GEO_VIEWPORT_HIDDEN) {
		return GT_PrimitiveHandle();
	}

	GT_GEOPrimCollectGeoIDData *collector = data->asPointer< GT_GEOPrimCollectGeoIDData >();
	int geoid = collector->insert(prim_list[0]);
	if (geoid == -1) {
		return GT_PrimitiveHandle();
	}

	GU_ConstDetailHandle gdh = geometry->getGeometry(0);
	GU_DetailHandleAutoReadLock  rlock(gdh);
	const GU_Detail &gdp = *rlock;

	GT_GEOOffsetList offsets;
	offsets.reserve(gdp.getNumPrimitives());
	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);

		int primgeoid = -1;
		prim->getIntrinsic(prim->findIntrinsic(theGeometryidToken), primgeoid);
		if (primgeoid != geoid) {
			continue;
		}

		offsets.append(prim->getMapOffset());
	}

	if (NOT(offsets.entries())) {
		return GT_PrimitiveHandle();
	}

	bool transformed = (offsets.entries() == 1);
	GT_PrimitiveHandle gtprim = new GT_GEOPrimPacked(gdh, primpacked, transformed);

	if (offsets.entries() == 1) {
		return gtprim;
	}

	GT_AttributeListHandle uniform;
	GT_AttributeListHandle detail;
	// The attribute filter isn't used in this primitive, but it allows
	// attributes to be excluded (i.e. spheres, might not want the "N" or the
	// "P" attributes).
	GT_GEOAttributeFilter  filter;
	// Create primitive attributes.
	//
	// Each array item is filled with the attribute data for the corresponding
	// face in the ga_faces array.
	uniform = geometry->getPrimitiveAttributes(filter, &offsets);
	// Create detail attributes.  These are common for all faces
	detail = geometry->getDetailAttributes(filter);

	GT_TransformArrayHandle transforms = new GT_TransformArray();
	transforms->setEntries(offsets.entries());
	for (exint i = 0; i < offsets.entries(); ++i) {
		auto primpacked = UTverify_cast< const GU_PrimPacked * >( gdp.getGEOPrimitive(offsets(i)) );

		UT_Matrix4D m(1.);
		primpacked->getFullTransform4(m);

		GT_TransformHandle xform = new GT_Transform();
		xform->alloc(1);
		xform->setMatrix(m, 0);
		transforms->set(i, xform);
	}

	GT_PrimitiveHandle gtinstance = new GT_PrimInstance(gtprim,
														transforms,
														offsets,
														uniform,
														detail,
														geometry);

	return gtinstance;
}


GT_PrimitiveHandle GT_PrimVRayProxyCollect::endCollecting(const GT_GEODetailListHandle &geometry,
													  GT_GEOPrimCollectData *data) const
{
	return GT_PrimitiveHandle();
}


GT_PrimitiveHandle GT_PrimVRayProxyCollect::getGTPrimPoints(const GU_PrimPacked &prim) const
{
	return GT_GEODetail::makePointMesh(prim.getPackedDetail(), nullptr);
}
