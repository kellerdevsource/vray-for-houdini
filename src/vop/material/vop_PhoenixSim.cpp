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

void PhxShaderSim::RampHandler::OnEditCurveDiagram(RampUi & curve, OnEditType editReason) {
	if (editReason == OnEdit_ChangeBegin || editReason == OnEdit_ChangeInProgress) {
		return;
	}

	const auto size = curve.pointCount(RampType_Curve);

	m_Data.xS.resize(size);
	m_Data.yS.reserve(size);
	m_Data.interps.resize(size);

	const auto newSize = curve.getCurvePoints(m_Data.xS.data(), m_Data.yS.data(), m_Data.interps.data(), size);

	// if we got less points resize down
	if (newSize != size) {
		m_Data.xS.resize(newSize);
		m_Data.yS.reserve(newSize);
		m_Data.interps.resize(newSize);
	}
}

void PhxShaderSim::RampHandler::OnEditColorGradient(RampUi & curve, OnEditType editReason) {
	// NOTE: m_Data.yS is of color type so it's 3 floats per point!

	if (editReason == OnEdit_ChangeBegin || editReason == OnEdit_ChangeInProgress) {
		return;
	}

	const auto size = curve.pointCount(RampType_Color);

	m_Data.xS.resize(size);
	m_Data.yS.reserve(size * 3);

	const auto newSize = curve.getColorPoints(m_Data.xS.data(), m_Data.yS.data(), size);

	// if we got less points resize down
	if (newSize != size) {
		m_Data.xS.resize(newSize);
		m_Data.yS.reserve(newSize * 3);
	}
}

int rampButtonClickCB(void *data, int index, fpreal64 time, const PRM_Template *tplate)
{
	using namespace std;
	using namespace AurRamps;

	const string token = tplate->getToken();

	auto simNode = reinterpret_cast<PhxShaderSim*>(data);
	auto & rampData = simNode->m_Ramps[token];

	const auto spareData = tplate->getSparePtr();
	const auto typeString = spareData ? spareData->getValue("vray_ramp_type") : "none";
	const auto type = !strcmp(typeString ? typeString : "", "curve") ? RampType_Curve : RampType_Color;

	auto ui = RampUi::createRamp(tplate->getLabel(), type, 200, 200, 300, 500, getWxWidgetsGUI(GetCurrentProcess()));

	if (!rampData.xS.empty()) {
		if (type == RampType_Curve) {
			ui->setCurvePoints(rampData.xS.data(), rampData.yS.data(), rampData.interps.data(), rampData.xS.size());
		} else {
			// NOTE: here rampData.yS is color type which is 3 floats per point so actual count is rampData.xS.size() !!
			ui->setColorPoints(rampData.xS.data(), rampData.yS.data(), rampData.xS.size());
		}
	}

	auto handlerIter = simNode->m_RampHandlers.find(token);
	if (handlerIter != simNode->m_RampHandlers.end()) {
		delete handlerIter->second;
	}

	ui->setChangeHandler(simNode->m_RampHandlers[token] = new PhxShaderSim::RampHandler(rampData));
	ui->show();

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
	m_Ramps["elum_curve"] = RampData();
	m_Ramps["ecolor_ramp"] = RampData();
	m_Ramps["dcolor_ramp"] = RampData();
	m_Ramps["transp_curve"] = RampData();
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
