//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORT_HAIR_H
#define VRAY_FOR_HOUDINI_EXPORT_HAIR_H

#include "vfh_export_primitive.h"
#include "vfh_export_geom.h"

namespace VRayForHoudini
{

/// Exports open poly primitives, bezier and nurbs curves as V-Ray Hair plugin
/// from geometry detail
class HairPrimitiveExporter
	: public PrimitiveExporter
{
public:
	HairPrimitiveExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp, const GEOPrimList &primList);

	/// Generate hair plugin description from all supported primitives in the
	/// GU_Detail provided
	/// @param gdp[in] - the detail to traverse
	/// @param pluginDesc[out] - the hair plugin description
	/// @retval true if hair primitives were found in gdp
	///         and pluginDesc is modified
	bool asPluginDesc(const GU_Detail &gdp, Attrs::PluginDesc &pluginDesc);

	/// Export hair geometry plugin and generate Node plugin description
	/// for all supported primitives in the GU_Detail provided
	/// @param gdp[in] - the detail to traverse
	/// @param plugins[out] - if any plugins are generted they will appended
	///                       to this list
	void exportPrimitives(const GU_Detail &gdp, PrimitiveItems &plugins) VRAY_OVERRIDE {}
	void exportPrimitive(const PrimitiveItem &item) VRAY_OVERRIDE {}

private:
	/// Helper function to get the node which holds optional hair geometry rendering
	/// parameters found on GeomMayaPlugin. These are added to the parent OBJ node
	/// or parent fur network via "Edit Rendering Parameters" interface in Houdini
	/// @retval pointer to the actual node holding the rendering parameters
	///         might be nullptr in which case the defaults are used
	OP_Node* findPramOwnerForHairParms() const;

	const GEOPrimList &primList;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_HAIR_H
