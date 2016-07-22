//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_class_utils.h"
#include "vfh_log.h"
#include "utils/vfh_error.h"

#include "vfh_rop.h"
#include "obj/obj_node_def.h"
#include "sop/sop_node_def.h"
#include "vop/vop_context.h"
#include "vop/material/vop_PhoenixSim.h"
#include "vop/brdf/vop_brdf_def.h"
#include "vop/brdf/vop_brdfvraymtl.h"
#include "vop/brdf/vop_brdfdiffuse.h"
#include "vop/material/vop_mtl_def.h"
#include "vop/texture/vop_texture_def.h"
#include "vop/uvwgen/vop_uvwgen_def.h"
#include "vop/meta/vop_meta_def.h"
#include "vop/rc/vop_rc_def.h"
#include "vop/env/vop_env_def.h"
#include "cmd/vfh_cmd_register.h"

#include "io/io_vrmesh.h"

// For newShopOperator()
#include <SHOP/SHOP_Node.h>
#include <SHOP/SHOP_Operator.h>

#include <UT/UT_DSOVersion.h>
#include <UT/UT_Exit.h>
#include <UT/UT_IOTable.h>

#ifdef CGR_HAS_AUR
#  include <aurloader.h>
#endif


using namespace VRayForHoudini;


static void registerExtensions()
{
	UT_ExtensionList *geoextension = UTgetGeoExtensions();
	if (geoextension && !geoextension->findExtension(IO::Vrmesh::extension)) {
		geoextension->addExtension(IO::Vrmesh::extension);
	}
}


void newGeometryIO(void *)
{
	GU_Detail::registerIOTranslator(new IO::Vrmesh());

	registerExtensions();
}


void unregister(void *)
{
	Error::ErrorChaser &errChaser = Error::getErrorChaser();
	errChaser.enable(false);
}


void newDriverOperator(OP_OperatorTable *table)
{
	Log::getLog().info("Build %s from " __DATE__ ", " __TIME__,
					   STRINGIZE(CGR_GIT_HASH));

	Error::ErrorChaser &errChaser = Error::getErrorChaser();
	errChaser.enable(true);

	VRayPluginRenderer::initialize();

	VRayRendererNode::register_operator(table);

	VOP::VRayVOPContext::register_operator_vrayrccontext(table);
	VOP::VRayVOPContext::register_operator_vrayenvcontext(table);

	registerExtensions();

	UT_Exit::addExitCallback(unregister);
}


void newSopOperator(OP_OperatorTable *table)
{
	using namespace SOP;
#ifdef CGR_HAS_AUR
	const char *vfhPhoenixLoaderDir = getenv("VRAY_FOR_HOUDINI_AURA_LOADERS");
	if (vfhPhoenixLoaderDir && *vfhPhoenixLoaderDir) {
		Log::getLog().info("Loading Phoenix cache loader plugins from \"%s\"...",
						   vfhPhoenixLoaderDir);
		if (!initalizeAuraLoader(vfhPhoenixLoaderDir, "vray", 2)) {
			Log::getLog().error("Failed to load Phoenix cache loader plugins from \"%s\"!",
								vfhPhoenixLoaderDir);
		}
	}

	VFH_SOP_ADD_OPERATOR_INPUTS(table, "GEOMETRY", PhxShaderCache, PhxShaderCache::GetPrmTemplate(), 0, 1);
#endif

	VFH_SOP_ADD_OPERATOR_AUTO(table, "GEOMETRY", GeomPlane);

	VFH_SOP_ADD_OPERATOR_AUTO_INPUTS(table, "GEOMETRY", GeomMayaHair, 1, 1);
#ifdef CGR_HAS_VRAYSCENE
	VFH_SOP_ADD_OPERATOR_AUTO(table,           "GEOMETRY", VRayScene);
#endif
	VFH_SOP_ADD_OPERATOR_CUSTOM_ID_AUTO(table, "GEOMETRY", VRayProxy, "GeomMeshFile");
}


