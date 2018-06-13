//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <QMutexLocker>

#include "vfh_vray.h"
#include "vfh_gu_cache.h"

#include "gu_vrayproxyref.h"

#include <GT/GT_GEODetail.h>

using namespace VRayForHoudini;

typedef QMap<uint32, VRayProxyRefItem> VRayProxyRefCache;
typedef VRayBaseRefFactory<VRayProxyRef> VRayProxyRefFactory;

static GA_PrimitiveTypeId theTypeId(-1);
static VRayProxyRefFactory theFactory("VRayProxyRef");
static VRayProxyRefCache vrayProxyRefCache;

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
{}

VRayProxyRef::~VRayProxyRef()
{}

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
	return true;
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
	key.objectPath = getObjectPath();
	key.objectType = static_cast<VRayProxyObjectType>(getObjectType());
	key.objectID = 0;
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

static QMutex mutex;

int VRayProxyRef::detailRebuild()
{
	const VRayProxyRefKey &vrmeshKey = getKey();

	VRayProxyRefCache::iterator it;
	QMutexLocker locker(&mutex); {
		it = vrayProxyRefCache.find(vrmeshKey.hash());
		if (it == vrayProxyRefCache.end()) {
			it = vrayProxyRefCache.insert(vrmeshKey.hash(), getVRayProxyDetail(vrmeshKey));
		}
	}

	const VRayProxyRefItem &vrmeshItem = it.value();

	const int res = m_detail != vrmeshItem.gdp;
	m_detail = vrmeshItem.gdp;

	printf("P %i\n", m_detail.gdp()->getUniqueId());

	return res;
}