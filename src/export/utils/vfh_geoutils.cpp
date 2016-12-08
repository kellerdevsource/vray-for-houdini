//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_geoutils.h"

using namespace VRayForHoudini;


bool GEO::getDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
							   VRay::VUtils::IntRefList &data)
{
	bool res = false;
	GA_ROAttributeRef attrref(attr);
	if (   attrref.isValid()
		&& attrref.getAIFTuple())
	{
		const GA_AIFTuple *aiftuple = attrref.getAIFTuple();
		int idx = 0;
		for (const GEO_Primitive *prim : primList) {
			switch (attr->getOwner()) {
				case GA_ATTRIB_VERTEX:
				{
					GA_Range range = prim->getVertexRange();
					res |= aiftuple->getRange(attr, range, &(data[idx]), 0, 1);
					idx += range.getEntries();
					break;
				}
				case GA_ATTRIB_POINT:
				{
					GA_Range range = prim->getPointRange();
					res |= aiftuple->getRange(attr, range, &(data[idx]), 0, 1);
					idx += range.getEntries();
					break;
				}
				case GA_ATTRIB_PRIMITIVE:
				{
					res |= aiftuple->get(attr, prim->getMapOffset(), &(data[idx]), 1);
					++idx;
					break;
				}
				default:
					break;
			}
		}
	}
	return res;
}


bool GEO::getDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
							   VRay::VUtils::FloatRefList &data)
{
	bool res = false;
	GA_ROAttributeRef attrref(attr);
	if (   attrref.isValid()
		&& attrref.getAIFTuple())
	{
		const GA_AIFTuple *aiftuple = attrref.getAIFTuple();
		int idx = 0;
		for (const GEO_Primitive *prim : primList) {
			switch (attr->getOwner()) {
				case GA_ATTRIB_VERTEX:
				{
					GA_Range range = prim->getVertexRange();
					res |= aiftuple->getRange(attr, range, &(data[idx]), 0, 1);
					idx += range.getEntries();
					break;
				}
				case GA_ATTRIB_POINT:
				{
					GA_Range range = prim->getPointRange();
					res |= aiftuple->getRange(attr, range, &(data[idx]), 0, 1);
					idx += range.getEntries();
					break;
				}
				case GA_ATTRIB_PRIMITIVE:
				{
					res |= aiftuple->get(attr, prim->getMapOffset(), &(data[idx]), 1);
					++idx;
					break;
				}
				default:
					break;
			}
		}
	}
	return res;
}


bool GEO::getDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
							   VRay::VUtils::VectorRefList &data)
{
	bool res = false;
	GA_ROAttributeRef attrref(attr);
	if (   attrref.isValid()
		&& attrref.getAIFTuple())
	{
		const GA_AIFTuple *aiftuple = attrref.getAIFTuple();
		int idx = 0;
		for (const GEO_Primitive *prim : primList) {
			switch (attr->getOwner()) {
				case GA_ATTRIB_VERTEX:
				{
					GA_Range range = prim->getVertexRange();
					res |= aiftuple->getRange(attr, range, &(data[idx].x), 0, 3);
					idx += range.getEntries();
					break;
				}
				case GA_ATTRIB_POINT:
				{
					GA_Range range = prim->getPointRange();
					res |= aiftuple->getRange(attr, range, &(data[idx].x), 0, 3);
					idx += range.getEntries();
					break;
				}
				case GA_ATTRIB_PRIMITIVE:
				{
					res |= aiftuple->get(attr, prim->getMapOffset(), &(data[idx].x), 3);
					++idx;
					break;
				}
				default:
					break;
			}
		}
	}
	return res;
}


bool GEO::getDataFromAttribute(const GA_Attribute *attr, const GEOPrimList &primList,
							   VRay::VUtils::ColorRefList &data)
{
	bool res = false;
	GA_ROAttributeRef attrref(attr);
	if (   attrref.isValid()
		&& attrref.getAIFTuple())
	{
		const GA_AIFTuple *aiftuple = attrref.getAIFTuple();
		int idx = 0;
		for (const GEO_Primitive *prim : primList) {
			switch (attr->getOwner()) {
				case GA_ATTRIB_VERTEX:
				{
					GA_Range range = prim->getVertexRange();
					res |= aiftuple->getRange(attr, range, &(data[idx].r), 0, 3);
					idx += range.getEntries();
					break;
				}
				case GA_ATTRIB_POINT:
				{
					GA_Range range = prim->getPointRange();
					res |= aiftuple->getRange(attr, range, &(data[idx].r), 0, 3);
					idx += range.getEntries();
					break;
				}
				case GA_ATTRIB_PRIMITIVE:
				{
					res |= aiftuple->get(attr, prim->getMapOffset(), &(data[idx].r), 3);
					++idx;
					break;
				}
				default:
					break;
			}
		}
	}
	return res;
}
