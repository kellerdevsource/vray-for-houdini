//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//
#ifdef CGR_HAS_AUR

#include "vfh_prm_templates.h"
#include "vfh_tex_utils.h"
#include "vfh_hou_utils.h"
#include "vfh_attr_utils.h"

#include "vop_PhoenixSim.h"
#include "gu_volumegridref.h"

#include <UT/UT_IStream.h>
#include <OP/OP_SaveFlags.h>
#include <OP/OP_Director.h>
#include <SOP/SOP_Node.h>
#include <PRM/PRM_RampUtils.h>
#include <HOM/HOM_Ramp.h>
#include <CH/CH_Channel.h>
#include <GEO/GEO_Detail.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_Detail.h>

#include <utility>

#include <QWidget>

using namespace AurRamps;
using namespace std;

namespace VRayForHoudini {
namespace VOP {

/// RampHandler will implement all AurRams handlers interfaces
struct RampHandler: public ChangeHandler, public ColorPickerHandler {
	RampHandler(RampContext * ctx = nullptr): m_ctx(ctx) {}

	/// ChangeHandler overrides

	/// Called by Ramps lib when the curve part of the UI is edited
	/// @param curve - the ui that triggered the change
	/// @param editReason - how the curve was edited
	virtual void OnEditCurveDiagram(RampUi & curve, OnEditType editReason);

	/// Called by Ramps lib when the color ramp part of the UI is edited
	/// @param curve - the ui that triggered the change
	/// @param editReason - how the curve was edited
	virtual void OnEditColorGradient(RampUi & curve, OnEditType editReason);

	/// Called by Ramps lib when the window is about to be closed either by user action or by calling close() on ui
	virtual void OnWindowDie();

	/// ColorPickerHandler overrides

	/// @param curve - the ui that triggered the picker creation
	/// @param prefered[in] - the prefered starting color of the picker
	virtual void Create(RampUi & curve, float prefered[3])
	{
		float result[3];
		bool canceled = curve.defaultColorPicker(prefered, result);
		curve.setSelectedPointsColor(result, canceled);
	}

	/// Called by Ramps lib when the color picker needs to be closed
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
/// Also this class holds RampData for all possible combinations of RampChannel and RampType
struct RampContext {
	friend class PhxShaderSim;

	/// Each ramp has different value depending on the channel it operates
	enum RampChannel {
		CHANNEL_DISABLED    = 0,
		CHANNEL_TEMPERATURE = 1,
		CHANNEL_SMOKE       = 2,
		CHANNEL_SPEED       = 3,
		CHANNEL_FUEL        = 4,
	};

	static const int CHANNEL_COUNT = 4; ///< actual count of channels - temp, smoke, speed and fuel

	typedef std::array<VRayVolumeGridRef::MinMaxPair, CHANNEL_COUNT> MinMaxMap;

	/// Constructs empty context with None type for ramps
	/// @param type - the type of the data inside the context
	RampContext(AurRamps::RampType type = AurRamps::RampType_None)
		: m_ui(nullptr)
		, m_freeUi(false)
		, m_uiType(type)
		, m_activeChan(CHANNEL_SMOKE)
	{
		for (int c = 0; c < CHANNEL_COUNT; ++c) {
			for (int r = AurRamps::RampType_Curve; r <= AurRamps::RampType_Color; ++r) {
				auto type = static_cast<AurRamps::RampType>(r);
				m_data[c][rampTypeToIdx(type)].m_type = type;
			}
		}
		memset(m_minMax.data(), 0, m_minMax.size() * sizeof(m_minMax[0]));
	}

	/// Check if the supplied value is valid RampChannel
	/// @param chan - the desired channel to check
	static bool isValidChannel(RampChannel chan) {
		return chan == CHANNEL_TEMPERATURE || chan == CHANNEL_SMOKE || chan == CHANNEL_SPEED || chan == CHANNEL_FUEL;
	}

	/// Convert RampChannel to index in m_data
	/// @param chan - the desired channel to convert
	/// @retval The index in m_data
	static int rampChanToIdx(RampChannel chan) {
		UT_ASSERT_MSG(chan >= CHANNEL_TEMPERATURE && chan <= CHANNEL_FUEL, "Unexpected value for rampChanToIdx(type)");
		return chan - 1;
	}

	/// Convert RampType to index in m_data.
	/// @param type - the desired type to convert
	/// @retval The index in m_data
	static int rampTypeToIdx(AurRamps::RampType type) {
		UT_ASSERT_MSG(type == AurRamps::RampType_Color || type == AurRamps::RampType_Curve, "Unexpected value for rampTypeToIdx(type)");
		return type - 1;
	}

	static GridChannels::Enum rampChannelToPhxChannel(RampChannel chan) {
		switch (chan) {
			case CHANNEL_TEMPERATURE: return GridChannels::ChT;
			case CHANNEL_SMOKE: return GridChannels::ChSm;
			case CHANNEL_SPEED: return GridChannels::ChSp;
			case CHANNEL_FUEL: return GridChannels::ChFl;
		}
		return GridChannels::ChReserved;
	}

