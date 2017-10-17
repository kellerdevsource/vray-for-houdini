//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
#include "vfh_error.h"
#include "vfh_vray_instances.h"
#include "vfh_rop.h"

#include "rop/rop_vrayproxyrop.h"
#include "obj/obj_node_def.h"
#include "sop/sop_node_def.h"
#include "vop/vop_context.h"
#include "vop/material/vop_PhoenixSim.h"
#include "vop/brdf/vop_brdf_def.h"
#include "vop/material/vop_mtl_def.h"
#include "vop/texture/vop_texture_def.h"
#include "vop/uvwgen/vop_uvwgen_def.h"
#include "vop/meta/vop_meta_def.h"
#include "vop/rc/vop_rc_def.h"
#include "vop/env/vop_env_def.h"
#include "cmd/vfh_cmd_register.h"
#include "gu_volumegridref.h"
#include "gu_vrayproxyref.h"
#include "gu_vraysceneref.h"
#include "gu_geomplaneref.h"
#include "io/io_vrmesh.h"

// For newShopOperator()
#include <SHOP/SHOP_Node.h>
#include <SHOP/SHOP_Operator.h>

#ifndef MAKING_DSO
#  define MAKING_DSO
#endif
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Exit.h>
#include <UT/UT_IOTable.h>
#include <GU/GU_Detail.h>

#ifdef CGR_HAS_AUR
#  include <threads.h>
#  include <aurloader.h>
VUtils::ThreadManager * aurThreadManager = nullptr;
#endif

using namespace VRayForHoudini;

///@note This file contains all entry points of vfh DSO for
///      registering custom nodes, geometry and tools.
///      This also is the place for any future entry points you create.
///      The entry point is of the form newFooOperator()
///      where Foo is replaced with the network type (Sop, Obj, Dop, etc).
///      For more info see:
///      http://archive.sidefx.com/docs/hdk15.5/_h_d_k__intro__creating_plugins.html
///      http://archive.sidefx.com/docs/hdk15.5/_h_d_k__op_basics__overview__registration.html


/// Register file extensions that could be handled by vfh custom translators
static void registerExtensions()
{
	Log::Logger::startLogging();
	UT_ExtensionList *geoextension = UTgetGeoExtensions();
	if (geoextension && !geoextension->findExtension(IO::Vrmesh::extension)) {
		geoextension->addExtension(IO::Vrmesh::extension);
	}
}


/// Called by Houdini to register vfh custom translators
void newGeometryIO(void *)
{
	GU_Detail::registerIOTranslator(new IO::Vrmesh());

	// Note: due to the just-in-time loading of GeometryIO,
	// the .vrmesh extension won't be added until after your first .vrmesh save/load.
	// Thus this is replicated in the newDriverOperator.
	registerExtensions();
}


/// Called when Houdini exits, but only if ROP operators have been registered
void unregister(void *)
{
	deleteVRayInit();
	Log::Logger::stopLogging();

#ifdef CGR_HAS_AUR
	finalizeAuraLoader();
	destroyDefaultThreadManager(aurThreadManager);
#endif

#ifndef VASSERT_ENABLED
	Error::ErrorChaser &errChaser = Error::ErrorChaser::getInstance();
	errChaser.enable(false);
#endif
}


/// Called by Houdini to register vfh custom primitives
/// @param gafactory[out] - primitive factory for DSO defined primitives
void newGeometryPrim(GA_PrimitiveFactory *gafactory)
{
	VRaySceneRef::install(gafactory);
	VRayProxyRef::install(gafactory);
	GeomPlaneRef::install(gafactory);
#ifdef CGR_HAS_AUR
	VRayVolumeGridRef::install(gafactory);
#endif
}


/// Called by Houdini to register vfh custom ROP operators
/// @param table[out] - ROP operator table
void newDriverOperator(OP_OperatorTable *table)
{
	Log::getLog().info("Build %s from " __DATE__ ", " __TIME__,
					   STRINGIZE(CGR_GIT_HASH));
#ifndef VASSERT_ENABLED
	Error::ErrorChaser &errChaser = Error::ErrorChaser::getInstance();
	errChaser.enable(true);
#endif

	VRayRendererNode::register_operator(table);
	VRayProxyROP::register_ropoperator(table),

	VOP::VRayVOPContext::register_operator_vrayrccontext(table);
	VOP::VRayVOPContext::register_operator_vrayenvcontext(table);

	registerExtensions();

	UT_Exit::addExitCallback(unregister);
}


