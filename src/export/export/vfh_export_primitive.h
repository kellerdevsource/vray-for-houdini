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
#include "vfh_material_override.h"

#include <GA/GA_Primitive.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPacked.h>

namespace VRayForHoudini {

enum ObjectIDTypes {
	objectIdUndefined = -1,
};

struct PrimitiveItem {
	enum InstancerItemFlags {
		itemFlagsNone = 0,
		itemFlagsUseTime = (1 << 0),
	};

	explicit PrimitiveItem(VRay::Plugin geometry=VRay::Plugin(),
						   VRay::Plugin material=VRay::Plugin())
		: prim(nullptr)
		, primID(0)
		, geometry(geometry)
		, tm(1)
		, objectID(objectIdUndefined)
		, t(0.0)
		, flags(itemFlagsNone)
	{}

	/// Primitive.
	const GA_Primitive *prim;

	/// Primitive ID.
	exint primID;

	/// Material.
	PrimMaterial primMaterial;

	/// Exporterd geometry plugin.
	VRay::Plugin geometry;

	/// Transform.
	VRay::Transform tm;

	/// Object ID.
	int objectID;

	/// Time instancing.
	fpreal t;

	/// Flags.
	uint32_t flags;
};

typedef VUtils::Table<PrimitiveItem, -1> PrimitiveItems;

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
		, detailID(0)
	{}
	virtual ~PrimitiveExporter() {}

	virtual void exportPrimitive(const PrimitiveItem &item) {}

	/// Generate plugin descriptions for all supported primitives in the provided GU_Detail
	/// @gdp - the detail to traverse
	/// @plugins[out] - the list of plugins generated for this detail
	virtual void exportPrimitives(const GU_Detail &detail, PrimitiveItems &plugins) {}

	/// Sets transform.
	void setTM(const VRay::Transform &value) { tm = value; }

	/// Sets detail ID.
	void setDetailID(exint value) { detailID = value;}

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
	exint detailID;
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

	void exportPrimitive(const PrimitiveItem &item) VRAY_OVERRIDE;

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

	void exportPrimitive(const PrimitiveItem &item) VRAY_OVERRIDE;
};

#endif // CGR_HAS_AUR

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_PRIMITIVE_H