	/// Return the appropriate RampData for given type and channel
	/// @param type - either color or curve data to get
	/// @param chan - if supplied is used to get data for this channel, otherwise the current one is used
	/// @retval The RampData
	RampData & data(AurRamps::RampType type, RampChannel chan = CHANNEL_DISABLED) {
		if (chan == CHANNEL_DISABLED) {
			chan = m_activeChan;
		}
		return m_data[rampChanToIdx(chan)][rampTypeToIdx(type)];
	}

	/// Return the current active channel (the on that is selected in the ui)
	/// @retval The RampChannel
	RampChannel getActiveChannel() const {
		return m_activeChan;
	}

	/// Set the min/max highligthed areas in the UI - call when the linked SOP is changed or current channel is changed
	void refreshUi() const {
		if (!m_ui || m_freeUi) {
			return;
		}

		auto sts = m_ui->getSettings();
		sts.highlightedDataRegionMax = m_minMax[rampChanToIdx(m_activeChan)].max;
		sts.highlightedDataRegionMin = m_minMax[rampChanToIdx(m_activeChan)].min;
		m_ui->setSettings(sts);
	}

	/// Set the current active channel and also refreshes the UI's data if it is open
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
			refreshUi();
		}
	}

	RampHandler             m_handler; ///< Handles all changes on m_ui
	std::unique_ptr<RampUi> m_ui;      ///< Pointer to the current UI, nullptr if not open
	bool                    m_freeUi;  ///< Flag to mark the m_ui for deletion when OnWindowDie is called
	AurRamps::RampType      m_uiType;  ///< Type is Color Curve or Both since there can be combined ramps
	MinMaxMap               m_minMax;  ///< min max value for each channel
private:
	typedef RampData RampPair[2];
	RampChannel m_activeChan;          ///< The current active channel as selected in Houdini's UI
	RampPair    m_data[CHANNEL_COUNT]; ///< Data for 4 channels x 2 type
};

/// Called when the curve ramp part of the UI is edited
/// @param curve - the ui that triggered the change
/// @param editReason - how the curve was edited
void RampHandler::OnEditCurveDiagram(RampUi & curve, OnEditType editReason)
{
	// we are interested in finished changes only
	if (!m_ctx || !(m_ctx->m_uiType & RampType_Curve) || editReason == OnEdit_ChangeBegin || editReason == OnEdit_ChangeInProgress) {
		return;
	}
	// sanity check
	UT_ASSERT(&curve == m_ctx->m_ui.get());

	auto & data = m_ctx->data(RampType_Curve);
	const auto size = curve.pointCount(RampType_Curve);

	data.m_xS.resize(size);
	data.m_yS.resize(size);
	data.m_interps.resize(size);

	const auto newSize = curve.getCurvePoints(data.m_xS.data(), data.m_yS.data(), data.m_interps.data(), size);

	// if we got less points resize down
	if (newSize != size) {
		data.m_xS.resize(newSize);
		data.m_yS.resize(newSize);
		data.m_interps.resize(newSize);
	}
}

/// Called when the color ramp part of the UI is edited
/// @param curve - the ui that triggered the change
/// @param editReason - how the curve was edited
void RampHandler::OnEditColorGradient(RampUi & curve, OnEditType editReason)
{
	// we are interested in finished change only
	if (!m_ctx || !(m_ctx->m_uiType & RampType_Color) || editReason == OnEdit_ChangeBegin || editReason == OnEdit_ChangeInProgress) {
		return;
	}
	// sanity check
	UT_ASSERT(&curve == m_ctx->m_ui.get());

	auto & data = m_ctx->data(RampType_Color);
	const auto size = curve.pointCount(RampType_Color);

	data.m_xS.resize(size);
	// NOTE: m_Data.yS is of color type so it's 3 floats per point!
	data.m_yS.resize(size * 3);

	const auto newSize = curve.getColorPoints(data.m_xS.data(), data.m_yS.data(), size);

	// if we got less points resize down
	if (newSize != size) {
		data.m_xS.resize(newSize);
		data.m_yS.resize(newSize * 3);
	}
}

void RampHandler::OnWindowDie()
{
	if (m_ctx) {
		m_ctx->m_freeUi = true;
	}
	// just in case
	m_ctx = nullptr;
}

} // namespace VRayForHoudini
} // namespace VOP

using namespace VRayForHoudini;
using namespace VOP;

static PRM_Template * AttrItems = nullptr;
static const char * SAVE_SEPARATOR = "\n";
static const char * SAVE_TOKEN = "phx_ramp_data";

