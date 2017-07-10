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
#include <OBJ/OBJ_Node.h>

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

	/// Merge overrides overwriting existing values.
	void mergeOverrides(const MtlOverrideItems &items);

	/// Merge overrides not overwriting existing values.
	void appendOverrides(const MtlOverrideItems &items);

	/// Material node (SHOP, VOP).
	OP_Node *matNode;

	/// Material overrides from stylesheet or SHOP overrides.
	MtlOverrideItems overrides;
};

struct SheetTarget {
	enum SheetTargetType {
		sheetTargetUnknown = -1,
		sheetTargetAll,
		sheetTargetPrimitive,
	};

	explicit SheetTarget(SheetTargetType targetType=sheetTargetUnknown)
		: targetType(targetType)
	{}

	struct TargetPrimitive {
		enum TargetType {
			primitiveTypeUnknown = -1,
			primitiveTypeGroup,
		};

		explicit TargetPrimitive(TargetType targetType=primitiveTypeUnknown)
			: targetType(targetType)
		{}

		UT_String group;

		TargetType targetType;
	} primitive;

	SheetTargetType targetType;
};

struct TargetStyleSheet {
	explicit TargetStyleSheet(SheetTarget::SheetTargetType target=SheetTarget::sheetTargetUnknown)
		: target(target)
	{}

	SheetTarget target;
	PrimMaterial overrides;
};

typedef VUtils::Table<TargetStyleSheet, -1> TargetStyleSheets;

struct ObjectStyleSheet {
	ObjectStyleSheet()
	{}

	ObjectStyleSheet(const ObjectStyleSheet &other) {
		*this = other;
	}

	ObjectStyleSheet& operator=(const ObjectStyleSheet &other) {
		styles.copy(other.styles);
		return *this;
	}

	void operator+=(const ObjectStyleSheet &other) {
		styles += other.styles;
	}

	TargetStyleSheets styles;
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

	static void buildAttributesList(const GA_Detail &gdp, GA_AttributeOwner owner, GEOAttribList &attrList);

private:
	static void addAttributesAsOverrides(const GEOAttribList &attrList, GA_Offset offs, MtlOverrideItems &overrides);

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

void parseObjectStyleSheet(OBJ_Node &objNode, ObjectStyleSheet &objSheet, fpreal t);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H
