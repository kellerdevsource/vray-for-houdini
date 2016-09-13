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
#include <vop_PhoenixSim.h>
#include <vfh_prm_templates.h>

#include <utility>

using namespace AurRamps;

using namespace VRayForHoudini;
using namespace VOP;

static PRM_Template * AttrItems = nullptr;

void PhxShaderSim::RampHandler::OnEditCurveDiagram(RampUi & curve, OnEditType editReason)
{
	if (!m_Ctx || editReason == OnEdit_ChangeBegin || editReason == OnEdit_ChangeInProgress) {
		return;
	}

	const auto size = curve.pointCount(RampType_Curve);

	m_Ctx->m_Data.xS.resize(size);
	m_Ctx->m_Data.yS.reserve(size);
	m_Ctx->m_Data.interps.resize(size);

	const auto newSize = curve.getCurvePoints(m_Ctx->m_Data.xS.data(), m_Ctx->m_Data.yS.data(), m_Ctx->m_Data.interps.data(), size);

	// if we got less points resize down
	if (newSize != size) {
		m_Ctx->m_Data.xS.resize(newSize);
		m_Ctx->m_Data.yS.reserve(newSize);
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
	m_Ctx->m_Data.yS.reserve(size * 3);

	const auto newSize = curve.getColorPoints(m_Ctx->m_Data.xS.data(), m_Ctx->m_Data.yS.data(), size);

	// if we got less points resize down
	if (newSize != size) {
		m_Ctx->m_Data.xS.resize(newSize);
		m_Ctx->m_Data.yS.reserve(newSize * 3);
	}
}

void PhxShaderSim::RampHandler::OnWindowDie()
{
	if (m_Ctx) {
		m_Ctx->m_Ui = nullptr;
	}
}


int rampButtonClickCB(void *data, int index, fpreal64 time, const PRM_Template *tplate)
{
	using namespace std;
	using namespace AurRamps;

	const string token = tplate->getToken();

	auto simNode = reinterpret_cast<PhxShaderSim*>(data);
	auto & ctx = simNode->m_Ramps[token];

	// there is already a window
	if (ctx.m_Ui) {
		ctx.m_Ui->show();
		return 1;
	}

	ctx.m_Ui = RampUi::createRamp(tplate->getLabel(), ctx.m_Type, 200, 200, 300, 500, getWxWidgetsGUI(GetCurrentProcess()));

	if (!ctx.m_Data.xS.empty()) {
		if (ctx.m_Type == RampType_Curve) {
			ctx.m_Ui->setCurvePoints(ctx.m_Data.xS.data(), ctx.m_Data.yS.data(), ctx.m_Data.interps.data(), ctx.m_Data.xS.size());
		} else {
			// NOTE: here rampData.yS is color type which is 3 floats per point so actual count is rampData.xS.size() !!
			ctx.m_Ui->setColorPoints(ctx.m_Data.xS.data(), ctx.m_Data.yS.data(), ctx.m_Data.xS.size());
		}
	}

	ctx.m_Hander = PhxShaderSim::RampHandler(&ctx);
	ctx.m_Ui->setChangeHandler(&ctx.m_Hander);
	ctx.m_Ui->show();

	return 1;
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
		const auto spareData = AttrItems[c].getSparePtr();
		if (spareData) {
			const auto typeString = spareData->getValue("vray_ramp_type");
			const auto token = AttrItems[c].getToken();
			if (token && typeString) {
				const auto type = !strcmp(typeString ? typeString : "", "curve") ? RampType_Curve : RampType_Color;
				m_Ramps[token] = RampContext(type);
			}
		}
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

	exporter.setAttrsFromOpNodePrms(pluginDesc, this, "", true);

	return OP::VRayNode::PluginResultContinue;
}

#endif // CGR_HAS_AUR
