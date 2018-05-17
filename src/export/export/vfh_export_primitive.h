//
// Copyright (c) 2015-2018, Chaos Software Ltd
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
#include "vfh_typedefs.h"
#include "vfh_material_override.h"
#include "vfh_plugin_attrs.h"

#include <GA/GA_Primitive.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPacked.h>

namespace VRayForHoudini {

enum ObjectIDTypes {
	objectIdUndefined = -1,
};

class VRayExporter;

/// Base class for exporting primitives from OBJ nodes
class PrimitiveExporter
{
public:
	PrimitiveExporter(OBJ_Node &objNode, OP_Context &ctx, VRayExporter &pluginExporter)
		: objNode(objNode)
		, ctx(ctx)
		, pluginExporter(pluginExporter)
		, tm(1)
		, primID(0)
	{}
	virtual ~PrimitiveExporter() {}

	virtual void exportPrimitive(const GA_Primitive &prim, const PrimMaterial &primMtl, PluginList &volumePlugins) {}

	virtual bool asPluginDesc(const GU_Detail &gdp, Attrs::PluginDesc &pluginDesc) { return false; }
	virtual bool hasData() const { return false; }

	/// Sets transform.
	void setTM(const VRay::Transform &value) { tm = value; }

	/// Sets detail ID.
	void setDetailID(exint value) { primID = value;}

protected:
	/// Object node owner of all details that will be passed to exportPrimitives.
	OBJ_Node &objNode;

	/// Current context used to obtain current time.
	OP_Context &ctx;

	/// Exporter instance for writing plugins.
	VRayExporter &pluginExporter;

	/// Transform.
	VRay::Transform tm;

	/// Detail ID. Used to generate unique plugin name.
	exint primID;
};

#ifdef CGR_HAS_AUR

/// Exports all VRayVolumeGridRef primitives of passed details.
class VolumeExporter
	: public PrimitiveExporter
{
public:
	VolumeExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp)
		: PrimitiveExporter(obj, ctx, exp)
	{}

	void exportPrimitive(const GA_Primitive &prim, const PrimMaterial &primMtl, PluginList &pluginsSet) VRAY_OVERRIDE;

protected:
	VRay::Plugin exportVRayVolumeGridRef(OBJ_Node &objNode, const GU_PrimPacked &prim) const;
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

	void exportPrimitive(const GA_Primitive &prim, const PrimMaterial &primMtl, PluginList &volumePlugins) VRAY_OVERRIDE;
};

#endif // CGR_HAS_AUR

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_PRIMITIVE_H



