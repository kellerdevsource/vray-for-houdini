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
#include "vfh_prm_templates.h"
#include "vfh_log.h"
#include "vfh_attr_utils.h"

#include <SHOP/SHOP_GeoOverride.h>
#include <GA/GA_AttributeFilter.h>
#include <GEO/GEO_Detail.h>

#include <STY/STY_StylerGroup.h>
#include <STY/STY_TargetMatchStatus.h>
#include <STY/STY_OverrideValues.h>
#include <STY/STY_OverrideValuesFilter.h>
#include <GSTY/GSTY_SubjectPrim.h>
#include <GSTY/GSTY_SubjectGeoObject.h>
#include <GSTY/GSTY_BundleMap.h>
#include <OP/OP_StyleManager.h>

#include <UT/UT_Version.h>

using namespace VRayForHoudini;

namespace Styles {
	const char styles[] = "styles";

	const char flags[] = "flags";

	namespace Flags {
		const char mute[] = "mute";
		const char solo[] = "solo";
	}

	const char target[] = "target";

	namespace Target {
		const char subTarget[] = "subTarget";
		const char group[] = "group";
	}

	const char overrides[] = "overrides";

	namespace Overrides {
		const char material[] = "material";
		const char materialParameters[] = "materialParameters";

		namespace Material {
			const char name[] = "name";
		}
	}
}

void PrimMaterial::append(const PrimMaterial &other, OverrideAppendMode mode)
{
	if (mode == overrideMerge) {
		if (!matNode && other.matNode) {
			matNode = other.matNode;
		}
	}

	appendOverrides(other.overrides, mode);
}

void PrimMaterial::appendOverrides(const MtlOverrideItems &items, OverrideAppendMode mode)
{
	FOR_CONST_IT (MtlOverrideItems, it, items) {
		if (mode == overrideAppend) {
			if (overrides.find(it.key()) != overrides.end()) {
				continue;
			}
		}

		overrides[it.key()] = it.data();
	}
}

