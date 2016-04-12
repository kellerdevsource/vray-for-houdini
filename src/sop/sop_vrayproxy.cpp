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
#include "vfh_prm_templates.h"
#include "gu_vrayproxyref.h"

#include <GU/GU_PrimPacked.h>
#include <EXPR/EXPR_Lock.h>


using namespace VRayForHoudini;


void SOP::VRayProxy::addPrmTemplate(Parm::PRMTmplList &prmTemplate)
{
	const char *lodItems[] = {
		"bbox", "Bounding Box",
		"preview", "Preview Geometry",
		"full", "Full Geometry",
	};

	const char *viewportlodItems[] = {
		GEOviewportLOD(GEO_VIEWPORT_FULL), GEOviewportLOD(GEO_VIEWPORT_FULL, true),
		GEOviewportLOD(GEO_VIEWPORT_POINTS), GEOviewportLOD(GEO_VIEWPORT_POINTS, true),
		GEOviewportLOD(GEO_VIEWPORT_BOX), GEOviewportLOD(GEO_VIEWPORT_BOX, true),
		GEOviewportLOD(GEO_VIEWPORT_CENTROID), GEOviewportLOD(GEO_VIEWPORT_CENTROID, true),
		GEOviewportLOD(GEO_VIEWPORT_HIDDEN), GEOviewportLOD(GEO_VIEWPORT_HIDDEN, true),
	};

	const char *missingfileItems[] = {
		"error", "Report Error",
		"empty", "No Geometry",
	};

	prmTemplate.push_back(Parm::PRMFactory(PRM_ORD, "loadtype", "Load")
						.setDefault(PRMoneDefaults)
						.setChoiceListItems(PRM_CHOICELIST_SINGLE, lodItems, CountOf(lodItems))
						.getPRMTemplate());
	prmTemplate.push_back(Parm::PRMFactory(PRM_ORD, "viewportlod", "Display As")
						.setDefault(PRMzeroDefaults)
						.setChoiceListItems(PRM_CHOICELIST_SINGLE, viewportlodItems, CountOf(viewportlodItems))
						.getPRMTemplate());
	prmTemplate.push_back(Parm::PRMFactory(PRM_ORD, "missingfile", "Missing File")
						.setDefault(PRMzeroDefaults)
						.setChoiceListItems(PRM_CHOICELIST_SINGLE, missingfileItems, CountOf(missingfileItems))
						.getPRMTemplate());
	prmTemplate.push_back(Parm::PRMFactory(PRM_CALLBACK, "reload", "Reload Geometry")
						.setCallbackFunc(VRayProxy::cbClearCache)
						.getPRMTemplate());
	prmTemplate.push_back(Parm::PRMFactory(PRM_HEADING, "vrayproxyheading", "VRayProxy Settings")
						.getPRMTemplate());
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

	gdp->stashAll();

	UT_String path;
	evalString(path, "file", 0, t);
	if (path.equal("")) {
		UT_String missingfile;
		evalString(missingfile, "missingfile", 0, t);
		if (missingfile == "error") {
			addError(SOP_ERR_FILEGEO, "Invalid file path!");
		}

		gdp->destroyStashed();
		return error();
	}

	if (error() < UT_ERROR_ABORT) {
		UT_Interrupt *boss = UTgetInterrupt();
		if (boss) {
			if(boss->opStart("Building V-Ray Scene Preview Mesh")) {
				// Create a packed sphere primitive
				GU_PrimPacked *pack = GU_PrimPacked::build(*gdp, "VRayProxyRef");
				if (NOT(pack)) {
					addWarning(SOP_MESSAGE, "Can't create packed primitive VRayProxyRef");
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

					UT_String viewportlod;
					evalString(viewportlod, "viewportlod", 0, t);
					pack->setViewportLOD(GEOviewportLOD(viewportlod));
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
