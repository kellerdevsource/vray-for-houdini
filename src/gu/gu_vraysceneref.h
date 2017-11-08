//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAYSCENEREF_H
#define VRAY_FOR_HOUDINI_VRAYSCENEREF_H

#include "vfh_primitives.h"
#include "vfh_includes.h"

#include <GU/GU_PackedImpl.h>

namespace VUtils {
namespace Vrscene {
namespace Preview {
struct VrsceneDesc;
} // Preview
} // Vrscene
}

namespace VRayForHoudini {

enum FlipAxisMode {
	none = 0,  ///< No flipping
	automatic, ///< Gets the flipping from the vrscene description
	flipZY     ///< Force the scene to flip the Z and Y axis
};

#define VFH_VRAY_SCENE_PARAMS_COUNT 50
#define VFH_VRAY_SCENE_PARAMS (\
	(const char *, filepath, ""),\
	(bool,   add_nodes,     true),\
	(bool,   add_lights,    true),\
	(bool,   anim_override, false),\
	(fpreal, anim_speed,  1),\
	(exint,  anim_type,   0),\
	(fpreal, anim_offset, 0),\
	(exint,  anim_start,  0),\
	(exint,  anim_length, 0),\
	(const char *, material_override, ""),\
	(bool,   disable, false),\
	(exint,  objectID, -1),\
	(bool,   mw_use,  false),\
	(bool,   mw_use_irrad_map, true),\
	(fpreal, mw_generate_gi, 1),\
	(fpreal, mw_receive_gi,  1),\
	(fpreal, mw_generate_caustics,  1),\
	(fpreal, mw_receive_caustics,   1),\
	(fpreal, mw_alpha_contribution, 1),\
	(bool,   mw_matte_surface, false),\
	(bool,   mw_shadows,       false),\
	(bool,   mw_affect_alpha,  false),\
	(fpreal, mw_shadow_brightness, 1),\
	(fpreal, mw_reflectio_amount,  1),\
	(fpreal, mw_refraction_amount, 1),\
	(fpreal, mw_gi_amount, 1),\
	(bool,   mw_no_gi_on_other_mattes, true),\
	(bool,   mw_matte_for_secondary_rays, 0),\
	(exint,  mw_gi_surface_id, 0),\
	(fpreal, mw_gi_quality_multiplier, 1),\
	(bool,   mw_maya_background_shader_compability, false),\
	(exint,  mw_trace_depth, -1),\
	(bool,   mw_generate_render_elements,     true),\
	(bool,   mw_reflection_list_is_inclusive, false),\
	(bool,   mw_refraction_list_is_inclusive, false),\
	(bool,   mrs_use,                    false),\
	(bool,   mrs_camera_visibility,      true),\
	(bool,   mrs_reflections_visibility, true),\
	(bool,   mrs_refraction_visibility,  true),\
	(bool,   mrs_gi_visibility,          true),\
	(bool,   mrs_shadows_visibility,     true),\
	(bool,   mrs_shadows_receive,        true),\
	(fpreal, mrs_visibility, 1),\
	(const char *, flip_axis, 0),\
	(bool,   use_overrides, false),\
	(const char *, plugin_mapping, ""),\
	(const char *, override_snippet,  ""),\
	(const char *, override_filepath, ""),\
	(fpreal, current_frame, 0),\
	(bool, should_flip, false)\
)

#define VFH_VRAY_SCENE_PARAMS_TUPLES_COUNT 4
#define VFH_VRAY_SCENE_PARAMS_TUPLES (\
	(UT_StringArray, hidden_objects, UT_StringArray()),\
	(UT_StringArray, mw_channels,    UT_StringArray()),\
	(UT_StringArray, mw_reflection_exclude, UT_StringArray()),\
	(UT_StringArray, mw_refraction_exclude, UT_StringArray())\
)

/// VRayScene preview mesh implemented as a packed primitive.
class VRaySceneRef
	: public GU_PackedImpl
{
public:
	static GA_PrimitiveTypeId typeId();
	static void install(GA_PrimitiveFactory *gafactory);

	VRaySceneRef();
	VRaySceneRef(const VRaySceneRef &src);
	~VRaySceneRef();

	GU_PackedFactory *getFactory() const VRAY_OVERRIDE;
	GU_PackedImpl *copy() const VRAY_OVERRIDE;

	bool isValid() const VRAY_OVERRIDE;
	void clearData() VRAY_OVERRIDE;
	bool save(UT_Options &options, const GA_SaveMap &map) const VRAY_OVERRIDE;
#if HDK_16_5
	void update(GU_PrimPacked *prim, const UT_Options &options) VRAY_OVERRIDE { updateFrom(options); }
	bool load(GU_PrimPacked *prim, const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE { return updateFrom(options); }
#else
	bool load(const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE { return updateFrom(options); }
	void update(const UT_Options &options) VRAY_OVERRIDE { updateFrom(options); }
#endif
	bool getBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;
	bool getRenderingBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;
	void getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const VRAY_OVERRIDE;
	void getWidthRange(fpreal &wmin, fpreal &wmax) const VRAY_OVERRIDE;
	bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;
	GU_ConstDetailHandle getPackedDetail(GU_PackedContext *context = 0) const VRAY_OVERRIDE;
	int64 getMemoryUsage(bool inclusive) const VRAY_OVERRIDE;
	void countMemory(UT_MemoryCounter &counter, bool inclusive) const VRAY_OVERRIDE;

	VFH_MAKE_ACCESSORS(VFH_VRAY_SCENE_PARAMS, VFH_VRAY_SCENE_PARAMS_COUNT);
	VFH_MAKE_ACCESSORS_TUPLE(VFH_VRAY_SCENE_PARAMS_TUPLES, VFH_VRAY_SCENE_PARAMS_TUPLES_COUNT);

	UT_Vector3D get_mw_shadow_tint_color() const;
	void _get_mw_shadow_tint_color(fpreal64 * v, exint size) const;
	exint get_mw_shadow_tint_color_size() const;
	void set_mw_shadow_tint_color(const UT_Vector3D & val);
	void _set_mw_shadow_tint_color(const fpreal64 * v, exint size);

	/// Returns current options.
	const UT_Options &getOptions() const { return m_options; }

	/// Returns mesh sample time based on animation overrides settings.
	/// @param t Current time.
	double getFrame(fpreal t) const;

private:
	/// Build packed detail.
	/// @param vrsceneDesc *.vrscene file preview data.
	/// @param flipAxis Flip axis Z-Y.
	void detailBuild(VUtils::Vrscene::Preview::VrsceneDesc *vrsceneDesc, int flipAxis);

	/// Clear detail.
	void detailClear();

	/// Update detail from options.
	/// @param options New options set.
	/// @returns True if detail was changed, false - otherwise.
	int updateFrom(const UT_Options &options);

	/// Preview mesh bbox.
	UT_BoundingBox m_bbox;

	/// Geometry detail.
	GU_ConstDetailHandle m_detail;

	/// Current options set.
	UT_Options m_options;
};

} // namespace VRayForHoudini

#endif // !VRAY_FOR_HOUDINI_VRAYSCENEREF_H