namespace {

/// Adds curve point to data
/// @param data - RampData to add point to
/// @param x - key value
/// @param y - curve value
/// @param pt - interpolation type
void addCurvePoint(RampData & data, float x, float y, MultiCurvePointType pt)
{
	data.m_xS.push_back(x);
	data.m_yS.push_back(y);
	data.m_interps.push_back(pt);
}

/// Adds color point to data
/// @param data - RampData to add point to
/// @param x - key value
/// @param r - color r value
/// @param g - color g value
/// @param b - color b value
/// @param pt - interpolation type
void addColorPoint(RampData & data, float x, float r, float g, float b, MultiCurvePointType pt)
{
	data.m_xS.push_back(x);

	data.m_yS.push_back(r);
	data.m_yS.push_back(g);
	data.m_yS.push_back(b);

	data.m_interps.push_back(pt);
}
} // namespace


void PhxShaderSim::clearRampData()
{
	const int chanCount = RampContext::CHANNEL_COUNT;
	for (auto & ramp : m_ramps) {
		for (int c = 0; c < chanCount; c++) {
			const auto type = m_rampTypes[ramp.first];
			auto & data = ramp.second->data(type, static_cast<RampContext::RampChannel>(c + 1));
			data.m_xS.clear();
			data.m_yS.clear();
			data.m_interps.clear();
		}
	}
}


void PhxShaderSim::setRampDefaults()
{
	const int chanCount = RampContext::CHANNEL_COUNT;
	const float MINT = 800;
	const float MAXT = 3000;
	const float fireMul[chanCount] = { 1.0f, 1.0f / MAXT, 0.1f, 1.0f / MAXT };

	const float smokeColorMul[chanCount] = { 4000, 1, 4000, 1 };
	const float smokeColors[chanCount * 3] = { 0,0,1, 0,1,0, 1,1,0, 1,0,0 };

	// smoke transp xS
	const float p0x[chanCount] = { 300, 0,  90, 0};
	const float p1x[chanCount] = {2000, 1, 600, 1};

	// defaults
	for (int c = 0; c < chanCount; ++c) {
		const auto ch = static_cast<RampContext::RampChannel>(c + 1);
		auto & fireColor = m_ramps["ecolor_ramp"]->data(RampType_Color, ch);
		auto & fireCurve = m_ramps["elum_curve"]->data(RampType_Curve, ch);
		auto & smokeCurve = m_ramps["transp_curve"]->data(RampType_Curve, ch);
		auto & smokeColor = m_ramps["dcolor_ramp"]->data(RampType_Color, ch);

		// fire
		const float fireColors[6] = { 1,0.094,0,  1,0.597,0.255 };

		for (int r = 0; r < 2; ++r) {
			const float T = MINT + (MAXT - MINT) * r / (2 - 1);
			const float x = T * fireMul[c];
			addCurvePoint(fireCurve, x, (T - MINT)/(MAXT - MINT), AurRamps::MCPT_Linear);
			addColorPoint(fireColor, x, fireColors[r * 3 + 0], fireColors[r * 3 + 1], fireColors[r * 3 + 2], AurRamps::MCPT_Linear);
		}

		// smoke color
		for (int r = 0; r < 4; ++r) {
			const float x = smokeColorMul[c] * r / (4 - 1);
			addColorPoint(smokeColor, x, smokeColors[r * 3 + 0], smokeColors[r * 3 + 1], smokeColors[r * 3 + 2], AurRamps::MCPT_Linear);
		}

		// smoke transp
		addCurvePoint(smokeCurve, p0x[c], 0.f, AurRamps::MCPT_Linear);
		addCurvePoint(smokeCurve, p1x[c], 1.f, AurRamps::MCPT_Linear);
	}
}


