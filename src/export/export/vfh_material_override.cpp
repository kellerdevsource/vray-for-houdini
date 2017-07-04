//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//   https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_material_override.h"

#include <SHOP/SHOP_GeoOverride.h>
#include <GA/GA_AttributeFilter.h>

#include <rapidjson/document.h>


using namespace VRayForHoudini;
using namespace rapidjson;

void PrimMaterial::mergeOverrides(const MtlOverrideItems &items)
{
	FOR_CONST_IT (MtlOverrideItems, it, items) {
		overrides.insert(it.key(), it.data());
	}
}

void PrimMaterial::appendOverrides(const MtlOverrideItems &items)
{
	FOR_CONST_IT (MtlOverrideItems, it, items) {
		if (overrides.find(it.key()) == overrides.end()) {
			overrides.insert(it.key(), it.data());
		}
	}
}

static void parseStyleSheetOverrides(const Value &paramOverrides, MtlOverrideItems &overrides)
{
	for (Value::ConstMemberIterator pIt = paramOverrides.MemberBegin(); pIt != paramOverrides.MemberEnd(); ++pIt) {
		const char *parmName = pIt->name.GetString();
		const Value &paramJsonValue = pIt->value;

		MtlOverrideItems::iterator moIt = overrides.find(parmName);
		if (moIt != overrides.end()) {
			// There is already some top-level override for this parameter.
			continue;
		}

		// NOTES:
		//  * Assuming current array value is a color
		//  * Only first 3 components will be used for vectors
		if (paramJsonValue.IsArray()) {
			if (paramJsonValue.Size() >= 3) {
				MtlOverrideItem &overrideItem = overrides[parmName];
				overrideItem.setType(MtlOverrideItem::itemTypeVector);
				overrideItem.valueVector.set(paramJsonValue[0].GetDouble(),
												paramJsonValue[1].GetDouble(),
												paramJsonValue[2].GetDouble());
			}
			else {
				// TODO: Implement other cases / print warning.
			}
		}
		else if (paramJsonValue.IsDouble()) {
			MtlOverrideItem &overrideItem = overrides[parmName];
			overrideItem.setType(MtlOverrideItem::itemTypeDouble);
			overrideItem.valueDouble = paramJsonValue.GetDouble();
		}
		else if (paramJsonValue.IsString()) {
			// TODO: String type.
		}
		else {
			// TODO: Implement other cases / print warning.
		}
	}
}

void VRayForHoudini::mergeStyleSheet(PrimMaterial &primMaterial, const QString &styleSheet, fpreal t, int materialOnly)
{
	Document document;
	document.Parse(styleSheet.toLocal8Bit().constData());

	//
	// {
	// 	"styles":[
	// 		{
	// 			"overrides":{
	// 				"material":{
	// 					"name":"/shop/vraymtl"
	// 				},
	// 				"materialParameters":{
	// 					"diffuse":[0.80046379566192627,0.510895013809204102,0.775474309921264648,1
	// 					]
	// 				}
	// 			}
	// 		}
	// 	]
	// }
	//
	if (!document.HasMember("styles"))
		return;

	const Value &styles = document["styles"];
	UT_ASSERT(styles.IsArray());

	for (Value::ConstValueIterator it = styles.Begin(); it != styles.End(); ++it) {
		const Value &style = *it;
		if (style.IsObject() && style.HasMember("overrides")) {
			const Value &styleOver = style["overrides"];

			if (styleOver.HasMember("material")) {
				const Value &mtlOver = styleOver["material"];
				if (mtlOver.HasMember("name")) {
					const char *matPath = mtlOver["name"].GetString();

					OP_Node *matNode = getOpNodeFromPath(matPath, t);
					if (matNode) {
						// There is already some top-level material assigned.
						if (!primMaterial.matNode) {
							primMaterial.matNode = matNode;
						}
					}

					if (materialOnly) {
						break;
					}
				}
			}

			if (!materialOnly && styleOver.HasMember("materialParameters")) {
				const Value &paramOverrides = styleOver["materialParameters"];

				parseStyleSheetOverrides(paramOverrides, primMaterial.overrides);
			}
		}
	}
}

