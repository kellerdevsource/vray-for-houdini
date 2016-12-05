//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_GEOUTILS_H
#define VRAY_FOR_HOUDINI_GEOUTILS_H

#include "vfh_vray.h"

#include <GEO/GEO_Primitive.h>


namespace VRayForHoudini {

namespace GEO {

typedef UT_ValArray< const GEO_Primitive* > GEOPrimList;

bool getDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
								 VRay::VUtils::IntRefList &data);

bool getDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
								 VRay::VUtils::FloatRefList &data);

bool getDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
								 VRay::VUtils::VectorRefList &data);

} // namespace GEO
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_GEOUTILS_H