void newObjectOperator(OP_OperatorTable *table)
{
	using namespace OBJ;
	VFH_OBJ_ADD_OPERATOR_AUTO(table, OBJ::VRayPluginType::Light, SunLight);
	VFH_OBJ_ADD_OPERATOR_AUTO(table, OBJ::VRayPluginType::Light, LightDirect);
	VFH_OBJ_ADD_OPERATOR_AUTO(table, OBJ::VRayPluginType::Light, LightAmbient);
	VFH_OBJ_ADD_OPERATOR_AUTO(table, OBJ::VRayPluginType::Light, LightOmni);
	VFH_OBJ_ADD_OPERATOR_AUTO(table, OBJ::VRayPluginType::Light, LightSphere);
	VFH_OBJ_ADD_OPERATOR_AUTO(table, OBJ::VRayPluginType::Light, LightSpot);
	VFH_OBJ_ADD_OPERATOR_AUTO(table, OBJ::VRayPluginType::Light, LightRectangle);
	VFH_OBJ_ADD_OPERATOR_AUTO(table, OBJ::VRayPluginType::Light, LightMesh);
	VFH_OBJ_ADD_OPERATOR_AUTO(table, OBJ::VRayPluginType::Light, LightIES);
	VFH_OBJ_ADD_OPERATOR_AUTO(table, OBJ::VRayPluginType::Light, LightDome);
	VFH_OBJ_ADD_OPERATOR_AUTO(table, OBJ::VRayPluginType::Geometry, VRayClipper);
}


void newShopOperator(OP_OperatorTable *table)
{
	using namespace VOP;
	VOP::VRayMaterialBuilder::register_shop_operator(table);
}