int PhxShaderSim::setPresetTypeCB(void *data, int index, fpreal64 time, const PRM_Template *tplate)
{
	const int chanCount = RampContext::CHANNEL_COUNT;
	auto simNode = reinterpret_cast<PhxShaderSim*>(data);

	UT_String presetName;
	simNode->evalString(presetName, tplate->getToken(), 0, time);

	simNode->clearRampData();
	simNode->setRampDefaults();

	// presets
	if (presetName == "FumeFX") {
		// channel is fuel
		// fire ramps
		auto & ecolorRamp = simNode->m_ramps["ecolor_ramp"]->data(RampType_Color, RampContext::RampChannel::CHANNEL_FUEL);
		ecolorRamp.m_xS.clear();
		ecolorRamp.m_yS.clear();
		ecolorRamp.m_interps.clear();

		addColorPoint(ecolorRamp, 0.1f, 1.f, 0.33f, 0.f, AurRamps::MCPT_Spline);

		auto & epowerCurve = simNode->m_ramps["elum_curve"]->data(RampType_Curve, RampContext::RampChannel::CHANNEL_FUEL);
		epowerCurve.m_xS.clear();
		epowerCurve.m_yS.clear();
		epowerCurve.m_interps.clear();

		addCurvePoint(epowerCurve, 0.010, 0.000, AurRamps::MCPT_Linear);
		addCurvePoint(epowerCurve, 0.100, 1.000, AurRamps::MCPT_Linear);
		addCurvePoint(epowerCurve, 0.200, 0.130, AurRamps::MCPT_Linear);
		addCurvePoint(epowerCurve, 1.000, 0.100, AurRamps::MCPT_Linear);
	} else if (presetName == "HoudiniVolume") {
		// channel is temp
		// fire ramps

		auto & ecolorRamp = simNode->m_ramps["ecolor_ramp"]->data(RampType_Color, RampContext::RampChannel::CHANNEL_TEMPERATURE);
		ecolorRamp.m_xS.clear();
		ecolorRamp.m_yS.clear();
		ecolorRamp.m_interps.clear();

		addColorPoint(ecolorRamp,  0.0f, 0.0, 0.00, 0.0, AurRamps::MCPT_Spline);
		addColorPoint(ecolorRamp,  8.0f, 1.0, 0.65, 0.0, AurRamps::MCPT_Spline);
		addColorPoint(ecolorRamp, 13.0f, 1.0, 0.88, 0.0, AurRamps::MCPT_Spline);
		addColorPoint(ecolorRamp, 14.0f, 1.0, 1.00, 1.0, AurRamps::MCPT_Spline);

		auto & epowerCurve = simNode->m_ramps["elum_curve"]->data(RampType_Curve, RampContext::RampChannel::CHANNEL_TEMPERATURE);
		epowerCurve.m_xS.clear();
		epowerCurve.m_yS.clear();
		epowerCurve.m_interps.clear();

		addCurvePoint(epowerCurve,  0.01, 0.000, AurRamps::MCPT_Spline);
		addCurvePoint(epowerCurve, 14.00, 1.000, AurRamps::MCPT_Spline);
	} else if (presetName == "HoudiniLiquid") {

	} else if (presetName == "MayaFluids") {
		// channel is temp
		// fire ramps

		auto & ecolorRamp = simNode->m_ramps["ecolor_ramp"]->data(RampType_Color, RampContext::RampChannel::CHANNEL_TEMPERATURE);
		ecolorRamp.m_xS.clear();
		ecolorRamp.m_yS.clear();
		ecolorRamp.m_interps.clear();

		addColorPoint(ecolorRamp, 0.0f, 0.00, 0.00, 0.00, AurRamps::MCPT_Spline);
		addColorPoint(ecolorRamp, 3.0f, 1.00, 0.25, 0.10, AurRamps::MCPT_Spline);
		addColorPoint(ecolorRamp, 3.5f, 1.37, 1.00, 0.00, AurRamps::MCPT_Spline);
		addColorPoint(ecolorRamp, 4.0f, 1.56, 1.56, 0.98, AurRamps::MCPT_Spline);

		auto & epowerCurve = simNode->m_ramps["elum_curve"]->data(RampType_Curve, RampContext::RampChannel::CHANNEL_TEMPERATURE);
		epowerCurve.m_xS.clear();
		epowerCurve.m_yS.clear();
		epowerCurve.m_interps.clear();

		addCurvePoint(epowerCurve, 0.2, 0.000, AurRamps::MCPT_Spline);
		addCurvePoint(epowerCurve, 4.5, 1.000, AurRamps::MCPT_Spline);

		// smoke opacity
		auto & transpCurve = simNode->m_ramps["transp_curve"]->data(RampType_Curve, RampContext::RampChannel::CHANNEL_SMOKE);
		transpCurve.m_xS.clear();
		transpCurve.m_yS.clear();
		transpCurve.m_interps.clear();

		addCurvePoint(transpCurve, 0.002, 0.00, AurRamps::MCPT_Spline);
		addCurvePoint(transpCurve, 0.008, 0.40, AurRamps::MCPT_Spline);
		addCurvePoint(transpCurve, 0.040, 0.70, AurRamps::MCPT_Spline);
		addCurvePoint(transpCurve, 0.110, 0.83, AurRamps::MCPT_Spline);
		addCurvePoint(transpCurve, 0.440, 0.95, AurRamps::MCPT_Spline);
	}
	return 1;
}


int PhxShaderSim::rampDropDownDependCB(void * data, int index, fpreal64 time, const PRM_Template *tplate)
{
	auto simNode = reinterpret_cast<PhxShaderSim*>(data);
	// this is they ramp key (two ramps could share same key -> they are merge in one window)
	const string token = tplate->getSparePtr()->getValue("vray_ramp_depend");

	auto ctx = simNode->m_ramps[token];
	const auto chan = static_cast<RampContext::RampChannel>(index);

	if (!ctx) {
		Log::getLog().error("Missing context for \"%s\"!", token.c_str());
		return 0;
	}

	if (!RampContext::isValidChannel(chan)) {
		if (ctx->m_ui && !ctx->m_freeUi) {
			ctx->m_ui->close();
		}
	} else {
		ctx->setActiveChannel(chan);
	}

	return 1;
}