void VRayForHoudini::mergeMaterialOverrides(PrimMaterial &primMaterial, const UT_String &matPath, const UT_String &materialOverrides, fpreal t, int materialOnly)
{
	// There is already some top-level material assigned.
	if (!primMaterial.matNode) {
		primMaterial.matNode = getOpNodeFromPath(matPath, t);
	}

	if (primMaterial.matNode && !materialOnly) { 
		//
		// { "diffuser" : 1.0, "diffuseg" : 1.0, "diffuseb" : 1.0 }
		//
		SHOP_GeoOverride mtlOverride;
		if (mtlOverride.load(materialOverrides.buffer())) {
			UT_StringArray mtlOverrideKeys;
			mtlOverride.getKeys(mtlOverrideKeys);

			for (const UT_StringHolder &key : mtlOverrideKeys) {
				// Channel for vector components.
				// NOTE: Only first 3 components will be used for vectors.
				int channelIIdx = -1;

				PRM_Parm *keyParm = primMaterial.matNode->getParmList()->getParmPtrFromChannel(key, &channelIIdx);
				if (keyParm && channelIIdx >= 0 && channelIIdx < 4) {
					const PRM_Type &keyParmType = keyParm->getType();
					const char *parmName = keyParm->getToken();

					MtlOverrideItems::iterator moIt = primMaterial.overrides.find(parmName);
					if (moIt != primMaterial.overrides.end()) {
						// There is already some top-level override for this parameter.
						continue;
					}

					if (keyParmType.isFloatType()) {
						MtlOverrideItem &overrideItem = primMaterial.overrides[parmName];

						fpreal channelValue = 0.0;
						mtlOverride.import(key, channelValue);

						if (keyParmType.getFloatType() == PRM_Type::PRM_FLOAT_RGBA) {
							overrideItem.setType(MtlOverrideItem::itemTypeVector);
							overrideItem.valueVector[channelIIdx] = channelValue;
						}
						else if (channelIIdx == 0) {
							overrideItem.setType(MtlOverrideItem::itemTypeDouble);
							overrideItem.valueDouble = channelValue;
						}
						else {
							// TODO: Implement other cases / print warning.
						}
					}
					else if (keyParmType.isOrdinalType() && channelIIdx == 0) {
						MtlOverrideItem &overrideItem = primMaterial.overrides[keyParm->getToken()];
						overrideItem.setType(MtlOverrideItem::itemTypeInt);
						mtlOverride.import(key, overrideItem.valueInt);
					}
					else if (keyParmType.isStringType()) {
						// TODO: String type.
					}
					else {
						// TODO: Implement other cases / print warning.
					}
				}
			}
		}
	}
}

void VRayForHoudini::mergeMaterialOverride(PrimMaterial &primMaterial,
										   const GA_ROHandleS &materialStyleSheetHndl,
										   const GA_ROHandleS &materialPathHndl,
										   const GA_ROHandleS &materialOverrideHndl,
										   GA_Offset primOffset,
										   fpreal t)
{
	if (materialStyleSheetHndl.isValid()) {
		const QString &styleSheet = materialStyleSheetHndl.get(primOffset);
		if (!styleSheet.isEmpty()) {
			mergeStyleSheet(primMaterial, styleSheet, t);
		}
	}
	else if (materialPathHndl.isValid()) {
		const UT_String &matPath = materialPathHndl.get(primOffset);

		UT_String materialOverrides;
		if (materialOverrideHndl.isValid()) {
			materialOverrides = materialOverrideHndl.get(primOffset);
		}

		if (!matPath.equal("")) {
			mergeMaterialOverrides(primMaterial, matPath, materialOverrides, t);
		}
	}
}

void MtlOverrideAttrExporter::buildAttributesList(const GA_Detail &gdp, GA_AttributeOwner owner, GEOAttribList &attrList)
{
	gdp.getAttributes().matchAttributes(
		GA_AttributeFilter::selectAnd(GA_AttributeFilter::selectFloatTuple(false),
			                            GA_AttributeFilter::selectByTupleRange(3,4)),
		owner, attrList);
	gdp.getAttributes().matchAttributes(GA_AttributeFilter::selectAlphaNum(), owner, attrList);
}

