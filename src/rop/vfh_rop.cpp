//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_rop.h"
#include "vfh_exporter.h"
#include "vfh_prm_def.h"
#include "vfh_prm_templates.h"
#include "vfh_hou_utils.h"
#include "vfh_gpu_device_select.h"
#include "vfh_vray_cloud.h"

#include <ROP/ROP_Error.h>
#include <OBJ/OBJ_Geometry.h>
#include <OP/OP_Director.h>
#include <OP/OP_BundleList.h>
#include <OP/OP_Bundle.h>

#include <TAKE/TAKE_Manager.h>
#include <UT/UT_Interrupt.h>

using namespace VRayForHoudini;

static const UT_StringRef RT_UPDATE_SPARE_TAG = "rt_update";
static const UT_StringRef apprenticeLimitMsg = "Third-party render engines are not allowed in Houdini Apprentice!";

static Parm::PRMList prmList;

/// Callback for the "Render RT" button on the ROP node.
/// This will start the renderer in IPR mode.
static int RtStartSession(void *data, int /*index*/, fpreal t, const PRM_Template* /*tplate*/)
{
	VRayRendererNode &rop = *reinterpret_cast<VRayRendererNode*>(data);
	rop.startRenderRT(t);
	return 1;
}

/// Callback for the "Show VFB" button on the ROP node.
/// Shows VFB window if there is one.
static int RendererShowVFB(void *data, int /*index*/, fpreal /*t*/, const PRM_Template* /*tplate*/)
{
	VRayRendererNode &rop = *reinterpret_cast<VRayRendererNode*>(data);
	rop.showVFB();
	return 1;
}

/// Callback for the "GPU Devices" button on the ROP node.
/// Shows GPU device select UI.
static int rendererGpuDeviceSelect(void *data, int /*index*/, fpreal /*t*/, const PRM_Template* /*tplate*/)
{
	VRayRendererNode &rop = *reinterpret_cast<VRayRendererNode*>(data);
	showGpuDeviceSelectUI();
	return 1;
}

OP_TemplatePair* VRayRendererNode::getTemplatePair()
{
	static OP_TemplatePair *ropPair = nullptr;
	if (!ropPair) {
		if (prmList.empty()) {
			prmList.addFromFile("vfh_rop.ds");

			PRM_Template *prmTemplate = prmList.getPRMTemplate();

			for (int c = 0; c < prmList.size(); ++c) {
				PRM_Template &param = prmTemplate[c];
				if (vutils_strcmp(param.getToken(), "show_current_vfb") == 0) {
					param.setCallback(RendererShowVFB);
				}
				else if (vutils_strcmp(param.getToken(), "render_rt") == 0) {
					param.setCallback(RtStartSession);
				}
				else if (vutils_strcmp(param.getToken(), "gpu_device_select") == 0) {
					param.setCallback(rendererGpuDeviceSelect);
				}
			}
		}

		ropPair = new OP_TemplatePair(getROPbaseTemplate(), new OP_TemplatePair(prmList.getPRMTemplate()));
	}

	return ropPair;
}

OP_VariablePair* VRayRendererNode::getVariablePair()
{
	static OP_VariablePair *pair = nullptr;
	if (!pair) {
		pair = new OP_VariablePair(ROP_Node::myVariableList);
	}
	return pair;
}

OP_Node* VRayRendererNode::myConstructor(OP_Network *parent, const char *name, OP_Operator *entry)
{
	return new VRayRendererNode(parent, name, entry);
}

VRayRendererNode::VRayRendererNode(OP_Network *net, const char *name, OP_Operator *entry)
	: ROP_Node(net, name, entry)
	, m_exporter(this)
	, m_tstart(0)
	, m_tend(0)
{
	Log::getLog().debug("VRayRendererNode()");
}

VRayRendererNode::~VRayRendererNode()
{
	Log::getLog().debug("~VRayRendererNode()");
}

void VRayRendererNode::onRtTakeChange()
{
	// Apply currently selected take.
	m_exporter.applyTake(nullptr);
}

void VRayRendererNode::RtCallbackRop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	Log::getLog().debug("RtCallbackRop: %s from \"%s\"", OPeventToString(type), caller->getName().buffer());

	VRayRendererNode &rop = *UTverify_cast<VRayRendererNode*>(caller);
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

	switch (type) {
		case OP_PARM_CHANGED: {
			const int prmIdx = static_cast<int>(reinterpret_cast<intptr_t>(data));

			PRM_Parm &prm = caller->getParm(prmIdx);
			if (prm.getSparePtr() &&
				prm.getSparePtr()->getValue(RT_UPDATE_SPARE_TAG))
			{
				rop.startRenderRT(exporter.getContext().getTime());
			}
			else if (vutils_strcmp(prm.getToken(), "take") == 0) {
				rop.onRtTakeChange();
			}
			break;
		}
		case OP_NODE_PREDELETE: {
			exporter.delOpCallbacks(caller);
			break;
		}
		default: {
			break;
		}
	}
}