void MtlOverrideAttrExporter::buildAttributesList(const GA_Detail &gdp, GA_AttributeOwner owner, GEOAttribList &attrList)
{
	gdp.getAttributes().matchAttributes(
		GA_AttributeFilter::selectAnd(GA_AttributeFilter::selectAnd(GA_AttributeFilter::selectFloatTuple(false),
		                                                            GA_AttributeFilter::selectByTupleRange(3,4)),
		                              GA_AttributeFilter::selectNot(GA_AttributeFilter::selectByName("N"))
		),
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
		if (moIt != overrides.end())
			continue;
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

static void appendOverrideValues(const STY_OverrideValues &styOverrideValues, PrimMaterial &primMaterial, OverrideAppendMode mode=overrideAppend, int materialOnly=false)
{
	UT_Int64Array intVals;
	UT_Fpreal64Array floatVals;
	UT_StringArray stringVals;

	if (!styOverrideValues.size())
		return;

	for (const auto &styOverrideValueCategory : styOverrideValues) {
		const UT_StringHolder &key = styOverrideValueCategory.first;
		const STY_OverrideValueMap &values = styOverrideValueCategory.second;

		if (key.equal(Styles::Overrides::material)) {
			const auto &nameIt = values.find(Styles::Overrides::Material::name);
			if (nameIt != values.end()) {
				const auto &nameValuePair = *nameIt;

				const STY_OptionEntryHandle &nameOpt = nameValuePair.second.myValue;
				if (nameOpt->importOption(stringVals)) {
					if (!primMaterial.matNode ||
						mode == overrideMerge)
					{
						primMaterial.matNode = getOpNodeFromPath(stringVals[0].buffer());
					}
				}
			}
			if (materialOnly) {
				break;
			}
		}
		else if (key.equal(Styles::Overrides::materialParameters)) {
			for (const auto &value : values) {
				const UT_StringHolder &attrName = value.first;
				STY_OptionEntryHandle opt = value.second.myValue;

				if (mode == overrideAppend) {
					MtlOverrideItems::iterator moIt = primMaterial.overrides.find(attrName);
					if (moIt != primMaterial.overrides.end())
						continue;
				}

				MtlOverrideItem overrideItem;

				if (opt->importOption(intVals)) {
					if (intVals.size() == 1) {
						overrideItem.setType(MtlOverrideItem::itemTypeInt);
						overrideItem.valueInt = intVals(0);
					}
					else if (intVals.size() == 3 ||
							 intVals.size() == 4)
					{
						overrideItem.setType(MtlOverrideItem::itemTypeVector);
						overrideItem.valueVector = utVectorVRayVector(intVals);
					}
				}
				else if (opt->importOption(stringVals)) {
					if (stringVals.size() == 1) {
						overrideItem.setType(MtlOverrideItem::itemTypeString);
						overrideItem.valueString = stringVals(0).buffer();
					}
				}
				else if (opt->importOption(floatVals)) {
					if (floatVals.size() == 1) {
						overrideItem.setType(MtlOverrideItem::itemTypeDouble);
						overrideItem.valueDouble = floatVals(0);
					}
					else if (floatVals.size() == 3 ||
							 floatVals.size() == 4)
					{
						overrideItem.setType(MtlOverrideItem::itemTypeVector);
						overrideItem.valueVector = utVectorVRayVector(floatVals);
					}
				}

				if (overrideItem.getType() != MtlOverrideItem::itemTypeNone) {
					primMaterial.overrides[attrName.buffer()] = overrideItem;
				}
			}
		}
	}
}

void VRayForHoudini::appendOverrideValues(const STY_Styler &styler, PrimMaterial &primMaterial, OverrideAppendMode mode, int materialOnly)
{
	static const STY_OverrideValuesFilter styOverrideValuesFilter(nullptr);

	STY_OverrideValues styOverrideValues;
	styler.getOverrides(styOverrideValues, styOverrideValuesFilter);

	appendOverrideValues(styOverrideValues, primMaterial, mode, materialOnly);
}

void VRayForHoudini::appendStyleSheet(PrimMaterial &primMaterial, const UT_StringHolder &styleSheet, fpreal t, OverrideAppendMode mode, int materialOnly)
{
	const char *styleBuf = styleSheet.buffer();
	if (!UTisstring(styleBuf))
		return;

	const STY_StyleSheetHandle styStyleSheetHandle(new STY_StyleSheet(styleBuf, NULL, STY_LOAD_FOR_STYLING));
	const STY_Styler styStyler(styStyleSheetHandle);

	appendOverrideValues(styStyler, primMaterial, mode, materialOnly);
}

void VRayForHoudini::appendMaterialOverrides(PrimMaterial &primMaterial,
											 const UT_String &matPath,
											 const UT_String &materialOverrides,
											 fpreal t,
											 int materialOnly)
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

			UT_StringArray validKeys;

			// Check keys if there is already some top-level override for this parameter.
			// Checking here because vectors are processed as channels.
			for (const UT_StringHolder &key : mtlOverrideKeys) {
				int channelIIdx = -1;
				PRM_Parm *keyParm = primMaterial.matNode->getParmList()->getParmPtrFromChannel(key, &channelIIdx);
				if (keyParm && channelIIdx >= 0 && channelIIdx < 4) {
					const char *parmName = keyParm->getToken();

					MtlOverrideItems::iterator moIt = primMaterial.overrides.find(parmName);
					if (moIt == primMaterial.overrides.end()) {
						validKeys.append(key);
					}
				}
			}

			for (const UT_StringHolder &key : validKeys) {
				// Channel for vector components.
				// NOTE: Only first 3 components will be used for vectors.
				int channelIIdx = -1;

				PRM_Parm *keyParm = primMaterial.matNode->getParmList()->getParmPtrFromChannel(key, &channelIIdx);
				if (keyParm && channelIIdx >= 0 && channelIIdx < 4) {
					const PRM_Type &keyParmType = keyParm->getType();
					const char *parmName = keyParm->getToken();

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
						MtlOverrideItem &overrideItem = primMaterial.overrides[keyParm->getToken()];
						overrideItem.setType(MtlOverrideItem::itemTypeString);

						UT_WorkBuffer buf;
						mtlOverride.import(key, buf);

						overrideItem.valueString = buf.buffer();
					}
					else {
						// TODO: Implement other cases / print warning.
					}
				}
			}
		}
	}
}

