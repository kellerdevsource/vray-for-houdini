//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "sop_vrayproxy.h"
#include "vfh_log.h"
#include "gu_vrayproxyref.h"

//#include "vfh_hashes.h" // For MurmurHash3_x86_32
//#include "vfh_lru_cache.hpp"

//#include <GEO/GEO_Point.h>
//#include <GU/GU_PrimPoly.h>
//#include <GU/GU_PackedGeometry.h>
//#include <HOM/HOM_Vector2.h>
//#include <HOM/HOM_BaseKeyframe.h>
//#include <HOM/HOM_playbar.h>
//#include <HOM/HOM_Module.h>

#include <GU/GU_PrimPacked.h>
#include <EXPR/EXPR_Lock.h>


using namespace VRayForHoudini;


/// VRayProxy node params
///
static PRM_Name prmCacheHeading("cacheheading", "VRayProxy Cache");
static PRM_Name prmClearCache("clear_cache", "Clear Cache");

static PRM_Name prmLoadType("loadtype", "Load");
static PRM_Name prmLoadTypeItems[] = {
	PRM_Name("Bounding Box"),
	PRM_Name("Preview Geometry"),
	PRM_Name("Full Geometry"),
	PRM_Name(),
};
static PRM_ChoiceList prmLoadTypeMenu(PRM_CHOICELIST_SINGLE, prmLoadTypeItems);

static PRM_Name prmProxyHeading("vrayproxyheading", "VRayProxy Settings");



void SOP::VRayProxy::addPrmTemplate(Parm::PRMTmplList &prmTemplate)
{
	prmTemplate.push_back(PRM_Template(PRM_HEADING, 1, &prmCacheHeading));
	prmTemplate.push_back(PRM_Template(PRM_ORD, 1, &prmLoadType, PRMoneDefaults, &prmLoadTypeMenu));
	prmTemplate.push_back(PRM_Template(PRM_CALLBACK, 1, &prmClearCache, 0, 0, 0, VRayProxy::cbClearCache));
	prmTemplate.push_back(PRM_Template(PRM_HEADING, 1, &prmProxyHeading));
}


int SOP::VRayProxy::cbClearCache(void *data, int /*index*/, float t, const PRM_Template */*tplate*/)
{
	OP_Node *node = reinterpret_cast<OP_Node *>(data);
	UT_String filepath;
	{
		EXPR_GlobalStaticLock::Scope scopedLock;
		node->evalString(filepath, "file", 0, t);
	}

//	if (g_cacheMan.contains(filepath.buffer())) {
//		VRayProxyCache &fileCache = g_cacheMan[filepath.buffer()];
//		fileCache.clearCache();
//	}

	return 0;
}


void SOP::VRayProxy::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "GeomMeshFile";
}


OP_NodeFlags &SOP::VRayProxy::flags()
{
	OP_NodeFlags &flags = SOP_Node::flags();

	const auto animType = static_cast<VUtils::MeshFileAnimType::Enum>(evalInt("anim_type", 0, 0.0f));
	const bool is_animated = (animType != VUtils::MeshFileAnimType::Still);
	flags.setTimeDep(is_animated);

	return flags;
}


