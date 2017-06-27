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

using namespace VRayForHoudini;

PrimMaterial VRayForHoudini::processStyleSheet(const QString &styleSheet, fpreal t)
{
	QJsonParseError parserError;
	QJsonDocument styleSheetParser = QJsonDocument::fromJson(styleSheet.toUtf8(), &parserError);

	PrimMaterial primMaterial;

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
						if (matNode) {
							primMaterial.matNode = matNode;

							if (overrides.contains("materialParameters")) {
								QJsonObject materialParameters = overrides["materialParameters"].toObject();

								QStringList keys = materialParameters.keys();
								for (const QString &paramName : keys) {
									const tchar *parmName = paramName.toLocal8Bit().constData();
									QJsonValue paramJsonValue = materialParameters[paramName];

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

	return primMaterial;
}

PrimMaterial VRayForHoudini::processMaterialOverrides(const UT_String &matPath, const UT_String &materialOverrides, fpreal t)
{
	PrimMaterial primMaterial;
	primMaterial.matNode = getOpNodeFromPath(matPath, t);
	
	if (primMaterial.matNode) { 
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

					if (keyParmType.isFloatType()) {
						MtlOverrideItem &overrideItem = primMaterial.overrides[keyParm->getToken()];
					
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

	return primMaterial;
}
