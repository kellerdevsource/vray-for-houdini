//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAYSCENEREF_H
#define VRAY_FOR_HOUDINI_VRAYSCENEREF_H

#include <vrscene_preview.h>

#include "vfh_vray.h"
#include "vfh_primitives.h"
#include "GU/GU_PackedImpl.h"

namespace VRayForHoudini {

#define VFH_VRAY_SCENE_PARAMS_COUNT 48
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
	(exint,  flip_axis,      0),\
	(bool,   use_overrides, false),\
	(const char *, override_snippet,  ""),\
	(const char *, override_filepath, ""),\
	(fpreal, current_frame, 0)\
)

#define VFH_VRAY_SCENE_PARAMS_TUPLES_COUNT 4
#define VFH_VRAY_SCENE_PARAMS_TUPLES (\
	(UT_StringArray, hidden_objects, UT_StringArray()),\
	(UT_StringArray, mw_channels,    UT_StringArray()),\
	(UT_StringArray, mw_reflection_exclude, UT_StringArray()),\
	(UT_StringArray, mw_refraction_exclude, UT_StringArray())\
)

// Represents VRayScene file
// Implemented as a packed primitive
class VRaySceneRef :
	public GU_PackedImpl
{
public:
	static const char *mw_shadow_tint_color_param_name;

	/// Get the type ID for the VRaySceneRef primitive.
	static GA_PrimitiveTypeId typeId() { return theTypeId; }

	/// Register the primitive with gafactory
	/// @param gafactory[out] - Houdini primitive factory for custom plugin primitives
	static void install(GA_PrimitiveFactory *gafactory);

	/// vrscene manager
	static VUtils::Vrscene::Preview::VrsceneDescManager vrsceneMan; 

private:
	/// type id for this primitive
	/// custom primitive types receive their type id at runtime
	/// when registering the primitive as opposed to houdini native primitives
	/// which are known at compile time
	static GA_PrimitiveTypeId theTypeId;

public:
	VRaySceneRef();
	VRaySceneRef(const VRaySceneRef &src);
	virtual ~VRaySceneRef();

public:
	/// Get the factory associated with this procedural
	virtual GU_PackedFactory *getFactory() const VRAY_OVERRIDE;

	/// Create a copy of this resolver
	virtual GU_PackedImpl *copy() const VRAY_OVERRIDE;

	/// Test whether the deferred load primitive data is valid
	virtual bool isValid() const VRAY_OVERRIDE;

	/// The clearData() method is called when the primitives are stashed during
	/// the cook of a SOP.  See GA_Primitive::stashed().  This gives the
	/// primitive to optionally clear some data during the stashing process.
	virtual void clearData() VRAY_OVERRIDE;

	/// Give a UT_Options of load data, create resolver data for the primitive
	virtual bool load(const UT_Options &options, const GA_LoadMap &map) VRAY_OVERRIDE;
	
	/// Depending on the update, the procedural should call one of:
	///	- transformDirty()
	///	- attributeDirty()
	///	- topologyDirty()
	virtual void update(const UT_Options &options) VRAY_OVERRIDE;

	/// Copy the resolver data into the UT_Options for saving
	virtual bool save(UT_Options &options, const GA_SaveMap &map) const VRAY_OVERRIDE;

	/// Get the bounding box for the geometry (not including transforms)
	virtual bool getBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;

	/// Get the rendering bounding box for the geometry (not including
	/// transforms).  For curve and point geometry, this needs to include any
	/// "width" attributes.
	virtual bool getRenderingBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;

	/// When rendering with velocity blur, the renderer needs to know the
	/// bounds on velocity to accurately compute the bounding box.
	virtual void getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const VRAY_OVERRIDE;

	/// When rendering points or curves, the renderer needs to know the bounds
	/// on the width attribute to accurately compute the bounding box.
	virtual void getWidthRange(fpreal &wmin, fpreal &wmax) const VRAY_OVERRIDE;

	virtual bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;

	/// Get a reference to a const GU_Detail for the packed geometry to display in vewport.
	/// @param context[in/out] - read/write primitive shared data if necessary
	virtual GU_ConstDetailHandle getPackedDetail(GU_PackedContext *context = 0) const VRAY_OVERRIDE;

	/// Report memory usage (includes all shared memory)
	virtual int64 getMemoryUsage(bool inclusive) const VRAY_OVERRIDE;

	/// Count memory usage using a UT_MemoryCounter in order to count
	/// shared memory correctly.
	/// NOTE: There's nothing outside of sizeof(*this) to count in the
	///       base class, so it can be pure virtual.
	virtual void countMemory(UT_MemoryCounter &counter, bool inclusive) const VRAY_OVERRIDE;

public:
	/// Get/set detail handle for this primitive
	const GU_ConstDetailHandle& getDetail() const { return m_detail; }
	VRaySceneRef& setDetail(const GU_ConstDetailHandle &h) { m_detail = h; return *this; }

	VFH_MAKE_ACCESSORS(VFH_VRAY_SCENE_PARAMS, VFH_VRAY_SCENE_PARAMS_COUNT);
	VFH_MAKE_ACCESSORS_TUPLE(VFH_VRAY_SCENE_PARAMS_TUPLES, VFH_VRAY_SCENE_PARAMS_TUPLES_COUNT);

	UT_Vector3D get_mw_shadow_tint_color() const
	{
		fpreal64 v[3];
		_get_mw_shadow_tint_color(v, 3);
		return UT_Vector3D(v[0], v[1], v[2]);
	}
	void _get_mw_shadow_tint_color(fpreal64 * v, exint size) const
	{
		UT_Vector3D val = m_options.hasOption(mw_shadow_tint_color_param_name) ?
			m_options.getOptionV3(mw_shadow_tint_color_param_name) : UT_Vector3D();
		v[0] = val[0]; v[1] = val[1]; v[2] = val[2];
	}
	exint get_mw_shadow_tint_color_size() const
	{
		return m_options.hasOption(mw_shadow_tint_color_param_name) ?
			m_options.getOptionV3(mw_shadow_tint_color_param_name).theSize : 0;
	}
	void set_mw_shadow_tint_color(const UT_Vector3D & val)
	{
		m_options.setOptionV3(mw_shadow_tint_color_param_name, val);
	}
	void _set_mw_shadow_tint_color(const fpreal64 * v, exint size)
	{
		m_options.setOptionV3(mw_shadow_tint_color_param_name, v[0], v[1], v[2]);
	}
	
public:
	const UT_Options& getOptions() const
	{
		return m_options;
	}

private:
	void clearDetail();
	bool updateFrom(const UT_Options &options);

private:
	GU_ConstDetailHandle   m_detail; ///< detail handle for viewport geometry
	UT_Options             m_options; ///< holds params for vrscene
	int                    m_dirty; ///< flags if the detail handle needs update
}; // !class VRaySceneRef

} // !namespace VRayForHoudini

#endif // !VRAY_FOR_HOUDINI_VRAYSCENEREF_H