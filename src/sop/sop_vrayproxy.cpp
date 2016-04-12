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

	VRayProxyCacheMan &theCacheMan = GetVRayProxyCacheManager();
	if (theCacheMan.contains(filepath.buffer())) {
		VRayProxyCache &fileCache = theCacheMan[filepath.buffer()];
		fileCache.clearCache();
	}

	return 0;
}


SOP::VRayProxy::VRayProxy(OP_Network *parent, const char *name, OP_Operator *entry):
	NodeBase(parent, name, entry)
{
	// This indicates that this SOP manually manages its data IDs,
	// so that Houdini can identify what attributes may have changed,
	// e.g. to reduce work for the viewport, or other SOPs that
	// check whether data IDs have changed.
	// By default, (i.e. if this line weren't here), all data IDs
	// would be bumped after the SOP cook, to indicate that
	// everything might have changed.
	// If some data IDs don't get bumped properly, the viewport
	// may not update, or SOPs that check data IDs
	// may not cook correctly, so be *very* careful!
	mySopFlags.setManagesDataIDs(true);
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

	gdp->stashAll();

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

					// Set the options on the primitive
					UT_Options options;
					options.setOptionS("path", this->getFullPath())
							.setOptionS("file", path)
							.setOptionI("lod", evalInt("loadtype", 0, t))
							.setOptionF("frame", context.getFloatFrame())
							.setOptionI("anim_type", evalInt("anim_type", 0, t))
							.setOptionF("anim_offset", evalFloat("anim_offset", 0, t))
							.setOptionF("anim_speed", evalFloat("anim_speed", 0, t))
							.setOptionB("anim_override", evalInt("anim_override", 0, t))
							.setOptionI("anim_start", evalInt("anim_start", 0, t))
							.setOptionI("anim_length", evalInt("anim_length", 0, t))
							.setOptionF("scale", evalFloat("scale", 0, t))
							.setOptionI("flip_axis", evalInt("flip_axis", 0, t));

					pack->implementation()->update(options);
					pack->setViewportLOD(GEO_VIEWPORT_FULL);


				}
			}
			boss->opEnd();
		}
	}

	gdp->destroyStashed();

#if UT_MAJOR_VERSION_INT < 14
	gdp->notifyCache(GU_CACHE_ALL);
#endif

	return error();
}


OP::VRayNode::PluginResult SOP::VRayProxy::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	fpreal t = exporter.getContext().getTime();
	UT_String path;
	evalString(path, "file", 0, t);
	if (NOT(path.isstring())) {
		Log::getLog().error("VRayProxy \"%s\": \"File\" is not set!",
					getName().buffer());
		return OP::VRayNode::PluginResultError;
	}

	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("file", path.buffer()));
	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("flip_axis", evalInt("flip_axis", 0, t)));

	exporter.setAttrsFromOpNodePrms(pluginDesc, this);

	return OP::VRayNode::PluginResultSuccess;
}