OP_ERROR SOP::VRayProxy::cookMySop(OP_Context &context)
{
	if (NOT(gdp)) {
		addError(SOP_MESSAGE, "Invalid geometry detail.");
		return error();
	}

	const float t = context.getTime();

	UT_String path;
	evalString(path, "file", 0, t);
	if (path.equal("")) {
		addError(SOP_ERR_FILEGEO, "Invalid file path!");
		return error();
	}

	gdp->clearAndDestroy();

	if (error() < UT_ERROR_ABORT) {
		UT_Interrupt *boss = UTgetInterrupt();
		if (boss) {
			if(boss->opStart("Building V-Ray Scene Preview Mesh")) {
				// Create a packed sphere primitive
				GU_PrimPacked *pack = GU_PrimPacked::build(*gdp, "VRayProxyRef");
				if (NOT(pack)) {
					addWarning(SOP_MESSAGE, "Can't create a packed VRayProxyRef");
				}
				else {
					// Set the location of the packed primitive's point.
					UT_Vector3 pivot(0, 0, 0);
					pack->setPivot(pivot);
					gdp->setPos3(pack->getPointOffset(0), pivot);
					// Set the options on the sphere primitive
					UT_Options options;
					options.setOptionS("file", path)
							.setOptionI("lod", 1)
							.setOptionF("frame", context.getFloatFrame())
							.setOptionI("anim_type", evalInt("anim_type", 0, t))
							.setOptionF("anim_offset", evalFloat("anim_offset", 0, t))
							.setOptionF("anim_speed", evalFloat("anim_speed", 0, t))
							.setOptionB("anim_override", evalInt("anim_override", 0, t))
							.setOptionI("anim_start", evalInt("anim_start", 0, t))
							.setOptionI("anim_length", evalInt("anim_length", 0, t));

					pack->implementation()->update(options);
					pack->setViewportLOD(GEO_VIEWPORT_FULL);
				}
			}
			boss->opEnd();
		}
	}


//	std::string filepath(path.buffer());
//	int inCache = g_cacheMan.contains(filepath);
//	VRayProxyCache &fileCache = g_cacheMan[filepath];
//	if (NOT(inCache)) {
//		VUtils::ErrorCode errCode = fileCache.init(path.buffer());
//		if (errCode.error()) {
//			g_cacheMan.erase(filepath);
//			addError(SOP_ERR_FILEGEO, errCode.getErrorString().ptr());
//			return error();
//		}
//	}

//	const bool flipAxis = (evalInt("flip_axis", 0, 0.0f) != 0);
//	const float scale   = evalFloat("scale", 0, 0.0f);

//	gdp->clearAndDestroy();

//	if (error() < UT_ERROR_ABORT) {
//		UT_Interrupt *boss = UTgetInterrupt();
//		if (boss) {
//			if(boss->opStart("Building V-Ray Scene Preview Mesh")) {
//				VUtils::ErrorCode errCode = fileCache.getFrame(context, *this, *gdp);
//				if (errCode.error()) {
//					addWarning(SOP_MESSAGE, errCode.getErrorString().ptr());
//				}

//	//			scale & flip axis
//				UT_Matrix4 mat(1.f);
//				mat(0,0) = scale;
//				mat(1,1) = scale;
//				mat(2,2) = scale;
//	//			houdini uses row major matrix
//				if (flipAxis) {
//					VUtils::swap(mat(1,0), mat(2,0));
//					VUtils::swap(mat(1,1), mat(2,1));
//					VUtils::swap(mat(1,2), mat(2,2));
//					mat(2,0) = -mat(2,0);
//					mat(2,1) = -mat(2,1);
//					mat(2,2) = -mat(2,2);
//				}
//				gdp->transform(mat, 0, 0, true, true, true, true, false, 0, true);
//			}

//			boss->opEnd();
//		}
//	}

//#if UT_MAJOR_VERSION_INT < 14
//	gdp->notifyCache(GU_CACHE_ALL);
//#endif

	return error();
}


OP::VRayNode::PluginResult SOP::VRayProxy::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	UT_ASSERT( exporter );

	UT_String path;
	evalString(path, "file", 0, 0.0f);
	if (NOT(path.isstring())) {
		Log::getLog().error("VRayProxy \"%s\": \"File\" is not set!",
					getName().buffer());
		return OP::VRayNode::PluginResultError;
	}

	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);

	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("file", path.buffer()));
	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("flip_axis", evalInt("flip_axis", 0, 0.0f)));

	exporter.setAttrsFromOpNodePrms(pluginDesc, this);

	return OP::VRayNode::PluginResultSuccess;
}
