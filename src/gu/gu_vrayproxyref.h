//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAYPROXYREF_H
#define VRAY_FOR_HOUDINI_VRAYPROXYREF_H

#include "vfh_includes.h"
#include "vfh_vrayproxyutils.h"
#include "vfh_VRayProxyRefBase.h"

namespace VRayForHoudini {

/// Custom primitive that represents *.vrmesh files
/// implemented as packed primitive.
class VRayProxyRef
	: public VRayProxyRefBase
{
public:
	static void install(GA_PrimitiveFactory *gafactory);

	VRayProxyRef();
	VRayProxyRef(const VRayProxyRef &src);
	virtual ~VRayProxyRef();

	// From GU_PackedImpl.
	GU_PackedFactory *getFactory() const VRAY_OVERRIDE;
	GU_PackedImpl *copy() const VRAY_OVERRIDE;
	bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;
	bool getLocalTransform(UT_Matrix4D &m) const VRAY_OVERRIDE;
	bool getBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;

private:
	/// Returns a key for cache look-up.
	VRayProxyRefKey getKey() const;

	int detailRebuild() VRAY_OVERRIDE;

	void updateCacheVars(const VRayProxyRefKey &newKey);

	VRayProxyRefKey lastKey;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAYPROXYREF_H