void newVopOperator(OP_OperatorTable *table)
{
	using namespace VOP;
	VOP::MaterialOutput::register_operator(table);

	VFH_VOP_ADD_OPERATOR(table, "SETTINGS", SettingsEnvironment);

	VFH_VOP_ADD_OPERATOR(table, "EFFECT", EnvironmentFog);
	VFH_VOP_ADD_OPERATOR(table, "EFFECT", EnvFogMeshGizmo);
	VFH_VOP_ADD_OPERATOR(table, "EFFECT", VolumeVRayToon);

	VFH_VOP_ADD_OPERATOR_OUTPUT_CUSTOM_ID(table, "RENDERCHANNEL", RenderChannelsContainer,
										  "SettingsRenderChannels");

	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelBumpNormals);
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelColor);
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelCoverage);
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelDRBucket);
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelExtraTex);
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelGlossiness);
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelNodeID);
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelNormals);
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelRenderID);
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelVelocity);
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelZDepth);

	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFBlinn);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFBump);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFCSV);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFCarPaint);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFCookTorrance);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFDiffuse);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFDiffuse_forSSS);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFFlakes);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFGGX);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFGlass);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFGlassGlossy);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFHOPS);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFHair);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFHair2);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFHair3);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFLayered);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFLight);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFMirror);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFMultiBump);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFPhong);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFSSS);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFSSS2);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFSSS2Complex);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFSampled);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFSkinComplex);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFVRayMtl);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFWard);

	VFH_VOP_ADD_OPERATOR_CUSTOM(table, "MATERIAL", PhxShaderSim, PhxShaderSim::GetPrmTemplate());

	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", Mtl2Sided);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlBump);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlDiffuse);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlDoubleSided);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlLayeredBRDF);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlMaterialID);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlMayaRamp);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlMulti);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlObjBBox);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlOverride);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlRenderStats);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlRoundEdges);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlSingleBRDF);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlStreakFade);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlVRmat);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlWrapper);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlWrapperMaya);

	VFH_VOP_ADD_OPERATOR(table, "GEOMETRY", GeomDisplacedMesh);
	VFH_VOP_ADD_OPERATOR(table, "GEOMETRY", GeomStaticSmoothedMesh);

	VFH_VOP_ADD_OPERATOR_CUSTOM(table, "TEXTURE", MetaImageFile, VOP::MetaImageFile::GetPrmTemplate());

	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TextureOutput);

	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", BitmapBuffer);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", ColorTextureToMono);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", FloatToTex);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", MayaProjectionTex);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", ParticleTex);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexAColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexAColorChannel);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexAColorOp);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBerconDistortion);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBerconGrad);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBerconNoise);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBerconTile);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBerconWood);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBezierCurve);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBifrostVVMix);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBillboardParticle);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBitmap);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBlend);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBlendBumpNormal);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBulge);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCellular);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexChecker);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexClamp);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCloth);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColor2Scalar);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColor8Mix);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColorAndAlpha);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColorAverage);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColorCurve);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColorExponential);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColorMathBasic);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColorSwitch);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColorToFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCombineColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCombineColorLightMtl);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCombineFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCompMax);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCondition);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCondition2);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCurvature);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCustomBitmap);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexDirt);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexDisplacacementRestrict);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexDistance);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexDistanceBetween);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexEdges);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexFalloff);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexFloatOp);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexFloatPerVertexHairSampler);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexFloatToColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexFresnel);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexGradRamp);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexGradient);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexGranite);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexGrid);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexHSVToRGB);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexHairRootSampler);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexHairSampler);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexICC);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexIDIntegerMap);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexInt);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexIntToFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexInterpLinear);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexInvert);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexInvertFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexLayered);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexLayeredMax);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexLeather);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexLuminance);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexLut);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMarble);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMarbleMax);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMaskMax);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMaxGamma);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMaxHairInfo);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMayaContrast);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMayaConversion);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMayaFluid);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMayaFluidCombined);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMayaFluidProcedural);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMayaFluidTransformed);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMayaHairColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMayaHairIncandescence);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMayaHairTransparency);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMeshVertexColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMeshVertexColorChannel);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMeshVertexColorWithDefault);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMix);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMotionOcclusion);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMulti);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMultiFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMultiProjection);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexNoise);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexNoiseMax);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexNoiseMaya);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexNormalMapFlip);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexOCIO);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexOutput);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexParticleDiffuse);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexParticleId);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexParticleSampler);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexParticleShape);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexPerVertexHairSampler);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexPlusMinusAverage);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexPtex);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRGBMultiplyMax);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRGBTintMax);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRGBToHSV);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRamp);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRaySwitch);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRemap);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRemapFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRgbaCombine);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRgbaSplit);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRock);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSampler);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexScalarCurve);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexScalarExponential);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexScalarHairRootSampler);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexScalarMathBasic);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSetRange);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSky);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSmoke);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSnow);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSoftbox);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSpeckle);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSplat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexStencil);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexStucco);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSurfIncidence);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSurfaceLuminance);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSwirl);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSwitch);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSwitchFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSwitchInt);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSwitchMatrix);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSwitchTransform);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexTemperature);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexTemperatureToColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexThickness);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexTiles);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexUVW);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexUVWGenToTexture);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexUserColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexUserScalar);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexVRayFurSampler);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexVectorOp);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexVectorProduct);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexVectorToColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexVertexColorDirect);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexVoxelData);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexWater);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexWood);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIBitmap);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSICell);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIColorBalance);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIColorCorrection);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIColorMix);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIFabric);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIFalloff);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIFlagstone);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIGradient);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIHLSAdjust);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIIntensity);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSILayered);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIMulti);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSINormalMap);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIRGBAKeyer);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIRipple);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIRock);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIScalar2Color);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIScalarInvert);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSISnow);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIVein);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIVertexColorLookup);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIWeightmapColorLookup);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIWeightmapLookup);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexXSIWood);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TransformToTex);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", texRenderHair);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", texXSIColor2Alpha);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", texXSIColor2Vector);

	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenBercon);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenC4D);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenChannel);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenEnvironment);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenExplicit);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenMayaPlace2dTexture);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenObject);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenObjectBBox);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenPlanarWorld);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenProjection);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenSwitch);
}


void CMDextendLibrary(CMD_Manager *cman)
{
	CMD::RegisterCommands(cman);
}
