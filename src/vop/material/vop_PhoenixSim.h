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
		RampHandler(RampContext * ctx = nullptr): m_ctx(ctx) {}

		/// ChangeHandler overrides
		virtual void OnEditCurveDiagram(AurRamps::RampUi & curve, OnEditType editReason);
		virtual void OnEditColorGradient(AurRamps::RampUi & curve, OnEditType editReason);
		virtual void OnWindowDie();

		/// ColorPickerHandler overrides
		virtual void Create(AurRamps::RampUi & curve, float prefered[3]);
		virtual void Destroy() {}

		RampContext * m_ctx;
	};

	struct RampData {
		std::vector<float>                          m_xS;
		std::vector<float>                          m_yS;
		std::vector<AurRamps::MultiCurvePointType>  m_interps;
		AurRamps::RampType                          m_type;

		RampData(): m_type(AurRamps::RampType_None) {};
	};

	struct RampContext {
		enum RampChannel {
			CHANNEL_TEMPERATURE = 1,
			CHANNEL_SMOKE       = 2,
			CHANNEL_SPEED       = 3,
			CHANNEL_FUEL        = 4,
			CHANNEL_COUNT       = 4,
		};

		RampContext(AurRamps::RampType type = AurRamps::RampType_None)
			: m_ui(nullptr)
			, m_uiType(type)
			, m_freeUi(false)
			, m_activeChan(CHANNEL_SMOKE)
		{
			for (int c = 0; c < CHANNEL_COUNT; ++c) {
				m_data[c][0].m_type = AurRamps::RampType_Curve;
				m_data[c][1].m_type = AurRamps::RampType_Color;
			}
		}

		RampData & data(AurRamps::RampType type)
		{
			if (type & AurRamps::RampType_Color) {
				return m_data[m_activeChan - 1][1];
			}
			return m_data[m_activeChan - 1][0];
		}

		void setActiveChannel(RampChannel ch) {
			if (ch < CHANNEL_TEMPERATURE || ch > CHANNEL_FUEL) {
				Log::getLog().error("Invalid active channel set %d", static_cast<int>(ch));
				return;
			}

			const bool differ = ch != m_activeChan;
			m_activeChan = ch;
			// if we change active channel and there is open UI - we need to update UI's data
			if (differ) {
				if (m_ui && !m_freeUi) {
					if (m_uiType & AurRamps::RampType_Curve) {
						auto & curveData = data(AurRamps::RampType_Curve);
						m_ui->setCurvePoints(curveData.m_xS.data(), curveData.m_yS.data(), curveData.m_interps.data(), curveData.m_xS.size());
					}

					if (m_uiType & AurRamps::RampType_Color) {
						auto & colorData = data(AurRamps::RampType_Color);
						// NOTE: here rampData.yS is color type which is 3 floats per point so actual count is rampData.xS.size() !!
						m_ui->setColorPoints(colorData.m_xS.data(), colorData.m_yS.data(), colorData.m_xS.size());
					}
				}
			}
		}

		RampHandler                       m_handler;
		std::unique_ptr<AurRamps::RampUi> m_ui;
		// flag to mark the @m_ui for deletion when OnWindowDie is called
		bool                              m_freeUi;
		AurRamps::RampType                m_uiType;
	private:
		typedef RampData RampPair[2];
		RampChannel m_activeChan;
		RampPair    m_data[4];
	};


	static PRM_Template       *GetPrmTemplate();

	                           PhxShaderSim(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual                   ~PhxShaderSim() {}

	virtual void               finishedLoadingNetwork(bool is_child_call=false) VRAY_OVERRIDE;

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

