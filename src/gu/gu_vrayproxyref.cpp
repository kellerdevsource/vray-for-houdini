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

using namespace VRayForHoudini;

static GA_PrimitiveTypeId theTypeId(-1);
static VRayBaseRefFactory<VRayProxyRef> theFactory("VRayProxyRef");

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
		} settingsKey = { key.lod
			, key.f
			, key.animType
			, key.animOffset
			, key.animSpeed
			, key.animOverride
			, key.animLength
		};
#pragma pack(pop)
		Hash::MHash data;
		Hash::MurmurHash3_x86_32(&settingsKey, sizeof(SettingsKey), 42, &data);
		return data;
	}
};

class VRayProxyRefKeyBuilder : public DetailBuilder<VRayProxyRefKey, bool> {
public:
	GU_DetailHandle buildDetail(const VUtils::CharString &filepath, const VRayProxyRefKey &settings, const fpreal &t, bool &rval) override {
		return getVRayProxyDetail(settings);
	}
} builder;

static DetailCachePrototype<bool, VRayProxyRefKey, VRayProxyRefKeyHasher> cache(builder);

void VRayProxyRef::install(GA_PrimitiveFactory *primFactory)
{
	theTypeId = theFactory.install(*primFactory, theFactory);

	SYSconst_cast(theFactory.typeDef()).setHasLocalTransform(true);
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
		flipTm(0,0) *=  1.0;
		flipTm(1,2) *= -1.0;
		flipTm(2,1) *=  1.0;
		flipTm(3,3) *=  1.0;

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
		cache.unregister(lastKey.filePath, lastKey);
		lastKey = newKey;
		cache.registerInCache(lastKey.filePath, lastKey);
	}
}
