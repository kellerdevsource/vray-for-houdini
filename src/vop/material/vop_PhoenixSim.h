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

	struct RampContext;
	struct RampHandler: public AurRamps::ChangeHandler, public AurRamps::ColorPickerHandler {
		RampHandler(std::shared_ptr<RampContext> ctx = nullptr): m_ctx(ctx) {}

		/// ChangeHandler overrides
		virtual void OnEditCurveDiagram(AurRamps::RampUi & curve, OnEditType editReason);
		virtual void OnEditColorGradient(AurRamps::RampUi & curve, OnEditType editReason);
		virtual void OnWindowDie();

		/// ColorPickerHandler overrides
		virtual void Create(AurRamps::RampUi & curve, float prefered[3]);
		virtual void Destroy() {}

		std::shared_ptr<RampContext> m_ctx;
	};

	struct RampData {
		std::vector<float>                          m_xS;
		std::vector<float>                          m_yS;
		std::vector<AurRamps::MultiCurvePointType>  m_interps;
		AurRamps::RampType                          m_type;

		RampData(): m_type(AurRamps::RampType_None) {};
	};

	struct RampContext {
		RampContext(AurRamps::RampType type = AurRamps::RampType_None)
			: m_ui(nullptr)
			, m_uiType(type)
		{
			m_data[0].m_type = AurRamps::RampType_Curve;
			m_data[1].m_type = AurRamps::RampType_Color;
		}

		RampData & data(AurRamps::RampType type)
		{
			if (type & AurRamps::RampType_Color) {
				return m_data[1];
			}
			return m_data[0];
		}

		RampHandler        m_handler;
		AurRamps::RampUi * m_ui;
		AurRamps::RampType m_uiType;
	private:
		RampData           m_data[2];
	};


	static PRM_Template       *GetPrmTemplate();

	                           PhxShaderSim(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual                   ~PhxShaderSim() {}

	OP_ERROR                   saveIntrinsic(std::ostream &os, const OP_SaveFlags &sflags) VRAY_OVERRIDE;
	bool                       loadPacket(UT_IStream &is, const char *token, const char *path) VRAY_OVERRIDE;

	bool                       savePresetContents(std::ostream &os) VRAY_OVERRIDE;
	bool                       loadPresetContents(const char *tok, UT_IStream &is) VRAY_OVERRIDE;

	virtual PluginResult       asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

	// this maps property name to ramp data, but since we can have a curve and color ramp in same window
	// some properties might map to one context
	std::unordered_map<std::string, std::shared_ptr<RampContext>> m_ramps;
	std::unordered_map<std::string, AurRamps::RampType>           m_rampTypes;
protected:

	bool                       saveRamps(std::ostream & os);
	bool                       loadRamps(UT_IStream & is);

	virtual void               setPluginType() VRAY_OVERRIDE;
};


} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H

