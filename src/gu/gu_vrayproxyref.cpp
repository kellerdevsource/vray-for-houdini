//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_vray.h"
#include "vfh_log.h"
#include "gu_vrayproxyref.h"

#include "DetailCachePrototype.h"
#include "vfh_hashes.h"

#include <GT/GT_GEOAttributeFilter.h>
#include <GT/GT_GEOPrimCollect.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_PrimInstance.h>
#include <GT/GT_GEODetail.h>

using namespace VRayForHoudini;

static GA_PrimitiveTypeId theTypeId(-1);
static VRayBaseRefFactory<VRayProxyRef> theFactory("VRayProxyRef");
static const UT_StringRef theGeometryidToken = "geometryid";

struct VRayProxyRefKeyHasher {
	uint32 operator()(const VRayProxyRefKey &key) const {
#pragma pack(push, 1)
		struct SettingsKey {
			int lod;
			int frame;
			int animType;
			int animOffset;
			int animSpeed;
			int animOverride;
			int animLength;
			int numPreviewFaces;
		} settingsKey = { key.lod
			, key.f
			, key.animType
			, key.animOffset
			, key.animSpeed
			, key.animOverride
			, key.animLength
			, key.previewFaces
		};
#pragma pack(pop)
		Hash::MHash data;
		Hash::MurmurHash3_x86_32(&settingsKey, sizeof(SettingsKey), 42, &data);
		return data;
	}
};

class VRayProxyRefKeyBuilder : public DetailBuilder<VRayProxyRefKey, bool> {
public:
	GU_DetailHandle buildDetail(const VUtils::CharString &filepath, const VRayProxyRefKey &settings, fpreal t, bool &rval) override {
		return getVRayProxyDetail(settings);
	}

	void cleanResource(const VUtils::CharString &filepath) override {
		clearVRayProxyCache(filepath.ptr());
	}

} builder;

static DetailCachePrototype<bool, VRayProxyRefKey, VRayProxyRefKeyHasher> cache(builder);

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

void VRayProxyRef::install(GA_PrimitiveFactory *primFactory)
{
	theTypeId = theFactory.install(*primFactory, theFactory);

	SYSconst_cast(theFactory.typeDef()).setHasLocalTransform(true);

	// register collector for type
	GT_PrimVRayProxyCollect::registerPrimitive(theTypeId);
}

VRayProxyRef::VRayProxyRef()
{}

VRayProxyRef::VRayProxyRef(const VRayProxyRef &src)
	: VRayProxyRefBase(src)
{
	const VRayProxyRefKey &key = getKey();
	cache.registerInCache(key.filePath, key);
}

VRayProxyRef::~VRayProxyRef()
{
	const VRayProxyRefKey &key = getKey();
	cache.unregister(key.filePath, key);
}

GU_PackedFactory* VRayProxyRef::getFactory() const
{
	return &theFactory;
}

GU_PackedImpl *VRayProxyRef::copy() const
{
	return new VRayProxyRef(*this);
}

bool VRayProxyRef::getLocalTransform(UT_Matrix4D &m) const
{
	m.identity();

	const fpreal64 scale = getScale();
	m.scale(scale, scale, scale);

	if (getFlipAxis()) {
		UT_Matrix4D flipTm(0.0);
		flipTm(0,0) =  1.0;
		flipTm(1,2) =  1.0;
		flipTm(2,1) =  1.0;
		flipTm(3,3) =  1.0;

		m = flipTm * m;
	}

	return true;
}

bool VRayProxyRef::getBounds(UT_BoundingBox &box) const
{
	const VRayProxyRefKey &vrmeshKey = getKey();
	return getVRayProxyBoundingBox(vrmeshKey, box);
}

bool VRayProxyRef::unpack(GU_Detail&) const
{
	// This will show error and indicate that we don't support unpacking.
	return false;
}

VRayProxyRefKey VRayProxyRef::getKey() const
{
	VRayProxyRefKey key;
	key.filePath = getFile();
	key.lod = static_cast<LOD>(getLod());
	key.f = getCurrentFrame();
	key.animType = getAnimType();
	key.animOffset = getAnimOffset();
	key.animSpeed = getAnimSpeed();
	key.animOverride = getAnimOverride();
	key.animStart = getAnimStart();
	key.animLength = getAnimLength();
	key.previewFaces = getNumPreviewFaces();
	return key;
}

int VRayProxyRef::detailRebuild()
{
	const VRayProxyRefKey &vrmeshKey = getKey();
	updateCacheVars(vrmeshKey);

	const GU_DetailHandle &getail = cache.getDetail(vrmeshKey.filePath, vrmeshKey, vrmeshKey.f);

	const int res = m_detail != getail;
	m_detail = getail;

	return res;
}

void VRayProxyRef::updateCacheVars(const VRayProxyRefKey &newKey) {
	if (lastKey.differingSettings(newKey)) {
		cache.registerInCache(newKey.filePath, lastKey);
		cache.unregister(lastKey.filePath, lastKey);
		lastKey = newKey;
	}
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
		return ((isInserted) ? geoid : -1);
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
		auto primpacked = UTverify_cast< const GU_PrimPacked * >(gdp.getGEOPrimitive(offsets(i)));

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