void MtlOverrideAttrExporter::addAttributesAsOverrides(const GEOAttribList &attrList, GA_Offset offs, MtlOverrideItems &overrides)
{
	for (const GA_Attribute *attr : attrList) {
		if (!attr)
			continue;

		const char *attrName = attr->getName().buffer();

		MtlOverrideItems::iterator moIt = overrides.find(attrName);
		if (moIt == overrides.end()) {
			MtlOverrideItem &overrideItem = overrides[attrName];

			GA_ROHandleV3 v3Hndl(attr);
			GA_ROHandleV4 v4Hndl(attr);
			GA_ROHandleS sHndl(attr);
			GA_ROHandleF fHndl(attr);

			if (v4Hndl.isValid()) {
				const UT_Vector4F &c = v4Hndl.get(offs);
				overrideItem.setType(MtlOverrideItem::itemTypeVector);
				overrideItem.valueVector = utVectorVRayVector(c);
			}
			else if (v3Hndl.isValid()) {
				const UT_Vector3F &c = v3Hndl.get(offs);
				overrideItem.setType(MtlOverrideItem::itemTypeVector);
				overrideItem.valueVector = utVectorVRayVector(c);
			}
			else if (sHndl.isValid()) {
				overrideItem.setType(MtlOverrideItem::itemTypeString);
				overrideItem.valueString = sHndl.get(offs);
			}
			else if (fHndl.isValid()) {
				overrideItem.setType(MtlOverrideItem::itemTypeDouble);
				overrideItem.valueDouble = fHndl.get(offs);
			}
		}
	}
}

static void parseStyleSheetTarget(const Value &target, SheetTarget &sheetTarget)
{
	for (Value::ConstMemberIterator pIt = target.MemberBegin(); pIt != target.MemberEnd(); ++pIt) {
		const UT_String key(pIt->name.GetString());
		const Value &value = pIt->value;
		if (key.equal("group")) {
			sheetTarget.targetType = SheetTarget::sheetTargetPrimitive;
			sheetTarget.primitive.targetType = SheetTarget::TargetPrimitive::primitiveTypeGroup;
			sheetTarget.primitive.group = value.GetString();
		}
		else {
			// ...
		}
	}
}

void VRayForHoudini::parseObjectStyleSheet(OBJ_Node &objNode, ObjectStyleSheet &objSheet, fpreal t)
{
	using namespace rapidjson;

	UT_String styleSheet;
	objNode.evalString(styleSheet, "shop_materialstylesheet", 0, t);

	Document document;
	document.Parse(styleSheet.buffer());

	if (!document.HasMember("styles"))
		return;

	const Value &styles = document["styles"];
	UT_ASSERT(styles.IsArray());

	//
	// {
	//     "styles":[
	//         {
	//             "label":"Style",
	//             "target":{
	//                 "label":"Target",
	//                 "group":"ballgrp"
	//             },
	//             "overrides":{
	//                 "material":{
	//                     "name":"`opfullpath('/shop/vrmat_balls_packedAssign')`"
	//                 }
	//             }
	//         }
	//     ]
	// }
	// 

	for (Value::ConstValueIterator it = styles.Begin(); it != styles.End(); ++it) {
		const Value &style = *it;
		UT_ASSERT(style.IsObject());

		if (style.HasMember("overrides")) {
			const Value &styleOver = style["overrides"];

			PrimMaterial styleOverrides;

			if (styleOver.HasMember("material")) {
				const Value &mtlOver = styleOver["material"];
				if (mtlOver.HasMember("name")) {
					const char *matPath = mtlOver["name"].GetString();

					styleOverrides.matNode = getOpNodeFromPath(matPath, t);
				}
			}

			if (styleOver.HasMember("materialParameters")) {
				const Value &paramOverrides = styleOver["materialParameters"];

				parseStyleSheetOverrides(paramOverrides, styleOverrides.overrides);
			}

			if (style.HasMember("target")) {
				const Value &target = style["target"];

				// If no specific target will be found later, then style will apply to all.
				TargetStyleSheet targetStyle(SheetTarget::sheetTargetAll);
				targetStyle.overrides = styleOverrides;

				if (target.HasMember("subTarget")) {
					const Value &subTarget = target["subTarget"];
					parseStyleSheetTarget(subTarget, targetStyle.target);
				}
				else {
					parseStyleSheetTarget(target, targetStyle.target);
				}

				objSheet.styles += targetStyle;
			}
		}
	}
}