/// Returns render mode/device from the ROP node depending on the session type.
/// @param ropNode ROP node instance. May be V-Ray IPR asset.
/// @param sessionType Session type.
static VRay::RendererOptions::RenderMode getRenderModeFromROP(const OP_Node &ropNode, VfhSessionType sessionType)
{
	switch (sessionType) {
		case VfhSessionType::ipr:
		case VfhSessionType::rt: {
			return getRendererIprMode(ropNode);
		}
		default:
			return getRendererMode(ropNode);
	}
}

static int isRenderInCloud(OP_Node &rop)
{
	return rop.evalInt("render_in_cloud", 0, 0.0);
}

int VRayRendererNode::initSession(VfhSessionType sessionType, int nframes, fpreal tstart, fpreal tend)
{
	Log::getLog().debug("VRayRendererNode::initSession(%i, %i, %.3ff, %.3f)", sessionType, nframes, tstart, tend);

	ROP_RENDER_CODE error = ROP_ABORT_RENDER;

	// Store end time for endRender() executePostRenderScript()
	m_tstart = tstart;
	m_tend = tend;

	const int hasUI = HOU::isUIAvailable();

	if (!VRayExporter::getCamera(this)) {
		Log::getLog().error("Camera is not set!");
	}
	else {
		// Force production mode for background rendering.
		if (!hasUI) {
			sessionType = VfhSessionType::production;
		}

		if (sessionType == VfhSessionType::production) {
			if (isRenderInCloud(*this)) {
				sessionType = VfhSessionType::cloud;
			}
		}

		if (sessionType == VfhSessionType::cloud) {
			if (!Cloud::isClientAvailable()) {
				return ROP_ABORT_RENDER;
			}
		}

		if (m_exporter.initRenderer(hasUI, false)) {
			const VRay::RendererOptions::RenderMode renderMode =
				getRenderModeFromROP(*this, sessionType);

			m_exporter.setRopPtr(this);

			m_exporter.setSessionType(sessionType);
			m_exporter.setExportMode(sessionType == VfhSessionType::cloud ? VRayExporter::ExpExport : getExportMode(*this));
			m_exporter.setRenderMode(renderMode);
			m_exporter.setDRSettings();

			m_exporter.initExporter(getFrameBufferType(*this), nframes, tstart, tend);

			// SOHO IPR handles this differently for now.
			if (sessionType == VfhSessionType::rt) {
				m_exporter.addOpCallback(this, RtCallbackRop);
			}

			error = m_exporter.getError();
		}
	}

	return error;
}

void VRayRendererNode::showVFB()
{
	if (isBackground())
		return;
	m_exporter.showVFB();
}

TAKE_Take* VRayRendererNode::applyTake(const char *take)
{
	if (!UTisstring(take))
		return nullptr;
	return applyRenderTake(take);
}

void VRayRendererNode::restoreTake(TAKE_Take *take)
{
	if (!take)
		return;
	restorePreviousTake(take);
}

void VRayRendererNode::startRenderRT(fpreal time)
{
	if (HOU::isApprentice()) {
		Log::getLog().error(apprenticeLimitMsg);
		addError(ROP_NONCOMMERCIAL_ERROR, apprenticeLimitMsg);
		return;
	}

	Log::getLog().debug("VRayRendererNode::startRenderRT(time = %.3f)",
						time);

	if (initSession(VfhSessionType::rt, 1, time, time)) {
		m_exporter.exportFrame(time);
	}
}

int VRayRendererNode::startRender(int nframes, fpreal tstart, fpreal tend)
{
	if (HOU::isApprentice()) {
		Log::getLog().error(apprenticeLimitMsg);
		addError(ROP_NONCOMMERCIAL_ERROR, apprenticeLimitMsg);
		return ROP_ABORT_RENDER;
	}

	Log::getLog().debug("VRayRendererNode::startRender(nframes = %i, tstart = %.3f, tend = %.3f)",
						nframes, tstart, tend);

	executePreRenderScript(tstart);

	return initSession(VfhSessionType::production, nframes, tstart, tend);
}

ROP_RENDER_CODE VRayRendererNode::renderFrame(fpreal time, UT_Interrupt*)
{
	Log::getLog().debug("VRayRendererNode::renderFrame(time = %.3f)", time);

	executePreFrameScript(time);

	m_exporter.exportFrame(time);

	executePostFrameScript(time);

	return m_exporter.getError();
}

ROP_RENDER_CODE VRayRendererNode::endRender()
{
	Log::getLog().debug("VRayRendererNode::endRender()");

	m_exporter.exportEnd();

	executePostRenderScript(m_tend);

	return ROP_CONTINUE_RENDER;
}

void VRayRendererNode::register_operator(OP_OperatorTable *table)
{
	OP_Operator *rop = new OP_Operator(/* Internal name     */ "vray_renderer",
									   /* UI name           */ "V-Ray Renderer",
									   /* How to create one */ myConstructor,
									   /* Parm definitions  */ getTemplatePair(),
									   /* Min # of inputs   */ 0,
									   /* Max # of inputs   */ 256,
									   /* Var definitions   */ getVariablePair(),
									   /* OP flags          */ OP_FLAG_GENERATOR);

	// Set icon
	rop->setIconName("ROP_vray");

	table->addOperator(rop);
}
