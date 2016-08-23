//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//
#ifndef VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H
#define VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H

#include <vfh_vray.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include "vop_node_base.h"

namespace VRayForHoudini {
namespace VOP {


class PhxShaderSim:
		public VOP::NodeBase
{
public:
	enum RenderMode {
		Volumetric  = 0,
		Volumetric_Geometry  = 1,
		Volumetric_Heat_Haze  = 2,
		Isosurface  = 3,
		Mesh  = 4,
	};


	static PRM_Template       *GetPrmTemplate();

	                           PhxShaderSim(OP_Network *parent, const char *name, OP_Operator *entry): NodeBase(parent, name, entry) {}
	virtual                   ~PhxShaderSim() {}

	virtual PluginResult       asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

protected:
	virtual void               setPluginType() VRAY_OVERRIDE;
};


} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H
