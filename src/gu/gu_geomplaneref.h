//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_GEOMPLANEREF_H
#define VRAY_FOR_HOUDINI_GEOMPLANEREF_H

#include "vfh_includes.h"
#include "vfh_GeomPlaneRefOptions.h"

namespace VRayForHoudini {

/// V-Ray Infinite Plane implemented as a packed primitive.
class GeomPlaneRef
	: public GeomPlaneRefOptions
{
public:
	static GA_PrimitiveTypeId typeId();
	static void install(GA_PrimitiveFactory *gafactory);

	GeomPlaneRef();
	GeomPlaneRef(const GeomPlaneRef &src);
	virtual ~GeomPlaneRef();

	GU_PackedFactory *getFactory() const VRAY_OVERRIDE;
	GU_PackedImpl *copy() const VRAY_OVERRIDE;
	bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;

private:
	void detailRebuild() VRAY_OVERRIDE;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_GEOMPLANEREF_H
