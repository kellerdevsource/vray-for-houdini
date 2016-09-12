//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//
#ifdef CGR_HAS_AUR
#ifndef VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H
#define VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H

#include <vfh_vray.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include "vop_node_base.h"

#include "ramps.h"
#include <vector>
#include <unordered_map>

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

	struct RampData {
		std::vector<float>                          xS;
		std::vector<float>                          yS;
		std::vector<AurRamps::MultiCurvePointType>  interps;
	};

	struct RampHandler: AurRamps::ChangeHandler {
		RampHandler(RampData & data): m_Data(data) {}
		virtual void OnEditCurveDiagram(AurRamps::RampUi & curve, OnEditType editReason);
		virtual void OnEditColorGradient(AurRamps::RampUi & curve, OnEditType editReason);
		RampData & m_Data;
	};

	static PRM_Template       *GetPrmTemplate();

	                           PhxShaderSim(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual                   ~PhxShaderSim() {}

	virtual PluginResult       asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

	std::unordered_map<std::string, RampData>     m_Ramps;
	// this will hold handler instances for ramps
	std::unordered_map<std::string, RampHandler*> m_RampHandlers;
protected:

	virtual void               setPluginType() VRAY_OVERRIDE;
};


} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H
#endif // CGR_HAS_AUR