void PhxShaderSim::loadDataRanges()
{
	const char * token = "selectedSopPath";
	const VRayVolumeGridRef::MinMaxPair zeroMinMax = {0, 0};
	// zero out all min/max ranges
	for (auto &rampIter : m_ramps) {
		if (auto ramp = rampIter.second) {
			for (int c = 0; c < RampContext::CHANNEL_COUNT; c++) {
				ramp->m_minMax[c] = zeroMinMax;
			}
			ramp->refreshUi();
		}
	}

	UT_String sopPath;
	evalString(sopPath, token, 0, 0);

	SOP_Node *cacheSop = getSOPNodeFromPath(sopPath);
	if (!cacheSop || !cacheSop->getOperator()->getName().startsWith("VRayNodePhxShaderCache")) {
		Log::getLog().warning("Only a V-Ray PhxShaderCache sop can be selected!");
		return;
	}

	OP_Context context(CHgetEvalTime());
	GU_DetailHandleAutoReadLock gdl(cacheSop->getCookedGeoHandle(context));
	if (!gdl.isValid()) {
		return;
	}
	const GU_Detail &detail = *gdl.getGdp();
	auto &primList = detail.getPrimitiveList();
	const int primCount = primList.offsetSize();

	const GA_Primitive *volumePrim = nullptr;
	// check all primities if we have a VRayVolumeGridRef
	for (int c = 0; c < primCount; ++c) {
		auto prim = primList.get(c);
		if (prim && prim->getTypeId() == VRayVolumeGridRef::typeId()) {
			volumePrim = prim;
			break;
		}
	}

	if (!volumePrim) {
		Log::getLog().warning("Selected SOP does not contain a VRayNodePhxShaderCache node!");
		return;
	}

	auto *packedPrim = UTverify_cast<const GU_PrimPacked*>(volumePrim);
	const auto *impl = reinterpret_cast<const VRayVolumeGridRef*>(packedPrim->implementation());
	const auto &ranges = impl->getChannelDataRanges();

	for (auto &rampIter : m_ramps) {
		if (auto ramp = rampIter.second) {
			for (int c = 0; c < RampContext::CHANNEL_COUNT; c++) {
				ramp->m_minMax[c] = ranges[RampContext::rampChannelToPhxChannel(static_cast<RampContext::RampChannel>(c + 1))];
			}
			ramp->refreshUi();
		}
	}
}


int PhxShaderSim::rampButtonClickCB(void *data, int, fpreal64, const PRM_Template *tplate)
{
	using namespace std;
	using namespace AurRamps;
#if _WIN32
	static auto app = AurRamps::getQtGUI(GetCurrentProcess());
#else
	static auto app = AurRamps::getQtGUI(nullptr);
#endif // WIN32

	const string token = tplate->getToken();

	auto simNode = reinterpret_cast<PhxShaderSim*>(data);
	simNode->loadDataRanges();
	auto ctx = simNode->m_ramps[token];

	// this should not happen - calling callback on uninited context
	if (!ctx) {
		Log::getLog().error("Missing context for \"%s\"!", token.c_str());
		return 0;
	}

	if (ctx->m_freeUi) {
		ctx->m_ui.reset(nullptr);
		ctx->m_freeUi = false;
	} else if (ctx->m_ui) {
		ctx->m_ui->close();
		ctx->m_ui.reset(nullptr);
		return 0;
	}

	if (ctx->m_freeUi) {
		// we already have window opened - do nothing
		return 0;
	}

	const int rampHeight  = 300;
	const int colorHeight = 80;

	int height = 0;
	if (ctx->m_uiType & RampType_Color) {
		height += colorHeight;
	}
	if (ctx->m_uiType & RampType_Curve) {
		height += rampHeight;
	}

	ctx->m_ui.reset(RampUi::createRamp(tplate->getLabel(), ctx->m_uiType, 200, 200, 300, height, app));
	QWidget *windowHandle = reinterpret_cast<QWidget*>(ctx->m_ui->getWindowHande());
	if (windowHandle) {
		windowHandle->setParent(HOU::getMainQtWindow());
		Qt::WindowFlags windowFlags = windowHandle->windowFlags();
		windowFlags |= (Qt::Window | Qt::WindowStaysOnTopHint);
		windowHandle->setWindowFlags(windowFlags);
	}
	// set ramp data to the ramp window, it will be kept in sycn via the callbacks
	if (ctx->m_uiType & RampType_Curve) {
		auto & curveData = ctx->data(RampType_Curve);
		ctx->m_ui->setCurvePoints(curveData.m_xS.data(), curveData.m_yS.data(), curveData.m_interps.data(), curveData.m_xS.size());
	}

	if (ctx->m_uiType & RampType_Color) {
		auto & colorData = ctx->data(RampType_Color);
		// NOTE: here rampData.yS is color type which is 3 floats per point so actual count is rampData.xS.size() !!
		ctx->m_ui->setColorPoints(colorData.m_xS.data(), colorData.m_yS.data(), colorData.m_xS.size());
	}

	ctx->m_handler = RampHandler(&*ctx);
	ctx->m_ui->setChangeHandler(&ctx->m_handler);
	if (ctx->m_uiType & RampType_Color) {
		ctx->m_ui->setColorPickerHandler(&ctx->m_handler);
	}

	ctx->refreshUi();
	ctx->m_ui->show();

	return 1;
}


