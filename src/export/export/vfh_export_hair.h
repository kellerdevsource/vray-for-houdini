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


namespace VRayForHoudini
{

class HairPrimitiveExporter:
		public PrimitiveExporter
{
public:
	HairPrimitiveExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp);

	virtual void exportPrimitives(const GU_Detail &detail, PluginDescList &plugins) VRAY_OVERRIDE;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_HAIR_H
