//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_geoutils.h"

#include <GA/GA_AttributeFilter.h>


using namespace VRayForHoudini;


namespace {

bool GEOgetAttribRange(const GA_AIFTuple *aiftuple, const GA_Attribute *attr, const GA_Range &range, int result[])
{
	return aiftuple->getRange(attr, range, result, 0, 1);
}

bool GEOgetAttribRange(const GA_AIFTuple *aiftuple, const GA_Attribute *attr, const GA_Range &range, float result[])
{
	return aiftuple->getRange(attr, range, result, 0, 1);
}

bool GEOgetAttribRange(const GA_AIFTuple *aiftuple, const GA_Attribute *attr, const GA_Range &range, VRay::Vector result[])
{
	return aiftuple->getRange(attr, range, &result[0].x, 0, 3);
}

bool GEOgetAttribRange(const GA_AIFTuple *aiftuple, const GA_Attribute *attr, const GA_Range &range, VRay::Color result[])
{
	return aiftuple->getRange(attr, range, &result[0].r, 0, 3);
}

bool GEOgetAttrib(const GA_AIFTuple *aiftuple, const GA_Attribute *attr, GA_Offset ai, int &result)
{
	return aiftuple->get(attr, ai, &result, 1);
}

bool GEOgetAttrib(const GA_AIFTuple *aiftuple, const GA_Attribute *attr, GA_Offset ai, float &result)
{
	return aiftuple->get(attr, ai, &result, 1);
}

bool GEOgetAttrib(const GA_AIFTuple *aiftuple, const GA_Attribute *attr, GA_Offset ai, VRay::Vector &result)
{
	return aiftuple->get(attr, ai, &result.x, 3);
}

bool GEOgetAttrib(const GA_AIFTuple *aiftuple, const GA_Attribute *attr, GA_Offset ai, VRay::Color &result)
{
	return aiftuple->get(attr, ai, &result.r, 3);
}

template < typename T >
bool GEOgetDataFromAttributeT(const GA_Attribute *attr,
											 const GEOPrimList &primList,
											 T &data)
{
	GA_ROAttributeRef attrref(attr);
	if (   attrref.isInvalid()
		|| !attrref.getAIFTuple())
	{
		return false;
	}

	bool res = true;
	const GA_AIFTuple *aiftuple = attrref.getAIFTuple();
	int idx = 0;
	switch (attr->getOwner()) {
		case GA_ATTRIB_VERTEX:
		{
			for (const GEO_Primitive *prim : primList) {
				GA_Range range = prim->getVertexRange();
				res &= GEOgetAttribRange(aiftuple, attr, range, &(data[idx]));
				idx += range.getEntries();
			}
			break;
		}
		case GA_ATTRIB_POINT:
		{
			for (const GEO_Primitive *prim : primList) {
				GA_Range range = prim->getPointRange();
				res &= GEOgetAttribRange(aiftuple, attr, range, &(data[idx]));
				idx += range.getEntries();
			}
			break;
		}
		case GA_ATTRIB_PRIMITIVE:
		{
			for (const GEO_Primitive *prim : primList) {
				res &= GEOgetAttrib(aiftuple, attr, prim->getMapOffset(), data[idx]);
				++idx;
			}
			break;
		}
		default:
			break;
	}

	return res;
}

}

GA_AttributeFilter& VRayForHoudini::GEOgetV3AttribFilter()
{
	static GA_AttributeFilter theV3Filter = GA_AttributeFilter::selectAnd(
												GA_AttributeFilter::selectFloatTuple(false),
												GA_AttributeFilter::selectByTupleSize(3)
												);
	return theV3Filter;
}

bool VRayForHoudini::GEOgetDataFromAttribute(const GA_Attribute *attr,
											const GEOPrimList &primList,
											VRay::VUtils::IntRefList &data)
{ return GEOgetDataFromAttributeT< VRay::VUtils::IntRefList >(attr, primList, data); }

bool VRayForHoudini::GEOgetDataFromAttribute(const GA_Attribute *attr,
											const GEOPrimList &primList,
											VRay::VUtils::FloatRefList &data)
{ return GEOgetDataFromAttributeT< VRay::VUtils::FloatRefList >(attr, primList, data); }

bool VRayForHoudini::GEOgetDataFromAttribute(const GA_Attribute *attr,
											const GEOPrimList &primList,
											VRay::VUtils::VectorRefList &data)
{ return GEOgetDataFromAttributeT< VRay::VUtils::VectorRefList >(attr, primList, data); }

bool VRayForHoudini::GEOgetDataFromAttribute(const GA_Attribute *attr,
											const GEOPrimList &primList,
											VRay::VUtils::ColorRefList &data)
{ return GEOgetDataFromAttributeT< VRay::VUtils::ColorRefList >(attr, primList, data); }

exint VRayForHoudini::getGEOPrimListHash(const GEOPrimList &primList)
{
	exint primListHash = 0;

	for (int i = 0; i < primList.size(); ++i) {
		primListHash ^= primList(i)->getMapOffset();
	}

	return primListHash;
}
