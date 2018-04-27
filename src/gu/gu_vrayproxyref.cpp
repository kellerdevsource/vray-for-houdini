//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_vray.h"
#include "vfh_gu_cache.h"
#include "vfh_hashes.h"

#include "gu_vrayproxyref.h"

#include <GT/GT_GEODetail.h>

using namespace VRayForHoudini;

typedef VRayBaseRefFactory<VRayProxyRef> VRayProxyRefFactory;

static GA_PrimitiveTypeId theTypeId(-1);
static VRayProxyRefFactory theFactory("VRayProxyRef");

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
		} settingsKey = {
			static_cast<int>(key.lod),
			VUtils::fast_floor(key.f * 100.0),
			key.animType,
			VUtils::fast_floor(key.animOffset * 100.0),
			VUtils::fast_floor(key.animSpeed * 100.0),
			key.animOverride,
			key.animLength,
			key.previewFaces
		};
#pragma pack(pop)
		Hash::MHash data;
		Hash::MurmurHash3_x86_32(&settingsKey, sizeof(SettingsKey), 42, &data);
		return data;
	}
};

class VRayProxyRefKeyBuilder
	: public DetailBuilder<VRayProxyRefKey, bool>
{
public:
	GU_DetailHandle buildDetail(const VUtils::CharString &filepath, const VRayProxyRefKey &settings, fpreal t, bool &rval) override {
		return getVRayProxyDetail(settings);
	}

	void cleanResource(const VUtils::CharString &filepath) override {
		clearVRayProxyCache(filepath.ptr());
	}
};

static VRayProxyRefKeyBuilder builder;
static DetailCachePrototype<bool, VRayProxyRefKey, VRayProxyRefKeyHasher> cache(builder);

void VRayProxyRef::install(GA_PrimitiveFactory *primFactory)
{
	theTypeId = VRayProxyRefFactory::install(*primFactory, theFactory);

	SYSconst_cast(theFactory.typeDef()).setHasLocalTransform(true);

	VRayBaseRefCollect::install(theTypeId);
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
	key.lod = static_cast<VRayProxyPreviewType>(getPreviewType());
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
