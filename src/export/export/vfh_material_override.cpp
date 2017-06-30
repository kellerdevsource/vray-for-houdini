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

#include "vfh_material_override.h"

#include <SHOP/SHOP_GeoOverride.h>
#include <UT/UT_Version.h>

#ifdef USE_QT5
#include <QtCore>
#endif

using namespace VRayForHoudini;

void VRayForHoudini::mergeStyleSheet(PrimMaterial &primMaterial, const QString &styleSheet, fpreal t, int materialOnly)
{
#ifdef USE_QT5
	QJsonParseError parserError;
	QJsonDocument styleSheetParser = QJsonDocument::fromJson(styleSheet.toUtf8(), &parserError);

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
	QJsonObject jsonObject = styleSheetParser.object();
	if (jsonObject.contains("styles")) {
		QJsonArray styles = jsonObject["styles"].toArray();

		for (const QJsonValue &style : styles) {
			QJsonObject obj = style.toObject();

			if (obj.contains("overrides")) {
				QJsonObject overrides = obj["overrides"].toObject();

				if (overrides.contains("material")) {
					QJsonObject material = overrides["material"].toObject();

					if (material.contains("name")) {
						const UT_String &matPath = material["name"].toString().toLocal8Bit().constData();
						OP_Node *matNode = getOpNodeFromPath(matPath, t);
						if (materialOnly) {
							break;
						}
						if (matNode) {
							// There is already some top-level material assigned.
							if (!primMaterial.matNode) {
								primMaterial.matNode = matNode;
							}

							if (overrides.contains("materialParameters")) {
								QJsonObject materialParameters = overrides["materialParameters"].toObject();

								QStringList keys = materialParameters.keys();
								for (const QString &paramName : keys) {
									const char *parmName = paramName.toLocal8Bit().constData();
									QJsonValue paramJsonValue = materialParameters[paramName];

									MtlOverrideItems::iterator moIt = primMaterial.overrides.find(parmName);
									if (moIt != primMaterial.overrides.end()) {
										// There is already some top-level override for this parameter.
										continue;
									}

									// NOTES:
									//  * Assuming current array value is a color
									//  * Only first 3 components will be used for vectors
									if (paramJsonValue.isArray()) {
										QJsonArray paramJsonVector = paramJsonValue.toArray();
										if (paramJsonVector.size() >= 3) {
											MtlOverrideItem &overrideItem = primMaterial.overrides[parmName];
											overrideItem.setType(MtlOverrideItem::itemTypeVector);
											overrideItem.valueVector.set(paramJsonVector.takeAt(0).toDouble(),
																			paramJsonVector.takeAt(1).toDouble(),
																			paramJsonVector.takeAt(2).toDouble());
										}
										else {
											// TODO: Implement other cases / print warning.
										}
									}
									else if (paramJsonValue.isDouble()) {
										MtlOverrideItem &overrideItem = primMaterial.overrides[parmName];
										overrideItem.setType(MtlOverrideItem::itemTypeDouble);
										overrideItem.valueDouble = paramJsonValue.toDouble();
									}
									else if (paramJsonValue.isString()) {
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
			}
		}
	}
#endif
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