/// Called by Houdini to register vfh custom SOP operators
/// @param table[out] - SOP operator table
void newSopOperator(OP_OperatorTable *table)
{
	using namespace SOP;

#ifdef CGR_HAS_AUR
	const char *vfhPhoenixLoaderDir = getenv("VRAY_FOR_HOUDINI_AURA_LOADERS");
	if (vfhPhoenixLoaderDir && *vfhPhoenixLoaderDir) {
		Log::getLog().info("Loading Phoenix cache loader plugins from \"%s\"...",
						   vfhPhoenixLoaderDir);
		aurThreadManager = createDefaultThreadManager();
		if (!aurThreadManager) {
			Log::getLog().error("Failed to create thread manager for aurloader - loading will be single threaded!");
		}
		if (!initalizeAuraLoader(vfhPhoenixLoaderDir, "phx", 2, aurThreadManager)) {
			Log::getLog().error("Failed to load Phoenix cache loader plugins from \"%s\"!",
								vfhPhoenixLoaderDir);
		}
	}

	VFH_ADD_SOP_GENERATOR_CUSTOM(table, PhxShaderCache, PhxShaderCache::GetPrmTemplate());
#endif

#ifdef CGR_HAS_VRAYSCENE
	VFH_ADD_SOP_GENERATOR(table, VRayScene);
#endif

	VFH_ADD_SOP_GENERATOR(table, GeomPlane);
	VFH_ADD_SOP_GENERATOR_CUSTOM(table, VRayProxy, VRayProxy::getPrmTemplate());

	VRayProxyROP::register_sopoperator(table);
}


/// Called by Houdini to register vfh custom OBJ operators
/// @param table[out] - OBJ operator table
void newObjectOperator(OP_OperatorTable *table)
{
	using namespace OBJ;

	VFH_ADD_OBJ_OPERATOR(table, SunLight);
	VFH_ADD_OBJ_OPERATOR(table, LightDirect);
	VFH_ADD_OBJ_OPERATOR(table, LightAmbient);
	VFH_ADD_OBJ_OPERATOR(table, LightOmni);
	VFH_ADD_OBJ_OPERATOR(table, LightSphere);
	VFH_ADD_OBJ_OPERATOR(table, LightSpot);
	VFH_ADD_OBJ_OPERATOR(table, LightRectangle);
	VFH_ADD_OBJ_OPERATOR(table, LightMesh);
	VFH_ADD_OBJ_OPERATOR(table, LightIES);
	VFH_ADD_OBJ_OPERATOR(table, LightDome);
	VFH_ADD_OBJ_OPERATOR(table, VRayClipper);
}


/// Called by Houdini to register vfh custom SHOP operators
/// @param table[out] - SHOP operator table
void newShopOperator(OP_OperatorTable *table)
{
	using namespace VOP;
	VOP::VRayMaterialBuilder::register_shop_operator(table);
}


/// Called by Houdini to register vfh custom VOP operators
/// @param table[out] - VOP operator table
void newVopOperator(OP_OperatorTable *table)
{
	using namespace VOP;
	VOP::MaterialOutput::register_operator(table);

	VFH_VOP_ADD_OPERATOR(table, "SETTINGS", SettingsEnvironment);

	VFH_VOP_ADD_OPERATOR(table, "EFFECT", EnvironmentFog);
	VFH_VOP_ADD_OPERATOR(table, "EFFECT", EnvFogMeshGizmo);
	VFH_VOP_ADD_OPERATOR(table, "EFFECT", VolumeVRayToon);

	VFH_VOP_ADD_OPERATOR_CUSTOM(table, "RENDERCHANNEL", RenderChannelsContainer, Parm::getPrmTemplate("SettingsRenderChannels"), OP_FLAG_UNORDERED | OP_FLAG_OUTPUT);

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
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelDenoiser);
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelMultiMatte);
	VFH_VOP_ADD_OPERATOR(table, "RENDERCHANNEL", RenderChannelCryptomatte);

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
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFScanned);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFSkinComplex);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFVRayMtl);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFWard);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFAlSurface);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFAlHair);

#ifdef CGR_HAS_AUR
	VFH_VOP_ADD_OPERATOR_CUSTOM(table, "MATERIAL", PhxShaderSim, PhxShaderSim::GetPrmTemplate(), OP_FLAG_UNORDERED);
#endif

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

	VFH_VOP_ADD_OPERATOR(table, VRayPluginType::GEOMETRY, GeomDisplacedMesh);
	VFH_VOP_ADD_OPERATOR(table, VRayPluginType::GEOMETRY, GeomStaticSmoothedMesh);


	// TODO: need to enable this at some point.
	// TextureOutput is intended to serve same purposes as MaterialOutput for materials
	// but used for textures in this case
	// VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TextureOutput);
	VFH_VOP_ADD_OPERATOR_CUSTOM(table, "TEXTURE", MetaImageFile, VOP::MetaImageFile::GetPrmTemplate(), OP_FLAG_UNORDERED);

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
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColorCorrect);
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
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TransformToTex);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", texRenderHair);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexTriPlanar);

	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenBercon);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenChannel);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenEnvironment);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenExplicit);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenMayaPlace2dTexture);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenObject);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenObjectBBox);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenPlanarWorld);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenProjection);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenSwitch);

	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", ColorCorrect);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", ColorCorrection);
}


/// Called by Houdini to register vfh custom hscript commands
/// @param cman[out] - Houdini's command manager
void CMDextendLibrary(CMD_Manager *cman)
{
	CMD::RegisterCommands(cman);
}
