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
#include <boost/unordered_map.hpp>

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

	/// RampHandler will implement all AurRams handlers interfaces
	struct RampHandler: public AurRamps::ChangeHandler, public AurRamps::ColorPickerHandler {
		RampHandler(RampContext * ctx = nullptr): m_ctx(ctx) {}

		/// ChangeHandler overrides

		/// Called when the curve part of the UI is edited
		/// @param curve - the ui that triggered the change
		/// @param editReason - how the curve was edited
		virtual void OnEditCurveDiagram(AurRamps::RampUi & curve, OnEditType editReason);

		/// Called when the color ramp part of the UI is edited
		/// @param curve - the ui that triggered the change
		/// @param editReason - how the curve was edited
		virtual void OnEditColorGradient(AurRamps::RampUi & curve, OnEditType editReason);

		/// Called when the window is about to be closed either by user action or by calling close() on ui
		virtual void OnWindowDie();

		/// ColorPickerHandler overrides

		/// @param curve - the ui that triggered the picker creation
		/// @param prefered[in] - the prefered starting color of the picker
		virtual void Create(AurRamps::RampUi & curve, float prefered[3]);

		/// Called when the color picker needs to be closed
		virtual void Destroy() {}

		/// Holds the current context that this handler is attached to
		RampContext * m_ctx;
	};

	/// Singe frame data for either color or curve ramp
	struct RampData {
		std::vector<float>                          m_xS;      ///< contains all the keys of the ramp
		std::vector<float>                          m_yS;      ///< if this is curve, there are same nuber of values as m_xS, else there are 3 times more for rgb color
		std::vector<AurRamps::MultiCurvePointType>  m_interps; ///< interpolation types for each point
		AurRamps::RampType                          m_type;    ///< the type of the data, either Ramp or Color but not both

		/// Constructs default data with None type
		RampData(): m_type(AurRamps::RampType_None) {};
	};

	/// RampContext is holder for one instance of ramp in the UI
	/// For example PhxShaderSim has one color ramp, one curve ramp, and on ramp with both
	/// So PhxShaderSim has 3 instances of RampContext
	/// Also this class hold RampData for all possible combinations of RampChannel and RampType
	struct RampContext {
		friend class PhxShaderSim;

		/// Each ramp has different value depending on the channel it operates
		enum RampChannel {
			CHANNEL_DISABLED    = 0,
			CHANNEL_TEMPERATURE = 1,
			CHANNEL_SMOKE       = 2,
			CHANNEL_SPEED       = 3,
			CHANNEL_FUEL        = 4,
			CHANNEL_COUNT       = 4,
		};

		/// Constructs empty context with None type for ramps
		/// @param type - the type of the data inside the context
		RampContext(AurRamps::RampType type = AurRamps::RampType_None)
			: m_ui(nullptr)
			, m_uiType(type)
			, m_freeUi(false)
			, m_activeChan(CHANNEL_SMOKE)
		{
			for (int c = 0; c < CHANNEL_COUNT; ++c) {
				for (int r = AurRamps::RampType_Curve; r <= AurRamps::RampType_Color; ++r) {
					auto type = static_cast<AurRamps::RampType>(r);
					m_data[c][rampTypeToIdx(type)].m_type = type;
				}
			}
		}

		/// Checks if the supplied value is valid RampChannel
		/// @param chan - the desired channel to check
		static bool isValidChannel(RampChannel chan) {
			return chan == CHANNEL_TEMPERATURE || chan == CHANNEL_SMOKE || chan == CHANNEL_SPEED || chan == CHANNEL_FUEL;
		}

		/// Converts RampChannel to index in m_data
		/// @param chan - the desired channel to convert
		/// @retval The index in m_data
		static int rampChanToIdx(RampChannel chan) {
			UT_ASSERT_MSG(chan >= CHANNEL_TEMPERATURE && chan <= CHANNEL_FUEL, "Unexpected value for rampChanToIdx(type)");
			return chan - 1;
		}

		/// Converts RampType to index in m_data.
		/// @param type - the desired type to convert
		/// @retval The index in m_data
		static int rampTypeToIdx(AurRamps::RampType type) {
			UT_ASSERT_MSG(type == AurRamps::RampType_Color || type == AurRamps::RampType_Curve, "Unexpected value for rampTypeToIdx(type)");
			return type - 1;
		}

		/// Returns the appropriate RampData for given type and channel
		/// @param type - either color or curve data to get
		/// @param chan - if supplied is used to get data for this channel, otherwise the current one is used
		/// @retval The RampData
		RampData & data(AurRamps::RampType type, RampChannel chan = CHANNEL_DISABLED) {
			if (chan == CHANNEL_DISABLED) {
				chan = m_activeChan;
			}
			return m_data[rampChanToIdx(chan)][rampTypeToIdx(type)];
		}

		/// Returns the current active channel (the on that is selected in the ui)
		/// @retval The RampChannel
		RampChannel getActiveChannel() const {
			return m_activeChan;
		}

		/// Sets the current active channel and also refreshes the UI's data if it is open
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

		RampHandler                       m_handler; ///< Handles all changes on m_ui
		std::unique_ptr<AurRamps::RampUi> m_ui;      ///< Pointer to the current UI, nullptr if not open
		bool                              m_freeUi;  ///< Flag to mark the m_ui for deletion when OnWindowDie is called
		AurRamps::RampType                m_uiType;  ///< Type is Color Curve or Both since there can be combined ramps
	private:
		typedef RampData RampPair[2];
		RampChannel m_activeChan; ///< The current active channel as selected in Houdini's UI
		RampPair    m_data[4];    ///< Data for 4 channels x 2 type
	};


	static PRM_Template       *GetPrmTemplate();

	                           PhxShaderSim(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual                   ~PhxShaderSim() {}

	/// Called by Houdini when all nodes are loaded, this parses UI and sets proper state on m_ramps
	virtual void               finishedLoadingNetwork(bool is_child_call=false) VRAY_OVERRIDE;

	/// Called by houdini when scene is saved
	OP_ERROR                   saveIntrinsic(std::ostream &os, const OP_SaveFlags &sflags) VRAY_OVERRIDE;
	/// Called on each packet in the saved scene, we parse the one we saved with saveIntrinsic
	bool                       loadPacket(UT_IStream &is, const char *token, const char *path) VRAY_OVERRIDE;

	/// Called when houdini saves presets, so we save current ramps data
	bool                       savePresetContents(std::ostream &os) VRAY_OVERRIDE;
	/// Called on each packet in preset and we only load the one saved with savePresetContents
	bool                       loadPresetContents(const char *tok, UT_IStream &is) VRAY_OVERRIDE;

	virtual PluginResult       asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

	/// Maps property name to ramp data, but since we can have a curve and color ramp in same window
	/// Some properties might map to one context
	boost::unordered_map<std::string, std::shared_ptr<RampContext>> m_ramps;
	/// Maps property name to ramp type, so we know what data to get from RampContext
	boost::unordered_map<std::string, AurRamps::RampType>           m_rampTypes;
protected:
	/// Sets the current active channels for all ramps
	/// @param fromUi - if true this takes the values from the current UI, otherwise uses the default from .ds file.
	///                 This is true when the scene is loaded from file and we need to parse the loaded channels
	void                       onLoadSetActiveChannels(bool fromUi);

	/// Writes ramp data to ostream *wuthot* writing the packet name
	/// @retval - true on success
	bool                       saveRamps(std::ostream & os);
	/// Reads ramp data from UT_IStream
	bool                       loadRamps(UT_IStream & is);

	virtual void               setPluginType() VRAY_OVERRIDE;
};


} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H

