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

#include <hash_map.h>

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
