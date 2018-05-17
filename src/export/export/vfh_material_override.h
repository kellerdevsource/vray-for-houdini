//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H
#define VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H

#include "vfh_includes.h"
#include "vfh_geoutils.h"
#include "vfh_defines.h"

#include <QString>

#include <GA/GA_Handle.h>
#include <OBJ/OBJ_Node.h>
#include <STY/STY_Styler.h>

#ifdef HDK_16_5
#include <STY/STY_Results.h>
#include <STY/STY_ResultsFilter.h>
#else
#include <STY/STY_OverrideValues.h>
#include <STY/STY_OverrideValuesFilter.h>
#endif

#include <STY/STY_StylerGroup.h>
#include <GSTY/GSTY_SubjectPrimGroup.h>

class GSTY_BundleMap;

#ifdef HDK_16_5
typedef STY_OverrideScript STY_OverrideScriptEntry;
typedef STY_Override STY_OverrideEntry;
typedef STY_OverrideCategory STY_OverridesEntry;
typedef STY_OverrideBlock  STY_OverrideSetsEntry;
typedef STY_Style STY_StyleEntry;
typedef STY_StyleFlags STY_StyleEntryFlags;
typedef STY_Material STY_MaterialEntry;
typedef STY_Result STY_OverrideValue;
typedef STY_ResultMap STY_OverrideValueMap;
typedef STY_Results STY_OverrideValues;
typedef STY_ResultsFilter STY_OverrideValuesFilter;
typedef STY_Constant STY_Value;
typedef STY_OverrideHandle STY_OverrideEntryHandle;
typedef STY_OverrideMap STY_OverrideEntryMap;
typedef STY_ConstantHandle STY_OptionEntryHandle;
#endif

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
	/// @param value Override value type.
	void setType(MtlOverrideItemType value) { type = value; }

	/// Returns override value type.
	MtlOverrideItemType getType() const { return type; }

	const char *getString() const { return qPrintable(valueString); }

	/// Override value type.
	MtlOverrideItemType type;

	exint valueInt;
	fpreal valueDouble;
	VRay::Vector valueVector;
	QString valueString;
};

typedef QMap<QString, MtlOverrideItem> MtlOverrideItems;

enum OverrideAppendMode {
	overrideAppend = 0, ///< Append new keys only.
	overrideMerge, ///< Merge overrides overwriting existing key.
};

struct PrimMaterial {
	PrimMaterial()
		: matNode(nullptr)
	{}

	/// Merge overrides.
	void append(const PrimMaterial &other, OverrideAppendMode mode=overrideAppend);

	/// Merge overrides.
	/// @param items Override items.
	/// @param mode Merge mode.
	void appendOverrides(const MtlOverrideItems &items, OverrideAppendMode mode=overrideAppend);

	/// Material node (SHOP, VOP).
	OP_Node *matNode;

	/// Material overrides from style sheet or SHOP overrides.
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

	static void buildAttributesList(const GA_Detail &gdp, GA_AttributeOwner owner, GEOAttribList &attrList);

private:
	static void addAttributesAsOverrides(const GEOAttribList &attrList, GA_Offset offs, MtlOverrideItems &overrides);

	GEOAttribList primAttrList;
	GEOAttribList pointAttrList;
};

void appendOverrideValues(const STY_Styler &styler, PrimMaterial &primMaterial, OverrideAppendMode mode=overrideAppend, int materialOnly=false);

/// Append material overrides from the style sheet.
/// @param primMaterial Material override to append to.
/// @param styleSheet Style sheet buffer.
/// @param t Time.
/// @param mode Append or merge values.
/// @param materialOnly Process material tag only.
void appendStyleSheet(PrimMaterial &primMaterial,
					  const UT_StringHolder &styleSheet,
					  fpreal t,
					  OverrideAppendMode mode=overrideAppend,
					  int materialOnly=false);

/// Append material overrides from material override attributes.
/// @param primMaterial Material override to append to.
/// @param matPath Material OP path.
/// @param materialOverrides Material overrides buffer.
/// @param t Time.
/// @param materialOnly Process material tag only.
void appendMaterialOverrides(PrimMaterial &primMaterial,
							const UT_String &matPath,
							const UT_String &materialOverrides,
							fpreal t,
							int materialOnly=false);

/// Append material overrides from pritimive override handles.
/// @param primMaterial Material override to append to.
/// @param materialStyleSheetHndl Style sheet handle.
/// @param materialPathHndl Material path handle.
/// @param materialOverrideHndl Material override handle.
/// @param offset Data offset for the handle.
/// @param t Time.
void appendMaterialOverride(PrimMaterial &primMaterial,
						   const GA_ROHandleS &materialStyleSheetHndl,
						   const GA_ROHandleS &materialPathHndl,
						   const GA_ROHandleS &materialOverrideHndl,
						   GA_Offset offset,
						   fpreal t);

/// Get styler for the object from "shop_materialstylesheet" attribute.
/// @param objNode OBJ node instance.
/// @param t Time.
STY_Styler getStylerForObject(OBJ_Node &objNode, fpreal t);

/// Get styler for the primitive.
/// @param topStyler Current top level styler.
/// @param prim Primitive instance.
STY_Styler getStylerForPrimitive(const STY_Styler &topStyler, const GEO_Primitive &prim);

/// Get styler for the object.
/// @param topStyler Current top level styler.
/// @param opNode Object node.
STY_Styler getStylerForObject(const STY_Styler &topStyler, const GSTY_BundleMap &bundles, const OP_Node &opNode);

/// Fills style sheet material overrides for a primitive.
/// @param topStyler Current top level styler.
/// @param prim Primitive instance.
/// @param primMaterial Material override to append to.
void getOverridesForPrimitive(const STY_Styler &topStyler, const GEO_Primitive &prim, PrimMaterial &primMaterial);

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_MATERIAL_OVERRIDE_H
