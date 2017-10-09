//
// Copyright (c) 2015-2017, Chaos Software Ltd
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

PRM_Template* SOP::VRayProxy::getPrmTemplate()
{
	PRM_Template *prmTemplate = Parm::getPrmTemplate("GeomMeshFile");
	while (prmTemplate && prmTemplate->getType() != PRM_LIST_TERMINATOR) {
		if (vutils_strcmp(prmTemplate->getToken(), "reload") == 0) {
			prmTemplate->setCallback(cbClearCache);
			break;
		}
		prmTemplate++;
	}
	return prmTemplate;
}

int SOP::VRayProxy::cbClearCache(void *data, int /*index*/, fpreal t, const PRM_Template* /*tplate*/)
{
	OP_Node *node = reinterpret_cast<OP_Node*>(data);

	UT_String filepath;
	node->evalString(filepath, "file", 0, t);

	ClearVRayProxyCache(filepath);

	return 0;
}

SOP::VRayProxy::VRayProxy(OP_Network *parent, const char *name, OP_Operator *entry)
	: NodeBase(parent, name, entry)
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
	pluginType = VRayPluginType::GEOMETRY;
	pluginID   = "GeomMeshFile";
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
		const auto animType = static_cast<VUtils::MeshFileAnimType::Enum>(evalInt("anim_type", 0, t));
		const bool is_animated = (animType != VUtils::MeshFileAnimType::Still);
		flags().setTimeDep(is_animated);

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
							.setOptionS("file", path)
							.setOptionI("anim_type", evalInt("anim_type", 0, t))
							.setOptionF("anim_offset", evalFloat("anim_offset", 0, t) + context.getFrame())
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

	// Set the node selection for this primitive. This will highlight all
	// primitives generated by the node, but only if the highlight flag for this
	// node is on and the node is selected.
	select(GA_GROUP_PRIMITIVE);

	return error();
}