int PhxShaderSim::setVopPathCB(void *data, int, fpreal64, const PRM_Template *)
{
	auto simNode = reinterpret_cast<PhxShaderSim*>(data);
	simNode->loadDataRanges();
	return 1;
}


PRM_Template* PhxShaderSim::GetPrmTemplate()
{
	if (!AttrItems) {
		static Parm::PRMList paramList;
		paramList.addFromFile(Parm::expandUiPath("CustomPhxShaderSim.ds").c_str());
		AttrItems = paramList.getPRMTemplate();

		// set callbacks for ramp params
		for (int c = 0; c < paramList.size(); ++c) {
			auto & param = AttrItems[c];
			if (!strcmp(param.getToken(), "selectedSopPath")) {
				param.setCallback(setVopPathCB);
			} else if (!strcmp(param.getToken(), "setPresetParam")) {
				param.setCallback(setPresetTypeCB);
			}

			const auto spareData = param.getSparePtr();
			if (spareData) {
				if (spareData->getValue("vray_ramp_type")) {
					param.setCallback(rampButtonClickCB);
				} else if (spareData->getValue("vray_ramp_depend")) {
					param.setCallback(rampDropDownDependCB);
				}
			}
		}
	}

	return AttrItems;
}


PhxShaderSim::PhxShaderSim(OP_Network *parent, const char *name, OP_Operator *entry)
    : NodeBase(parent, name, entry)
{
	const auto count = AttrItems ? PRM_Template::countTemplates(AttrItems) : 0;
	for (int c = 0; c < count; ++c) {
		auto & parm = AttrItems[c];
		const auto spareData = parm.getSparePtr();
		if (spareData) {
			const auto typeString = spareData->getValue("vray_ramp_type");
			const auto mergeRamp = spareData->getValue("vray_ramp_merge");
			const auto token = parm.getToken();
			if (token && typeString) {
				std::shared_ptr<RampContext> ctx;

				// try to find the ramp we should merge with
				// if it is already created - use it's context
				if (mergeRamp) {
					auto mergeCtx = m_ramps.find(mergeRamp);
					if (mergeCtx != m_ramps.end()) {
						ctx = mergeCtx->second;
						// attach ref with our token
						m_ramps[token] = ctx;
					}
				}
				if (!ctx) {
					ctx.reset(new RampContext);
					m_ramps[token] = ctx; 
				}

				if (!strcmp(typeString, "color")) {
					ctx->m_uiType = static_cast<RampType>(ctx->m_uiType | RampType_Color);
					m_rampTypes[token] = RampType_Color;
				} else if (!strcmp(typeString, "curve")) {
					ctx->m_uiType = static_cast<RampType>(ctx->m_uiType | RampType_Curve);
					m_rampTypes[token] = RampType_Curve;
				}
			}
		}
	}

	onLoadSetActiveChannels(false);
	setRampDefaults();
}


void PhxShaderSim::onLoadSetActiveChannels(bool fromUi)
{
	const auto count = AttrItems ? PRM_Template::countTemplates(AttrItems) : 0;
	for (int c = 0; c < count; ++c) {
		auto & parm = AttrItems[c];
		const auto spareData = parm.getSparePtr();
		if (spareData) {
			const char * rampToken = spareData->getValue("vray_ramp_depend");
			if (rampToken) {
				auto ramp = m_ramps.find(rampToken);
				if (ramp != m_ramps.end()) {
					if (!ramp->second) {
						Log::getLog().error("Missing context for \"%s\"!", rampToken);
					} else {
						int idx = -1;
						if (fromUi) {
							idx = evalInt(parm.getToken(), 0, 0);
						} else {
							if (auto factDefaults = parm.getFactoryDefaults()) {
								idx = factDefaults->getOrdinal();
							}
						}
						if (!RampContext::isValidChannel(static_cast<RampContext::RampChannel>(idx))) {
							idx = RampContext::CHANNEL_TEMPERATURE;
						}

						ramp->second->setActiveChannel(static_cast<RampContext::RampChannel>(idx));
					}
				}
			}
		}
	}
}


void PhxShaderSim::finishedLoadingNetwork(bool is_child_call)
{
	onLoadSetActiveChannels(true);
}


bool PhxShaderSim::savePresetContents(ostream &os)
{
	os << SAVE_TOKEN << SAVE_SEPARATOR;
	return saveRamps(os) && VOP_Node::savePresetContents(os);
}


