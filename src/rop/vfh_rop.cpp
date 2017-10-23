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
#include "vfh_prm_globals.h"
#include "vfh_prm_def.h"
#include "vfh_prm_templates.h"
#include "vfh_hou_utils.h"

#include <ROP/ROP_Templates.h>
#include <ROP/ROP_Error.h>
#include <OBJ/OBJ_Geometry.h>
#include <OBJ/OBJ_Light.h>
#include <OP/OP_Director.h>
#include <OP/OP_BundleList.h>
#include <OP/OP_Bundle.h>
#include <UT/UT_Interrupt.h>

using namespace VRayForHoudini;

static const UT_StringRef RT_UPDATE_SPARE_TAG = "rt_update";
static const UT_StringRef apprenticeLimitMsg = "Third-party render engines are not allowed in Houdini Apprentice!";

static Parm::PRMList prmList;

OP_TemplatePair* VRayRendererNode::getTemplatePair()
{
	static OP_TemplatePair *ropPair = nullptr;
	if (!ropPair) {
		if (prmList.empty()) {
			UT_String uiPath = getenv("VRAY_UI_DS_PATH");
			uiPath += "/rop";
			prmList.addFromFile(Parm::expandUiPath("vfh_rop.ds").c_str(), uiPath.buffer());

			PRM_Template *prmTemplate = prmList.getPRMTemplate();

			for (int c = 0; c < prmList.size(); ++c) {
				PRM_Template &param = prmTemplate[c];
				if (vutils_strcmp(param.getToken(), "show_current_vfb") == 0) {
					param.setCallback(RendererShowVFB);
				}
				else if (vutils_strcmp(param.getToken(), "render_rt") == 0) {
					param.setCallback(RtStartSession);
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

bool VRayRendererNode::updateParmsFlags()
{
	bool changed = ROP_Node::updateParmsFlags();
	return changed;
}

void VRayRendererNode::RtCallbackRop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	Log::getLog().debug("RtCallbackRop: %s from \"%s\"", OPeventToString(type), caller->getName().buffer());

	VRayExporter &exporter = *reinterpret_cast< VRayExporter* >(callee);
	VRayRendererNode &rop = *UTverify_cast< VRayRendererNode* >(caller);

	switch (type) {
		case OP_PARM_CHANGED: {
			const long prmIdx = reinterpret_cast<intptr_t>(data);
			PRM_Parm &prm = caller->getParm(prmIdx);
			if (   prm.getSparePtr()
				&& prm.getSparePtr()->getValue(RT_UPDATE_SPARE_TAG))
			{
				rop.startIPR(exporter.getContext().getTime());
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

int VRayRendererNode::RtStartSession(void *data, int /*index*/, fpreal t, const PRM_Template* /*tplate*/)
{
	VRayRendererNode &rop = *reinterpret_cast<VRayRendererNode*>(data);
	rop.startIPR(t);
	return 1;
}

int VRayRendererNode::RendererShowVFB(void *data, int /*index*/, fpreal /*t*/, const PRM_Template* /*tplate*/) {
	VRayRendererNode &rop = *reinterpret_cast<VRayRendererNode*>(data);
	if (!isBackground()) {
		rop.m_exporter.showVFB();
	}
	return 1;
}

int VRayRendererNode::initSession(int interactive, int nframes, fpreal tstart, fpreal tend)
{
	Log::getLog().debug("VRayRendererNode::initSession(%i, %i, %.3ff, %.3f)", interactive, nframes, tstart, tend);

	ROP_RENDER_CODE error = ROP_ABORT_RENDER;

	if (!VRayExporter::getCamera(this)) {
		Log::getLog().error("Camera is not set!");
	}
	else {
		// Store end time for endRender() executePostRenderScript()
		m_tstart = tstart;
		m_tend = tend;

		executePreRenderScript(tstart);

		// Renderer mode (CPU / GPU)
		const int rendererMode = interactive
								 ? getRendererIprMode(*this)
								 : getRendererMode(*this);

		// Interactive mode
		const int wasRT = m_exporter.isIPR();
		const int isRT  = (!isBackground() && interactive);

		// Rendering device
		const int wasGPU = m_exporter.isGPU();
		const int isGPU  = (rendererMode > VRay::RendererOptions::RENDER_MODE_RT_CPU);

		// Whether to re-create V-Ray renderer
		const int reCreate = (wasRT != isRT) || (wasGPU != isGPU);

		m_exporter.setIPR(isRT);

		if (m_exporter.initRenderer(!isBackground(), reCreate)) {
			// Add RT update callbacks to detect scene export changes.
			m_exporter.addOpCallback(this, RtCallbackRop);

			m_exporter.setRendererMode(rendererMode);
			m_exporter.setDRSettings();

			m_exporter.setWorkMode(getExporterWorkMode(*this));
			m_exporter.initExporter(getFrameBufferType(*this), nframes, tstart, tend);

			m_exporter.exportSettings();

			error = m_exporter.getError();
		}
	}

	return error;
}

void VRayRendererNode::startIPR(fpreal time)
{
	if (HOU::isApprentice()) {
		Log::getLog().error(apprenticeLimitMsg);
		addError(ROP_NONCOMMERCIAL_ERROR, apprenticeLimitMsg);
		return;
	}

	if (initSession(true, 1, time, time)) {
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

	Log::getLog().debug("VRayRendererNode::startRender(%i, %.3f, %.3f)", nframes, tstart, tend);

	int err = initSession(false, nframes, tstart, tend);
	return err;
}

ROP_RENDER_CODE VRayRendererNode::renderFrame(fpreal time, UT_Interrupt *boss)
{
	Log::getLog().debug("VRayRendererNode::renderFrame(%.3f)", time);

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
									   /* How to create one */ VRayRendererNode::myConstructor,
									   /* Parm definitions  */ VRayRendererNode::getTemplatePair(),
									   /* Min # of inputs   */ 0,
									   /* Max # of inputs   */ 256,
									   /* Var definitions   */ VRayRendererNode::getVariablePair(),
									   /* OP flags          */ OP_FLAG_GENERATOR);

	// Set icon
	rop->setIconName("ROP_vray");

	table->addOperator(rop);
}
