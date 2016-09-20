//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <vop_PhoenixSim.h>
#include <vfh_prm_templates.h>
#include <vfh_tex_utils.h>

#include <UT/UT_IStream.h>
#include <OP/OP_SaveFlags.h>
#include <PRM/PRM_RampUtils.h>
#include <HOM/HOM_Ramp.h>
#include <CH/CH_Channel.h>

#include <utility>

using namespace AurRamps;

using namespace VRayForHoudini;
using namespace VOP;
using namespace std;

static PRM_Template * AttrItems = nullptr;
static const char * SAVE_SEPARATOR = "\n";
static const char * SAVE_TOKEN = "phx_ramp_data";

namespace {

int rampButtonClickCB(void *data, int index, fpreal64 time, const PRM_Template *tplate)
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
	auto & ctx = simNode->m_Ramps[token];

	// there is already a window
	if (ctx.m_Ui) {
		ctx.m_Ui->show();
		return 1;
	}

	ctx.m_Ui = RampUi::createRamp(tplate->getLabel(), ctx.m_Type, 200, 200, 300, 500, app);

	if (!ctx.m_Data.xS.empty()) {
		if (ctx.m_Type == RampType_Curve) {
			ctx.m_Ui->setCurvePoints(ctx.m_Data.xS.data(), ctx.m_Data.yS.data(), ctx.m_Data.interps.data(), ctx.m_Data.xS.size());
		} else {
			// NOTE: here rampData.yS is color type which is 3 floats per point so actual count is rampData.xS.size() !!
			ctx.m_Ui->setColorPoints(ctx.m_Data.xS.data(), ctx.m_Data.yS.data(), ctx.m_Data.xS.size());
		}
	}

	ctx.m_Handler = PhxShaderSim::RampHandler(&ctx);
	ctx.m_Ui->setChangeHandler(&ctx.m_Handler);
	if (ctx.m_Type == RampType_Color) {
		ctx.m_Ui->setColorPickerHandler(&ctx.m_Handler);
	}

	ctx.m_Ui->show();

	return 1;
}

}

void PhxShaderSim::RampHandler::OnEditCurveDiagram(RampUi & curve, OnEditType editReason)
{
	if (!m_Ctx || editReason == OnEdit_ChangeBegin || editReason == OnEdit_ChangeInProgress) {
		return;
	}

	const auto size = curve.pointCount(RampType_Curve);

	m_Ctx->m_Data.xS.resize(size);
	m_Ctx->m_Data.yS.resize(size);
	m_Ctx->m_Data.interps.resize(size);

	const auto newSize = curve.getCurvePoints(m_Ctx->m_Data.xS.data(), m_Ctx->m_Data.yS.data(), m_Ctx->m_Data.interps.data(), size);

	// if we got less points resize down
	if (newSize != size) {
		m_Ctx->m_Data.xS.resize(newSize);
		m_Ctx->m_Data.yS.resize(newSize);
		m_Ctx->m_Data.interps.resize(newSize);
	}
}

void PhxShaderSim::RampHandler::OnEditColorGradient(RampUi & curve, OnEditType editReason)
{
	// NOTE: m_Data.yS is of color type so it's 3 floats per point!

	if (!m_Ctx || editReason == OnEdit_ChangeBegin || editReason == OnEdit_ChangeInProgress) {
		return;
	}

	const auto size = curve.pointCount(RampType_Color);

	m_Ctx->m_Data.xS.resize(size);
	m_Ctx->m_Data.yS.resize(size * 3);

	const auto newSize = curve.getColorPoints(m_Ctx->m_Data.xS.data(), m_Ctx->m_Data.yS.data(), size);

	// if we got less points resize down
	if (newSize != size) {
		m_Ctx->m_Data.xS.resize(newSize);
		m_Ctx->m_Data.yS.resize(newSize * 3);
	}
}

void PhxShaderSim::RampHandler::OnWindowDie()
{
	if (m_Ctx) {
		m_Ctx->m_Ui = nullptr;
	}
}

void PhxShaderSim::RampHandler::Create(AurRamps::RampUi & curve, float prefered[3])
{
	float result[3];
	bool canceled = curve.defaultColorPicker(prefered, result);
	curve.setSelectedPointsColor(result, canceled);
}


