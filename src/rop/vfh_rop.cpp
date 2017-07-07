//
// Copyright (c) 2015-2016, Chaos Software Ltd
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

static int getRendererMode(OP_Node &rop)
{
	int renderMode = rop.evalInt("render_render_mode", 0, 0.0);
	switch (renderMode) {
		case 0: renderMode = -1; break; // Production CPU
		case 1: renderMode =  1; break; // RT GPU (OpenCL)
		case 2: renderMode =  4; break; // RT GPU (CUDA)
		default: renderMode = -1; break;
	}
	return renderMode;
}

static int getRendererIprMode(OP_Node &rop)
{
	int renderMode = rop.evalInt("render_rt_mode", 0, 0.0);
	switch (renderMode) {
		case 0: renderMode =  0; break; // RT CPU
		case 1: renderMode =  1; break; // RT GPU (OpenCL)
		case 2: renderMode =  4; break; // RT GPU (CUDA)
		default: renderMode = 0; break;
	}
	return renderMode;
}

static VRayExporter::ExpWorkMode getExporterWorkMode(OP_Node &rop)
{
	return static_cast<VRayExporter::ExpWorkMode>(rop.evalInt("render_export_mode", 0, 0.0));
}

static int isBackground()
{
	return NOT(HOU::isUIAvailable());
}

static int getFrameBufferType(OP_Node &rop)
{
	return isBackground() ? -1 : 0;
}

