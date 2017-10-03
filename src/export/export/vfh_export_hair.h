//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
#include "vfh_plugin_exporter.h"

namespace VRayForHoudini {

/// Exports open poly primitives, bezier and nurbs curves as GeomMayaHair plugin.
class HairPrimitiveExporter
	: public PrimitiveExporter
{
public:
	HairPrimitiveExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp, const GEOPrimList &primList);

	bool asPluginDesc(const GU_Detail &gdp, Attrs::PluginDesc &pluginDesc) VRAY_OVERRIDE;
	bool hasData() const VRAY_OVERRIDE { return primList.size(); }

private:
	/// Helper function to get the node which holds optional hair geometry rendering
	/// parameters found on GeomMayaPlugin. These are added to the parent OBJ node
	/// or parent fur network via "Edit Rendering Parameters" interface in Houdini
	/// @retval pointer to the actual node holding the rendering parameters
	///         might be nullptr in which case the defaults are used
	static OP_Node* findPramOwnerForHairParms(OBJ_Node &obj);

	const GEOPrimList &primList;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_HAIR_H
