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

	struct RampContext;
	struct RampHandler: AurRamps::ChangeHandler {
		RampHandler(RampContext * ctx = nullptr): m_Ctx(ctx) {}
		virtual void OnEditCurveDiagram(AurRamps::RampUi & curve, OnEditType editReason);
		virtual void OnEditColorGradient(AurRamps::RampUi & curve, OnEditType editReason);
		virtual void OnWindowDie();
		RampContext * m_Ctx;
	};

	struct RampContext {
		RampData           m_Data;
		RampHandler        m_Hander;
		AurRamps::RampUi * m_Ui;
		AurRamps::RampType m_Type;

		RampContext(AurRamps::RampType type = AurRamps::RampType_None): m_Ui(nullptr), m_Type(type) {}
	};


	static PRM_Template       *GetPrmTemplate();

	                           PhxShaderSim(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual                   ~PhxShaderSim() {}

	virtual PluginResult       asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

	std::unordered_map<std::string, RampContext> m_Ramps;
protected:

	virtual void               setPluginType() VRAY_OVERRIDE;
};


} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H
#endif // CGR_HAS_AUR
