//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORT_PRIMITIVE_H
#define VRAY_FOR_HOUDINI_EXPORT_PRIMITIVE_H

#include "vfh_vray.h"
#include "vfh_exporter.h"

namespace VRayForHoudini {

struct InstancerItem {
	InstancerItem()
		: tm(1)
	{}

	/// Geometry.
	VRay::Plugin geometry;

	/// Override material.
	VRay::Plugin material;

	/// Transform.
	VRay::Transform tm;

	/// User attributes.
	UT_String userAttributes;
};

typedef VUtils::Table<InstancerItem, -1> InstancerItems;

typedef VUtils::Table<VRay::Plugin> PluginsTable;
typedef std::vector< VRay::Plugin > PluginList;
typedef std::list< Attrs::PluginDesc > PluginDescList;

/// Base class for exporting primitives from OBJ nodes
class PrimitiveExporter
{
public:
	PrimitiveExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp)
		: m_object(obj)
		, m_context(ctx)
		, m_exporter(exp)
	{}
	virtual ~PrimitiveExporter() {}

	/// Generate plugin descriptions for all supported primitives in the provided GU_Detail
	/// @gdp - the detail to traverse
	/// @plugins[out] - the list of plugins generated for this detail
	virtual void exportPrimitives(const GU_Detail &detail, InstancerItems &plugins) = 0;

protected:
	/// Object node owner of all details that will be passed to exportPrimitives.
	OBJ_Node &m_object;

	/// Current context used to obtain current time.
	OP_Context &m_context;

	/// Exporter instance for writing plugins.
	VRayExporter &m_exporter;
};

typedef std::shared_ptr<PrimitiveExporter> PrimitiveExporterPtr;

#ifdef CGR_HAS_AUR

/// Exports all VRayVolumeGridRef primitives of passed details.
class VolumeExporter
	: public PrimitiveExporter
{
public:
	VolumeExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp)
		: PrimitiveExporter(obj, ctx, exp)
	{}

	/// Generate plugin descriptions for all supported primitives in the provided GU_Detail
	/// @gdp - the detail to traverse
	/// @plugins[out] - the list of plugins generated for this detail
	void exportPrimitives(const GU_Detail &detail, InstancerItems &plugins) VRAY_OVERRIDE;

protected:
	/// Export the PhxShaderCache for the given primitive
	/// @prim - the primitive
	void exportCache(const GA_Primitive &prim);

	/// Export the PhxShaderSim for the given SHOP node and associate it with a PhxShaderCache by name
	/// @shop - pointer to the SHOP node containing the sim properties
	/// @overrideAttrs - list of attributes that need to be overriden in the sim plugin, e.g. node_transform
	/// @cacheName - the name of the PhxShaderCache for the 'cache' property
	void exportSim(SHOP_Node *shop, const Attrs::PluginAttrs &overrideAttrs, const std::string &cacheName);
};

/// Specialization for exporting Houdini's volumes as textures
/// Uses VolumeExporter::exportCache and VolumeExporter::exportSim to export needed plugins
class HoudiniVolumeExporter
	: public VolumeExporter
{
public:
	HoudiniVolumeExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp)
		: VolumeExporter(obj, ctx, exp)
	{}

	/// Generate plugin descriptions for all supported primitives in the provided GU_Detail
	/// @gdp - the detail to traverse
	/// @plugins[out] - the list of plugins generated for this detail
	void exportPrimitives(const GU_Detail &detail, InstancerItems &plugins) VRAY_OVERRIDE;
};

#endif // CGR_HAS_AUR

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_PRIMITIVE_H