bool PhxShaderSim::loadPresetContents(const char *tok, UT_IStream &is)
{
	if (!strcmp(tok, SAVE_TOKEN)) {
		return loadRamps(is);
	} else {
		return VOP_Node::loadPresetContents(tok, is);
	}
}


OP_ERROR PhxShaderSim::saveIntrinsic(ostream &os, const OP_SaveFlags &sflags)
{
	os << SAVE_TOKEN << SAVE_SEPARATOR;
	saveRamps(os);

	return VOP_Node::saveIntrinsic(os, sflags);
}


bool PhxShaderSim::loadPacket(UT_IStream &is, const char *token, const char *path)
{
	if (VOP_Node::loadPacket(is, token, path)) {
		return true;
	}

	if (!strcmp(token, SAVE_TOKEN)) {
		return loadRamps(is);
	}

	return false;
}

/// save format:
/// @rampCount SEP
/// foreach ramp: @rampToken SEP @rampType SEP @rampActiveChannel SEP
/// foreach channel: @rampPointCount SEP @rampKeys SEP @rampValues SEP @rampInterpolations SEP
bool PhxShaderSim::saveRamps(std::ostream & os)
{
	os << static_cast<int>(m_ramps.size()) << SAVE_SEPARATOR;

	for (const auto & ramp : m_ramps) {
		if (!ramp.second) {
			continue;
		}

		const auto type = m_rampTypes[ramp.first];
		os << ramp.first << SAVE_SEPARATOR; // token
		os << static_cast<int>(type) << SAVE_SEPARATOR; // type

		os << static_cast<int>(ramp.second->getActiveChannel()) << SAVE_SEPARATOR; // active channel

		for (int c = 0; c < RampContext::CHANNEL_COUNT; ++c) {
			const auto & data = ramp.second->m_data[c][RampContext::rampTypeToIdx(type)];
			const int pointCount = data.m_xS.size();

			os << pointCount << SAVE_SEPARATOR; // point count

			for (int c = 0; c < pointCount; ++c) {
				os << data.m_xS[c] << SAVE_SEPARATOR; // keys
			}

			const int components = type == RampType_Curve ? 1 : 3;
			for (int c = 0; c < pointCount * components; ++c) {
				os << data.m_yS[c] << SAVE_SEPARATOR; // values
			}

			if (type == RampType_Curve) {
				for (int c = 0; c < pointCount; ++c) {
					os << static_cast<int>(data.m_interps[c]) << SAVE_SEPARATOR; // interpolations
				}
			}
		}
	}

	return os.good();
}


bool PhxShaderSim::loadRamps(UT_IStream & is)
{
	bool success = true;
	const char * expectedStr= "", * expressionStr = "";

// Try to read some data and check if we read the appropriate amount
#define readSome(declare, expected, expression)                       \
	declare;                                                          \
	if ((expected) != (expression)) {                                 \
		if (success) { /* save exp and expr only on the first error */\
			expectedStr = #expected;                                  \
			expressionStr = #expression;                              \
		}                                                             \
		success = false;                                              \
	}

	readSome(int rampCount, 1, is.read(&rampCount));
	for (int c = 0; c < rampCount && success; ++c) {
		readSome(string rampName, 1, is.read(rampName));

		auto ramp = m_ramps.find(rampName);
		// if we dont have the expected ramp in object we still have to read trogh the data to enable
		// loading of other ramps from the file
		if (ramp == m_ramps.end() || !ramp->second) {
			Log::getLog().error("Ramp name \"%s\" not expected - discarding data!");
		}

		readSome(RampType type, 1, is.read(reinterpret_cast<int*>(&type)));
		readSome(RampContext::RampChannel activeChan, 1, is.read(reinterpret_cast<int*>(&activeChan)));
		if (ramp != m_ramps.end() && ramp->second) {
			ramp->second->setActiveChannel(activeChan);
		}

		for (int r = 0; r < RampContext::CHANNEL_COUNT; ++r) {
			readSome(int pointCount, 1, is.read(&pointCount));

			RampData data;
			data.m_type = type;
			data.m_xS.resize(pointCount);
			data.m_yS.resize(pointCount * (type == RampType_Curve ? 1 : 3));
			data.m_interps.resize(pointCount);

			readSome(, data.m_xS.size(), is.read<fpreal32>(data.m_xS.data(), data.m_xS.size()));
			readSome(, data.m_yS.size(), is.read<fpreal32>(data.m_yS.data(), data.m_yS.size()));

			if (type == RampType_Curve) {
				readSome(, data.m_interps.size(), is.read<int>(reinterpret_cast<int*>(data.m_interps.data()), data.m_interps.size()));
			} else {
				std::fill(data.m_interps.begin(), data.m_interps.end(), AurRamps::MCPT_Linear);
			}

			if (ramp != m_ramps.end() && ramp->second) {
				if (!(ramp->second->m_uiType & type)) {
					Log::getLog().error("Ramp name \"%s\" has unexpected type [%d]- discarding data!", rampName.c_str(), static_cast<int>(type));
				} else {
					ramp->second->m_data[r][RampContext::rampTypeToIdx(type)] = data;
				}
			}
		}

		// this is and error with reading from file - break
		if (!success) {
			break;
		}
	}
#undef readSome

	if (!success) {
		Log::getLog().error("Error reading \"%s\" expecting %s", expressionStr, expectedStr);
		return false;
	} else {
		return true;
	}
}