PRM_Template* PhxShaderSim::GetPrmTemplate()
{
	if (!AttrItems) {
		static Parm::PRMList paramList;
		paramList.addFromFile(Parm::PRMList::expandUiPath("CustomPhxShaderSim.ds"));
		AttrItems = paramList.getPRMTemplate();

		for (int c = 0; c < paramList.size(); ++c) {
			auto & param = AttrItems[c];
			const auto spareData = param.getSparePtr();
			if (spareData && spareData->getValue("vray_ramp_type")) {
				param.setCallback(rampButtonClickCB);
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
			const auto token = parm.getToken();
			if (token && typeString) {
				const auto type = !strcmp(typeString ? typeString : "", "curve") ? RampType_Curve : RampType_Color;
				m_Ramps[token] = RampContext(type);
			}
		}
	}
}


bool PhxShaderSim::savePresetContents(ostream &os)
{
	os << SAVE_TOKEN << SAVE_SEPARATOR;
	return saveRamps(os) && OP_Node::savePresetContents(os);
}


bool PhxShaderSim::loadPresetContents(const char *tok, UT_IStream &is)
{
	if (!strcmp(tok, SAVE_TOKEN)) {
		return loadRamps(is);
	} else {
		return OP_Node::loadPresetContents(tok, is);
	}
}


OP_ERROR PhxShaderSim::saveIntrinsic(ostream &os, const OP_SaveFlags &sflags)
{
	os << SAVE_TOKEN << SAVE_SEPARATOR;
	saveRamps(os);

    return OP_Node::saveIntrinsic(os, sflags);
}


bool PhxShaderSim::loadPacket(UT_IStream &is, const char *token, const char *path)
{
    if (OP_Node::loadPacket(is, token, path)) {
		return true;
	}

	if (!strcmp(token, SAVE_TOKEN)) {
		return loadRamps(is);
	}

	return false;
}


bool PhxShaderSim::saveRamps(std::ostream & os)
{
	os << static_cast<int>(m_Ramps.size()) << SAVE_SEPARATOR;

	for (const auto & ramp : m_Ramps) {
		const auto & data = ramp.second.m_Data;
		const int count = data.xS.size();
		const auto type = ramp.second.m_Type;
		os << ramp.first << SAVE_SEPARATOR;
		os << static_cast<int>(ramp.second.m_Type) << SAVE_SEPARATOR;
		os << count << SAVE_SEPARATOR;

		for (int c = 0; c < count; ++c) {
			os << data.xS[c] << SAVE_SEPARATOR;
		}

		const int components = type == RampType_Curve ? 1 : 3;
		for (int c = 0; c < count * components; ++c) {
			os << data.yS[c] << SAVE_SEPARATOR;
		}

		if (type == RampType_Curve) {
			for (int c = 0; c < count; ++c) {
				os << static_cast<int>(data.interps[c]) << SAVE_SEPARATOR;
			}
		}
	}

	return os;
}

bool PhxShaderSim::loadRamps(UT_IStream & is)
{
	bool success = true;
	const char * exp = "", * expr = "";

#define readSome(declare, expected, expression)\
	declare;\
	if ((expected) != (expression)) {\
		success = false;\
		exp = #expected;\
		expr = #expression;\
	}

	readSome(int rampCount, 1, is.read(&rampCount));
	for (int c = 0; c < rampCount && success; ++c) {
		readSome(string rampName, 1, is.read(rampName));
		readSome(int readType, 1, is.read(&readType));
		const auto type = static_cast<RampType>(readType);
		readSome(int pointCount, 1, is.read(&pointCount));

		RampData data;
		data.xS.resize(pointCount);
		data.yS.resize(pointCount * (type == RampType_Curve ? 1 : 3));
		data.interps.resize(pointCount);

		readSome(, data.xS.size(), is.read<fpreal32>(data.xS.data(), data.xS.size()));
		readSome(, data.yS.size(), is.read<fpreal32>(data.yS.data(), data.yS.size()));

		if (type == RampType_Curve) {
			readSome(, data.interps.size(), is.read<int>(reinterpret_cast<int*>(data.interps.data()), data.interps.size()));
		} else {
			std::fill(data.interps.begin(), data.interps.end(), MCPT_Linear);
		}

		if (!success) {
			break;
		}

		auto ramp = m_Ramps.find(rampName);
		if (ramp != m_Ramps.end()) {
			ramp->second.m_Data = data;
			ramp->second.m_Type = type;
		} else {
			Log::getLog().error("Ramp name \"%s\" not expected - discarding data!");
		}
	}
#undef readSome

	if (!success) {
		Log::getLog().error("Error reading \"%s\" expecting %s", exp, expr);
		return false;
	} else {
		return true;
	}
}


void PhxShaderSim::setPluginType()
{
	pluginType = "MATERIAL";
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
		for (const auto & ramp : m_Ramps) {
			const auto & rampToken = ramp.first;

			const auto attrIter = pluginInfo->attributes.find(rampToken);
			if (attrIter == pluginInfo->attributes.end()) {
				Log::getLog().error("Node \"%s\": Plugin \"%s\" missing description for \"%s\"",
									this->getName().buffer(), pluginDesc.pluginID.c_str(), rampToken.c_str());
			}
			const auto & attrDesc = attrIter->second;
			const auto & data = ramp.second.m_Data;
			const auto pointCount = data.xS.size();

			if (ramp.second.m_Type == AurRamps::RampType_Color) {
				VRay::ColorList colorList(pointCount);
				for (int c = 0; c < pointCount; ++c) {
					colorList[c] = VRay::Color(data.yS[c * 3 + 0], data.yS[c * 3 + 1], data.yS[c * 3 + 2]);
				}

				pluginDesc.add(Attrs::PluginAttr(attrDesc.value.defRamp.positions, data.xS));
				pluginDesc.add(Attrs::PluginAttr(attrDesc.value.defRamp.colors, colorList));
				// color interpolations are not supported - export linear
				pluginDesc.add(Attrs::PluginAttr(attrDesc.value.defRamp.interpolations, VRay::IntList(pointCount, static_cast<int>(Texture::VRAY_InterpolationType::Linear))));
			} else if (ramp.second.m_Type == AurRamps::RampType_Curve) {
				pluginDesc.add(Attrs::PluginAttr(attrDesc.value.defCurve.values, data.yS));
				pluginDesc.add(Attrs::PluginAttr(attrDesc.value.defCurve.positions, data.xS));

				VRay::IntList interpolations(pointCount);
				// exporter expects ints instead of enums
				memcpy(interpolations.data(), data.interps.data(), pointCount * sizeof(int));

				pluginDesc.add(Attrs::PluginAttr(attrDesc.value.defCurve.interpolations, interpolations));
			}
		}
	}

	exporter.setAttrsFromOpNodePrms(pluginDesc, this, "", true);

	return OP::VRayNode::PluginResultContinue;
}