VRayRendererNode::VRayRendererNode(OP_Network *net, const char *name, OP_Operator *entry)
	: ROP_Node(net, name, entry)
	, m_exporter(this)
{
	Log::getLog().debug("VRayRendererNode()");

	m_activeLightsBundleName.itoa(getUniqueId());
	m_activeLightsBundleName.prepend("V-RayROPLights_");

	m_activeGeoBundleName.itoa(getUniqueId());
	m_activeGeoBundleName.prepend("V-RayROPGeo_");

	m_forcedGeoBundleName.itoa(getUniqueId());
	m_forcedGeoBundleName.prepend("V-RayROPForcedGeo_");
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
		case OP_PARM_CHANGED:
		{
			const long prmIdx = reinterpret_cast<intptr_t>(data);
			PRM_Parm &prm = caller->getParm(prmIdx);
			if (   prm.getSparePtr()
				&& prm.getSparePtr()->getValue(RT_UPDATE_SPARE_TAG))
			{
				rop.startIPR(exporter.getContext().getTime());
			}
			break;
		}
		case OP_NODE_PREDELETE:
		{
			exporter.delOpCallbacks(caller);
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


OP_Bundle* getBundleFromOpNodePrm(OP_Node *node, const char *pn, fpreal time)
{
	if (!node){
		return nullptr;
	}

	if (!UTisstring(pn)) {
		return nullptr;
	}

	UT_String mask;
	PRM_Parm *prm = nullptr;
	node->evalParameterOrProperty(pn, 0, time, mask, &prm);

	OP_Network *opcreator = nullptr;
	const char *opfilter = nullptr;
	if (prm && prm->getSparePtr()) {
		const PRM_SpareData	&prmSpareData = *prm->getSparePtr();

		opcreator = UTverify_cast<OP_Network*>(getOpNodeFromPath(prmSpareData.getOpRelative()));
		opfilter = prmSpareData.getOpFilter();
	}
	if (!opcreator) {
		opcreator = node->getCreator();
	}

	UT_String bundleName;
	bundleName.itoa(node->getUniqueId());
	bundleName.prepend(pn);
	OP_Bundle *bundle = OPgetDirector()->getBundles()->getPattern(bundleName,
																  opcreator,
																  opcreator,
																  mask,
																  opfilter,
																  0,
																  false,
																  0);

	return bundle;
}


OP_Bundle* VRayRendererNode::getActiveLightsBundle()
{
	// if "sololight" parm is set ignore others
	OP_Bundle *sbundle = getBundleFromOpNodePrm(this, "sololight", m_tstart);
	if (sbundle && sbundle->entries() > 0) {
		return sbundle;
	}

	OP_BundleList *blist = OPgetDirector()->getBundles();
	OP_Bundle *bundle = blist->getBundle(m_activeLightsBundleName);
	if (!bundle) {
		bundle = blist->createBundle(m_activeLightsBundleName, true);
	}

	if (!bundle) {
		return bundle;
	}

	bundle->clear();

	OP_Bundle *fbundle = getBundleFromOpNodePrm(this, "forcelights", m_tstart);
	if (fbundle) {
		OP_NodeList list;
		fbundle->getMembers(list);
		bundle->addOpList(list);
	}

	OP_Bundle *abundle = getBundleFromOpNodePrm(this, "alights", m_tstart);
	if (abundle) {
		OP_NodeList list;
		abundle->getMembers(list);
		for (exint i = 0; i < list.size(); ++i) {
			OBJ_Node *light = list(i)->castToOBJNode();
			if (light &&
				light->isObjectRenderable(m_tstart) &&
				light->getVisible())
			{
				UT_StringHolder name = light->getFullPath();

				int enabled = 0;
				light->evalParameterOrProperty("enabled", 0, m_tstart, enabled);
				if (enabled > 0) {
					bundle->addOp(light);
				}
			}
		}
	}

	OP_Bundle *exbundle = getBundleFromOpNodePrm(this, "excludelights", m_tstart);
	if (exbundle) {
		OP_NodeList list;
		exbundle->getMembers(list);
		for (exint i = 0; i < list.size(); ++i) {
			OP_Node *light = list(i);
			UT_StringHolder name = light->getFullPath();

			bundle->removeOp(light);
		}
	}

	return bundle;
}


OP_Bundle* VRayRendererNode::getForcedLightsBundle()
{
	// if "sololight" parm is set ignore others
	OP_Bundle *sbundle = getBundleFromOpNodePrm(this, "sololight", m_tstart);
	if (sbundle && sbundle->entries() > 0) {
		return sbundle;
	}

	OP_Bundle *fbundle = getBundleFromOpNodePrm(this, "forcelights", m_tstart);
	return fbundle;
}


OP_Bundle* VRayRendererNode::getActiveGeometryBundle()
{
	OP_BundleList *blist = OPgetDirector()->getBundles();
	OP_Bundle *bundle = blist->getBundle(m_activeGeoBundleName);
	if (!bundle) {
		bundle = blist->createBundle(m_activeGeoBundleName, true);
	}

	if (!bundle) {
		return bundle;
	}

	bundle->clear();

	OP_Bundle *fbundle = getForcedGeometryBundle();
	if (   fbundle
		&& fbundle->entries() > 0)
	{
		OP_NodeList list;
		fbundle->getMembers(list);
		bundle->addOpList(list);
	}

	OP_Bundle *vbundle = getBundleFromOpNodePrm(this, "vobject", m_tstart);
	if (vbundle) {
		OP_NodeList list;
		vbundle->getMembers(list);

		for (exint i = 0; i < list.size(); ++i) {
			OBJ_Node *node = list(i)->castToOBJNode();
			if (   node
				&& node->isObjectRenderable(m_tstart)
				&& node->getVisible() )
			{
				UT_StringHolder name = node->getFullPath();

				bundle->addOp(node);
			}
		}
	}

	OP_Bundle *exbundle = getBundleFromOpNodePrm(this, "excludeobject", m_tstart);
	if (exbundle) {
		OP_NodeList list;
		exbundle->getMembers(list);
		for (exint i = 0; i < list.size(); ++i) {
			OP_Node *node = list(i);
			UT_StringHolder name = node->getFullPath();

			bundle->removeOp(node);
		}
	}

	return bundle;
}


OP_Bundle* VRayRendererNode::getForcedGeometryBundle()
{
	OP_BundleList *blist = OPgetDirector()->getBundles();
	OP_Bundle *bundle = blist->getBundle(m_forcedGeoBundleName);
	if (!bundle) {
		bundle = blist->createBundle(m_forcedGeoBundleName, true);
	}

	if (!bundle) {
		return bundle;
	}

	bundle->clear();

	OP_Bundle *fbundle = getBundleFromOpNodePrm(this, "forceobject", m_tstart);
	if (   fbundle
		&& fbundle->entries() > 0)
	{
		OP_NodeList list;
		fbundle->getMembers(list);
		bundle->addOpList(list);
	}

	fbundle = getMatteGeometryBundle();
	if (   fbundle
		&& fbundle->entries() > 0)
	{
		OP_NodeList list;
		fbundle->getMembers(list);
		bundle->addOpList(list);
	}

	fbundle = getPhantomGeometryBundle();
	if (   fbundle
		&& fbundle->entries() > 0)
	{
		OP_NodeList list;
		fbundle->getMembers(list);
		bundle->addOpList(list);
	}

	return bundle;
}


OP_Bundle* VRayRendererNode::getMatteGeometryBundle()
{
	OP_Bundle *bundle = getBundleFromOpNodePrm(this, "matte_objects", m_tstart);
	return bundle;
}


OP_Bundle* VRayRendererNode::getPhantomGeometryBundle()
{
	OP_Bundle *bundle = getBundleFromOpNodePrm(this, "phantom_objects", m_tstart);
	return bundle;
}


void VRayRendererNode::register_operator(OP_OperatorTable *table)
{
	OP_Operator *rop = new OP_Operator(/* Internal name     */ "vray_renderer",
									   /* UI name           */ "V-Ray Renderer",
									   /* How to create one */ VRayRendererNode::myConstructor,
									   /* Parm definitions  */ VRayRendererNode::getTemplatePair(),
									   /* Min # of inputs   */ 0,
									   /* Max # of inputs   */ 0,
									   /* Var definitions   */ VRayRendererNode::getVariablePair(),
									   /* OP flags          */ OP_FLAG_GENERATOR);

	// Set icon
	rop->setIconName("ROP_vray");

	table->addOperator(rop);
}
