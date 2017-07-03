//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H
#define VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H

#include "vfh_attr_utils.h"
#include "vfh_geoutils.h"

#include <QString>

#include <hash_map.h>
#include <GA/GA_Handle.h>

namespace VRayForHoudini {

struct MtlOverrideItem {
	enum MtlOverrideItemType {
		itemTypeNone = 0,
		itemTypeInt,
		itemTypeDouble,
		itemTypeVector,
		itemTypeString,
	};

	MtlOverrideItem()
		: type(itemTypeNone)
		, valueInt(0)
		, valueDouble(0.0)
		, valueVector(0.0f, 0.0f, 0.0f)
	{}

	/// Sets override value type.
	void setType(MtlOverrideItemType value) { type = value; }

	/// Returns override value type.
	MtlOverrideItemType getType() const { return type; }

	/// Override value type.
	MtlOverrideItemType type;

	exint valueInt;
	fpreal valueDouble;
	VRay::Vector valueVector;
	QString valueString;
};

typedef VUtils::HashMap<MtlOverrideItem> MtlOverrideItems;

struct PrimMaterial {
	PrimMaterial()
		: matNode(nullptr)
	{}

	/// Material node (SHOP, VOP).
	OP_Node *matNode;

	/// Material overrides from stylesheet or SHOP overrides.
	MtlOverrideItems overrides;
};

struct MtlOverrideAttrExporter {
	explicit MtlOverrideAttrExporter(const GA_Detail &gdp) {
		buildAttributesList(gdp, GA_ATTRIB_PRIMITIVE, primAttrList);
		buildAttributesList(gdp, GA_ATTRIB_POINT,     pointAttrList);
	}

	void fromPrimitive(MtlOverrideItems &overrides, GA_Offset offs) const {
		addAttributesAsOverrides(primAttrList, offs, overrides);
	}

	void fromPoint(MtlOverrideItems &overrides, GA_Offset offs) const {
		addAttributesAsOverrides(pointAttrList, offs, overrides);
	}

	static void buildAttributesList(const GA_Detail &gdp, GA_AttributeOwner owner, GEOAttribList &attrList) {
		gdp.getAttributes().matchAttributes(
			GA_AttributeFilter::selectAnd(GA_AttributeFilter::selectFloatTuple(false),
			                              GA_AttributeFilter::selectByTupleRange(3,4)),
			owner, attrList);
		gdp.getAttributes().matchAttributes(GA_AttributeFilter::selectAlphaNum(), owner, attrList);
	}

private:
	static void addAttributesAsOverrides(const GEOAttribList &attrList, GA_Offset offs, MtlOverrideItems &overrides) {
		for (const GA_Attribute *attr : attrList) {
			if (!attr)
				continue;

			const char *attrName = getAttributeName(attr->getName().buffer());

			MtlOverrideItems::iterator moIt = overrides.find(attrName);
			if (moIt == overrides.end()) {
				MtlOverrideItem &overrideItem = overrides[attrName];

				GA_ROHandleV3 v3Hndl(attr);
				GA_ROHandleV4 v4Hndl(attr);
				GA_ROHandleS sHndl(attr);
				GA_ROHandleF fHndl(attr);

				if (sHndl.isValid()) {
					overrideItem.setType(MtlOverrideItem::itemTypeString);
					overrideItem.valueString = sHndl.get(offs);
				}
				else if (fHndl.isValid()) {
					overrideItem.setType(MtlOverrideItem::itemTypeDouble);
					overrideItem.valueDouble = fHndl.get(offs);
				}
				else if (v4Hndl.isValid()) {
					const UT_Vector4F &c = v4Hndl.get(offs);
					overrideItem.setType(MtlOverrideItem::itemTypeVector);
					overrideItem.valueVector = utVectorVRayVector(c);
				}
				else if (v3Hndl.isValid()) {
					const UT_Vector3F &c = v3Hndl.get(offs);
					overrideItem.setType(MtlOverrideItem::itemTypeVector);
					overrideItem.valueVector = utVectorVRayVector(c);
				}
			}
		}
	}

	static const char* getAttributeName(const char *attrName) {
		const char *attrPacked = ::strchr(attrName, ':');
		if (attrPacked) {
			return attrPacked + 1;
		}
		return attrName;
	}

	GEOAttribList primAttrList;
	GEOAttribList pointAttrList;
};

void mergeStyleSheet(PrimMaterial &primMaterial,
					 const QString &styleSheet,
					 fpreal t,
					 int materialOnly=false);

void mergeMaterialOverrides(PrimMaterial &primMaterial,
							const UT_String &matPath,
							const UT_String &materialOverrides,
							fpreal t,
							int materialOnly=false);

void mergeMaterialOverride(PrimMaterial &primMaterial,
						   const GA_ROHandleS &materialStyleSheetHndl,
						   const GA_ROHandleS &materialPathHndl,
						   const GA_ROHandleS &materialOverrideHndl,
						   GA_Offset primOffset,
						   fpreal t);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H
