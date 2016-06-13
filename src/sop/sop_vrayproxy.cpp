//
// Copyright (c) 2015-2016, Chaos Software Ltd
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


int SOP::VRayProxy::cbClearCache(void *data, int /*index*/, float t, const PRM_Template* /*tplate*/)
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
				// Create a packed primitive
				GU_PrimPacked *pack = GU_PrimPacked::build(*gdp, "VRayProxyRef");
				if (NOT(pack)) {
					addWarning(SOP_MESSAGE, "Can't create packed primitive VRayProxyRef");
				}
				else {
					// Set the location of the packed primitive's point.
					UT_Vector3 pivot(0, 0, 0);
					pack->setPivot(pivot);
					gdp->setPos3(pack->getPointOffset(0), pivot);

					UT_String viewportlod;
					evalString(viewportlod, "viewportlod", 0, t);
					pack->setViewportLOD(GEOviewportLOD(viewportlod));

					UT_String objectPath;
					evalString(objectPath, "object_path", 0, t);

					UT_String velocityColorSet;
					evalString(velocityColorSet, "velocity_color_set", 0, t);

					// Set the options on the primitive
					UT_Options options;
					options.setOptionI("lod", evalInt("loadtype", 0, t))
							.setOptionF("frame", context.getFloatFrame())
							.setOptionS("file", path)
							.setOptionI("anim_type", evalInt("anim_type", 0, t))
							.setOptionF("anim_offset", evalFloat("anim_offset", 0, t))
							.setOptionF("anim_speed", evalFloat("anim_speed", 0, t))
							.setOptionB("anim_override", evalInt("anim_override", 0, t))
							.setOptionI("anim_start", evalInt("anim_start", 0, t))
							.setOptionI("anim_length", evalInt("anim_length", 0, t))
							.setOptionB("compute_bbox", evalInt("compute_bbox", 0, t))
							.setOptionB("compute_normals", evalInt("compute_normals", 0, t))
							.setOptionI("first_map_channel", evalInt("first_map_channel", 0, t))
							.setOptionI("flip_axis", evalInt("flip_axis", 0, t))
							.setOptionB("flip_normals", evalInt("flip_normals", 0, t))
							.setOptionI("hair_visibility_lists_type", evalInt("hair_visibility_lists_type", 0, t))
							.setOptionF("hair_width_multiplier", evalFloat("hair_width_multiplier", 0, t))
							.setOptionB("instancing", evalInt("instancing", 0, t))
							.setOptionI("num_preview_faces", evalInt("num_preview_faces", 0, t))
							.setOptionS("object_path", objectPath)
							.setOptionI("particle_render_mode", evalInt("particle_render_mode", 0, t))
							.setOptionI("particle_visibility_lists_type", evalInt("particle_visibility_lists_type", 0, t))
							.setOptionF("particle_width_multiplier", evalFloat("particle_width_multiplier", 0, t))
							.setOptionF("point_cloud_mult", evalFloat("point_cloud_mult", 0, t))
							.setOptionB("primary_visibility", evalInt("primary_visibility", 0, t))
							.setOptionF("scale", evalFloat("scale", 0, t))
							.setOptionF("smooth_angle", evalFloat("smooth_angle", 0, t))
							.setOptionB("smooth_uv", evalInt("smooth_uv", 0, t))
							.setOptionB("smooth_uv_borders", evalInt("smooth_uv_borders", 0, t))
							.setOptionI("sort_voxels", evalInt("sort_voxels", 0, t))
							.setOptionB("subdiv_all_meshes", evalInt("subdiv_all_meshes", 0, t))
							.setOptionI("subdiv_level", evalInt("subdiv_level", 0, t))
							.setOptionB("subdiv_preserve_geom_borders", evalInt("subdiv_preserve_geom_borders", 0, t))
							.setOptionB("subdiv_preserve_map_borders", evalInt("subdiv_preserve_map_borders", 0, t))
							.setOptionI("subdiv_type", evalInt("subdiv_type", 0, t))
							.setOptionB("subdiv_uvs", evalInt("subdiv_uvs", 0, t))
							.setOptionB("use_alembic_offset", evalInt("use_alembic_offset", 0, t))
							.setOptionB("use_face_sets", evalInt("use_face_sets", 0, t))
							.setOptionB("use_full_names", evalInt("use_full_names", 0, t))
							.setOptionB("use_point_cloud", evalInt("use_point_cloud", 0, t))
							.setOptionS("velocity_color_set", velocityColorSet)
							.setOptionF("velocity_multiplier", evalFloat("velocity_multiplier", 0, t))
							.setOptionI("visibility_lists_type", evalInt("visibility_lists_type", 0, t))
							;

					pack->implementation()->update(options);
					pack->setPathAttribute(getFullPath());
				}
			}
			boss->opEnd();
		}
	}

	gdp->destroyStashed();

#if UT_MAJOR_VERSION_INT < 14
	gdp->notifyCache(GU_CACHE_ALL);
#endif

	// Set the node selection for this primitive. This will highlight all
	// primitives generated by the node, but only if the highlight flag for this
	// node is on and the node is selected.
	select(GA_GROUP_PRIMITIVE);

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

	pluginDesc.pluginID   = pluginID;
	pluginDesc.pluginName = VRayExporter::getPluginName(this);

	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("file", path.buffer()));
	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("flip_axis", evalInt("flip_axis", 0, 0.0f)));

	exporter.setAttrsFromOpNodePrms(pluginDesc, this);

	return OP::VRayNode::PluginResultSuccess;
}