void VRayForHoudini::appendMaterialOverride(PrimMaterial &primMaterial,
											const GA_ROHandleS &materialStyleSheetHndl,
											const GA_ROHandleS &materialPathHndl,
											const GA_ROHandleS &materialOverrideHndl,
											GA_Offset primOffset,
											fpreal t)
{
	if (materialStyleSheetHndl.isValid()) {
		const UT_String styleSheet(materialStyleSheetHndl.get(primOffset), true);
		appendStyleSheet(primMaterial, styleSheet, t);
	}
	else if (materialPathHndl.isValid()) {
		const UT_String &matPath = materialPathHndl.get(primOffset);

		UT_String materialOverrides;
		if (materialOverrideHndl.isValid()) {
			materialOverrides = materialOverrideHndl.get(primOffset);
		}

		if (!matPath.equal("")) {
			appendMaterialOverrides(primMaterial, matPath, materialOverrides, t);
		}
	}
}

void VRayForHoudini::getOverridesForPrimitive(const STY_Styler &geoStyler, const GEO_Primitive &prim, PrimMaterial &primMaterial)
{
	const STY_Styler &primStyler = getStylerForPrimitive(geoStyler, prim);

	// If overrides comes from the stylesheet we have to override the values.
	// This is different from when the stylesheet is a primitive attribute, then
	// we'll append the values.
	appendOverrideValues(primStyler, primMaterial, overrideMerge);
}

STY_Styler VRayForHoudini::getStylerForPrimitive(const STY_Styler &geoStyler, const GEO_Primitive &prim)
{
#if UT_BUILD_VERSION_INT > 633
	const GSTY_SubjectPrim primSubject(&prim, nullptr);
#else
	const GSTY_SubjectPrim primSubject(&prim);
#endif
	return geoStyler.cloneWithSubject(primSubject);
}

STY_Styler VRayForHoudini::getStylerForObject(const STY_Styler &topStyler, const OP_Node &opNode)
{
	const UT_TagListPtr tags;
	const GSTY_BundleMap bundles;
	const UT_StringHolder stylesheet;

	const GSTY_SubjectGeoObject geoSubject(opNode.getFullPath(), tags, bundles, stylesheet);

	return topStyler.cloneWithSubject(geoSubject);
}

static STY_Styler cloneWithStyleSheet(const STY_Styler &topStyler, const char *styleSheetBuffer)
{
	if (!UTisstring(styleSheetBuffer))
		return topStyler;
	return topStyler.cloneWithAddedStyleSheet(new STY_StyleSheet(styleSheetBuffer, NULL, STY_LOAD_FOR_STYLING));
}

STY_Styler VRayForHoudini::getStylerForObject(OBJ_Node &objNode, fpreal t)
{
	STY_Styler styStyler;

	const UT_StringArray &globalStyles =
		OPgetDirector()->getStyleManager()->getStyleNames();

	for (const UT_StringHolder &globalStyle : globalStyles) {
		const UT_StringHolder &globalSheet =
			OPgetDirector()->getStyleManager()->getStyleSheet(globalStyle);

		styStyler = cloneWithStyleSheet(styStyler, globalSheet.buffer());
	}

	if (Parm::isParmExist(objNode, VFH_ATTR_SHOP_MATERIAL_STYLESHEET)) {
		UT_String objStyleSheet;
		objNode.evalString(objStyleSheet, VFH_ATTR_SHOP_MATERIAL_STYLESHEET, 0, t);

		styStyler = cloneWithStyleSheet(styStyler, objStyleSheet.buffer());
	}

	return styStyler;
}