void PhxShaderSim::setPluginType()
{
	pluginType = VRayPluginType::MATERIAL;
	pluginID   = "PhxShaderSim";
}

OP::VRayNode::PluginResult PhxShaderSim::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	const auto t = exporter.getContext().getTime();

	RenderMode rendMode;

	// renderMode
	rendMode = static_cast<RenderMode>(evalInt("renderMode", 0, t));
	pluginDesc.addAttribute(Attrs::PluginAttr("geommode", rendMode == Volumetric_Geometry || rendMode == Volumetric_Heat_Haze || rendMode == Isosurface));
	pluginDesc.addAttribute(Attrs::PluginAttr("mesher", rendMode == Mesh));
	pluginDesc.addAttribute(Attrs::PluginAttr("rendsolid", rendMode == Isosurface));
	pluginDesc.addAttribute(Attrs::PluginAttr("heathaze", rendMode == Volumetric_Heat_Haze));

	// TODO: find a better way to pass these
	// add these so we know later in what to wrap this sim
	Attrs::PluginAttr attrRendMode("_vray_render_mode", Attrs::PluginAttr::AttrTypeIgnore);
	attrRendMode.paramValue.valInt = static_cast<int>(rendMode);
	pluginDesc.add(attrRendMode);

	// in geom mode, this will be property for the geom plugin generated for the sim
	const bool dynamic_geometry = evalInt("dynamic_geometry", 0, t) == 1;
	Attrs::PluginAttr attrDynGeom("_vray_dynamic_geometry", Attrs::PluginAttr::AttrTypeIgnore);
	attrRendMode.paramValue.valInt = dynamic_geometry;
	pluginDesc.add(attrDynGeom);


	const auto primVal = evalInt("pmprimary", 0, t);
	const bool enableProb = (exporter.isIPR() && primVal) || primVal == 2;
	pluginDesc.addAttribute(Attrs::PluginAttr("pmprimary", enableProb));

	const auto pluginInfo = Parm::GetVRayPluginInfo(pluginDesc.pluginID);
	if (NOT(pluginInfo)) {
		Log::getLog().error("Node \"%s\": Plugin \"%s\" description is not found!",
							this->getName().buffer(), pluginDesc.pluginID.c_str());
	} else {
		// export ramp data
		for (const auto & ramp : m_ramps) {
			if (!ramp.second) {
				continue;
			}
			const auto & rampToken = ramp.first;

			const auto attrIter = pluginInfo->attributes.find(rampToken);
			if (attrIter == pluginInfo->attributes.end()) {
				Log::getLog().error("Node \"%s\": Plugin \"%s\" missing description for \"%s\"",
									this->getName().buffer(), pluginDesc.pluginID.c_str(), rampToken.c_str());
			}
			const auto & attrDesc = attrIter->second;
			const auto & data = ramp.second->data(m_rampTypes[rampToken]);
			const auto pointCount = data.m_xS.size();

			if (data.m_type == AurRamps::RampType_Color) {
				VRay::ColorList colorList(pointCount);
				for (int c = 0; c < pointCount; ++c) {
					colorList[c] = VRay::Color(data.m_yS[c * 3 + 0], data.m_yS[c * 3 + 1], data.m_yS[c * 3 + 2]);
				}

				pluginDesc.add(Attrs::PluginAttr(attrDesc.value.defRamp.positions, data.m_xS));
				pluginDesc.add(Attrs::PluginAttr(attrDesc.value.defRamp.colors, colorList));
				// color interpolations are not supported - export linear
				pluginDesc.add(Attrs::PluginAttr(attrDesc.value.defRamp.interpolations, VRay::IntList(pointCount, static_cast<int>(Texture::VRAY_InterpolationType::Linear))));
			} else if (data.m_type == AurRamps::RampType_Curve) {
				pluginDesc.add(Attrs::PluginAttr(attrDesc.value.defCurve.values, data.m_yS));
				pluginDesc.add(Attrs::PluginAttr(attrDesc.value.defCurve.positions, data.m_xS));

				VRay::IntList interpolations(pointCount);
				// exporter expects ints instead of enums
				memcpy(interpolations.data(), data.m_interps.data(), pointCount * sizeof(int));

				pluginDesc.add(Attrs::PluginAttr(attrDesc.value.defCurve.interpolations, interpolations));
			}
		}
	}

	exporter.setAttrsFromOpNodePrms(pluginDesc, this, "", true);

	return OP::VRayNode::PluginResultContinue;
}

#endif // CGR_HAS_AUR
