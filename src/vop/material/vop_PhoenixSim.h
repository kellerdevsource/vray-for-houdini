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
	struct RampHandler: public AurRamps::ChangeHandler, public AurRamps::ColorPickerHandler {
		RampHandler(RampContext * ctx = nullptr): m_Ctx(ctx) {}

		/// ChangeHandler overrides
		virtual void OnEditCurveDiagram(AurRamps::RampUi & curve, OnEditType editReason);
		virtual void OnEditColorGradient(AurRamps::RampUi & curve, OnEditType editReason);
		virtual void OnWindowDie();

		/// ColorPickerHandler overrides
		virtual void Create(AurRamps::RampUi & curve, float prefered[3]);
		virtual void Destroy() {}

		RampContext * m_Ctx;
	};

	struct RampContext {
		RampData           m_Data;
		RampHandler        m_Handler;
		AurRamps::RampUi * m_Ui;
		AurRamps::RampType m_Type;

		RampContext(AurRamps::RampType type = AurRamps::RampType_None): m_Ui(nullptr), m_Type(type) {}
	};


	static PRM_Template       *GetPrmTemplate();

	                           PhxShaderSim(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual                   ~PhxShaderSim() {}

	OP_ERROR                   saveIntrinsic(std::ostream &os, const OP_SaveFlags &sflags) VRAY_OVERRIDE;
	bool                       loadPacket(UT_IStream &is, const char *token, const char *path) VRAY_OVERRIDE;

	bool                       savePresetContents(std::ostream &os) VRAY_OVERRIDE;
	bool                       loadPresetContents(const char *tok, UT_IStream &is) VRAY_OVERRIDE;

	virtual PluginResult       asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

	std::unordered_map<std::string, RampContext> m_Ramps;
protected:

	bool                       saveRamps(std::ostream & os);
	bool                       loadRamps(UT_IStream & is);

	virtual void               setPluginType() VRAY_OVERRIDE;
};


} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H

