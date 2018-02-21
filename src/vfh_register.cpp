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
#include "vfh_vray_instances.h"
#include "vfh_rop.h"
#ifndef VASSERT_ENABLED
#include "vfh_error.h"
#endif

#include "rop/rop_vrayproxyrop.h"
#include "obj/obj_node_def.h"
#include "sop/sop_node_def.h"
#include "vop/vop_context.h"
#include "vop/vop_node_osl.h"
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
#include "gu_pgyeti.h"
#include "io/io_vrmesh.h"
#include "io/io_vrscene.h"

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


/// Register file extension that could be handled by vfh custom translators.
static void registerExtension(UT_ExtensionList &extList, const char *ext)
{
	if (!extList.findExtension(ext)) {
		extList.addExtension(ext);
	}
}

/// Register file extensions that could be handled by vfh custom translators.
static void registerExtensions()
{
	Log::Logger::startLogging();

	UT_ExtensionList *geoExtensions = UTgetGeoExtensions();
	if (!geoExtensions)
		return;

	registerExtension(*geoExtensions, IO::Vrmesh::extension);
	registerExtension(*geoExtensions, IO::Vrscene::fileExtension);
}


/// Called by Houdini to register vfh custom translators
void newGeometryIO(void *)
{
	GU_Detail::registerIOTranslator(new IO::Vrmesh());
	GU_Detail::registerIOTranslator(new IO::Vrscene());

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
#ifdef CGR_HAS_VRAYSCENE
	VRaySceneRef::install(gafactory);
#endif
	VRayProxyRef::install(gafactory);
	GeomPlaneRef::install(gafactory);
#ifdef CGR_HAS_AUR
	VRayVolumeGridRef::install(gafactory);
#endif
	VRayPgYetiRef::install(gafactory);
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

	VFH_ADD_SOP_GENERATOR_CUSTOM(table, PhxShaderCache, Parm::getPrmTemplate("PhxShaderCache", true));
#endif

#ifdef CGR_HAS_VRAYSCENE
	VFH_ADD_SOP_GENERATOR(table, VRayScene);
#endif
	VFH_ADD_SOP_GENERATOR(table, VRayPgYeti);

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

#if BRDF_COMPONENTS
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFBlinn);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFCookTorrance);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFDiffuse);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFFlakes);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFGGX);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFGlass);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFGlassGlossy);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFHair);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFHair2);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFMirror);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFPhong);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFSSS);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFSSS2);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFSampled);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFWard);
#endif
#if BRDF_EXPERIMENTAL
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFAlHair);
#endif

	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFBump);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFCarPaint);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFHair3);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFHair4);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFLayered);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFLight);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFSSS2Complex);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFScanned);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFSkinComplex);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFVRayMtl);
	VFH_VOP_ADD_OPERATOR(table, "BRDF", BRDFAlSurface);

#ifdef CGR_HAS_AUR
	VFH_VOP_ADD_OPERATOR_CUSTOM(table, "MATERIAL", PhxShaderSim, PhxShaderSim::GetPrmTemplate(), OP_FLAG_UNORDERED);
#endif

	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", Mtl2Sided);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlMaterialID);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlMulti);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlOSL);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlOverride);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlRenderStats);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlRoundEdges);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlSingleBRDF);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlVRmat);
	VFH_VOP_ADD_OPERATOR(table, "MATERIAL", MtlWrapper);

	VFH_VOP_ADD_OPERATOR(table, VRayPluginType::GEOMETRY, GeomDisplacedMesh);
	VFH_VOP_ADD_OPERATOR(table, VRayPluginType::GEOMETRY, GeomStaticSmoothedMesh);

	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", BitmapBuffer);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", ColorTextureToMono);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", FloatToTex);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexAColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexAColorChannel);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexAColorOp);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBezierCurve);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBitmap);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBlend);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBlendBumpNormal);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexBulge);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCellular);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexChecker);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexClamp);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColorAndAlpha);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColorToFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCombineColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCombineFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCompMax);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCondition2);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexCurvature);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexColorCorrect);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexDirt);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexDistance);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexDistanceBetween);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexEdges);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexFalloff);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexFloatOp);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexFresnel);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexGradRamp);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexGrid);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexHSVToRGB);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexHairSampler);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexICC);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexInt);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexIntToFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexInvert);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexInvertFloat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexLayeredMax);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexOSL);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexLuminance);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexLut);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMaskMax);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMeshVertexColorChannel);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMix);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexMulti);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexNoise);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexNormalMapFlip);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexOCIO);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexOutput);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexParticleId);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexParticleSampler);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexPtex);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRGBMultiplyMax);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRGBTintMax);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRGBToHSV);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRaySwitch);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRemap);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRgbaCombine);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexRock);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSampler);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSetRange);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSky);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSmoke);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSnow);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSoftbox);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSpeckle);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSplat);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexStencil);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexSwirl);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexTemperature);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexTemperatureToColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexThickness);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexTiles);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexUVW);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexUVWGenToTexture);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexUserColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexUserScalar);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexUserInteger);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexVectorOp);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexVectorProduct);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexVectorToColor);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexWater);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TransformToTex);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", TexTriPlanar);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", MetaImageFile);

	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenChannel);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenEnvironment);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenExplicit);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenMayaPlace2dTexture);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenObject);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenObjectBBox);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenPlanarWorld);
	VFH_VOP_ADD_OPERATOR(table, "UVWGEN", UVWGenProjection);

	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", ColorCorrect);
	VFH_VOP_ADD_OPERATOR(table, "TEXTURE", ColorCorrection);
}


/// Called by Houdini to register vfh custom hscript commands
/// @param cman[out] - Houdini's command manager
void CMDextendLibrary(CMD_Manager *cman)
{
	CMD::RegisterCommands(cman);
}
