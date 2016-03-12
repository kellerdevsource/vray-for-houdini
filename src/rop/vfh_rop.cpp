//
// Copyright (c) 2015, Chaos Software Ltd
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
#include "vfh_prm_json.h"
#include "vfh_prm_def.h"
#include "vfh_prm_templates.h"
#include "vfh_ui.h"

#include <ROP/ROP_Templates.h>
#include <UT/UT_Interrupt.h>


using namespace VRayForHoudini;

static PRM_Name     parm_render_scripts("parm_render_scripts", "Scripts");

static PRM_Name     parm_render_interactive("render_rt", "Render RT");

static PRM_Name     parm_render_vfb_mode("render_vfb_mode", "Framebuffer");
static PRM_Name     parm_render_vfb_mode_items[] = {
	PRM_Name("Native"),
	PRM_Name("Simple"),
	PRM_Name(),
};
static PRM_ChoiceList parm_render_vfb_mode_menu(PRM_CHOICELIST_SINGLE, parm_render_vfb_mode_items);

static PRM_Name     parm_render_sep_render("render_sep_render", "Render Settings");
static PRM_Name     parm_render_camera("render_camera", "Camera");
static PRM_Default  parm_render_camera_def(0, "/obj/cam1");

static PRM_Name     parm_render_sep_export("render_sep_export", "Export Settings");
static PRM_Name     parm_render_export_path("render_export_filepath", "Export Filepath");
static PRM_Default  parm_render_export_path_def(0, "$HIP/$HIPNAME.vrscene");

static PRM_Name     parm_render_export_mode("render_export_mode", "Export Mode");
static PRM_Name     parm_render_export_mode_items[] = {
	PRM_Name("Render"),
	PRM_Name("Export & Render"),
	PRM_Name("Export"),
	PRM_Name(),
};
static PRM_ChoiceList parm_render_export_mode_menu(PRM_CHOICELIST_SINGLE, parm_render_export_mode_items);

static PRM_Name     parm_render_render_mode("render_render_mode", "Render Mode");
static PRM_Name     parm_render_render_mode_items[] = {
	PRM_Name("Production"),
	PRM_Name("RT CPU"),
	PRM_Name("GPU OpenCL"),
	PRM_Name("GPU CUDA"),
	PRM_Name(),
};
static PRM_ChoiceList parm_render_render_mode_menu(PRM_CHOICELIST_SINGLE, parm_render_render_mode_items);

static PRM_Name  parm_render_ipr_mode("render_rt_mode", "RT Render Mode");
static PRM_Name  parm_render_ipr_mode_items[] = {
	PRM_Name("RT CPU"),
	PRM_Name("GPU OpenCL"),
	PRM_Name("GPU CUDA"),
	PRM_Name(),
};
static PRM_ChoiceList parm_render_ipr_mode_menu(PRM_CHOICELIST_SINGLE, parm_render_ipr_mode_items);


static PRM_Name  parm_render_sep_networks("render_sep_networks", "Networks");

static PRM_Name          RenderSettingsSwitcherName("VRayRenderSettings");
static Parm::PRMDefList  RenderSettingsSwitcherTabs;
static Parm::PRMTmplList RenderSettingsPrmTemplate;

static Parm::TabItemDesc RenderSettingsTabItemsDesc[] = {
	{ "Options",        "SettingsOptions"          },
	{ "Output",         "SettingsOutput"           },
	{ "Color Mapping",  "SettingsColorMapping"     },
	{ "Raycaster",      "SettingsRaycaster"        },
	{ "Regions",        "SettingsRegionsGenerator" },
	{ "RT",             "SettingsRTEngine"         },
	{ "Caustics",       "SettingsCaustics"         },
};

static PRM_Name          GiSettingsSwitcherName("VRayGiSettings");
static Parm::PRMDefList  GiSettingsSwitcherTabs;
static Parm::TabItemDesc GiSettingsTabItemsDesc[] = {
	{ "GI",             "SettingsGI"               },
	{ "Brute Force",    "SettingsDMCGI"            },
	{ "Irradiance Map", "SettingsIrradianceMap"    },
	{ "Light Cache",    "SettingsLightCache"       },
};

static PRM_Name          CameraSettingsSwitcherName("VRayCameraSettings");
static Parm::PRMDefList  CameraSettingsSwitcherTabs;
static Parm::TabItemDesc CameraSettingsTabItemsDesc[] = {
	{ "Camera",         "SettingsCamera"           },
	{ "Depth Of Field", "SettingsCameraDof"        },
	{ "Motion Blur",    "SettingsMotionBlur"       },
	{ "Stereo",         "VRayStereoscopicSettings" },
};

static PRM_Name          SamplersSettingsSwitcherName("VRaySamplersSettings");
static Parm::PRMDefList  SamplersSettingsSwitcherTabs;
static Parm::TabItemDesc SamplersSettingsTabItemsDesc[] = {
	{ "DMC",            "SettingsDMCSampler"       },
	{ "AA",             "SettingsImageSampler"     },
};

static PRM_Default       default_DRHost_address(0.0, "localhost");
static PRM_Default       default_DRHost_port(0.0, "20207");
static PRM_Name          parm_DR_enabled("dr_enabled", "Enabled");
static PRM_Name          parm_DefaultDRHost_port("drhost_port", "Default Port");
static PRM_Name          parm_DRHost_count("drhost_cnt", "Number of Hosts");
static PRM_Range         parm_DRHost_countrange(PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 31);
static PRM_Name          parm_DRHost_enabled("drhost#_enabled", "Enanbled");
static PRM_Name          parm_DRHost_address("drhost#_address", "Host Address");
static PRM_Name          parm_DRHost_usedefaultport("drhost#_usedefaultport", "Use Default Port");
static PRM_Name          parm_DRHost_port("drhost#_port", "Host Port");
static PRM_Conditional   condition_DRDisabled("{ dr_enabled == 0 }");
static PRM_Conditional   condition_DRHostDisabled("{ dr_enabled == 0 } { drhost#_enabled == 0 }");
static PRM_Conditional   condition_DRHostPortDisabled("{ dr_enabled == 0 } { drhost#_enabled == 0 } { drhost#_usedefaultport == 1 }");

static PRM_Name          parm_recreate_renderer("recreate_renderer", "Recreate Renderer");

static PRM_Template      DRHostPrmTemplate[] = {
	PRM_Template(PRM_TOGGLE_E, 1, &parm_DRHost_enabled, PRMoneDefaults,0,0,0,0,1,0,&condition_DRDisabled),
	PRM_Template(PRM_STRING_E, 1, &parm_DRHost_address, &default_DRHost_address,0,0,0,0,1,0,&condition_DRHostDisabled),
	PRM_Template(PRM_TOGGLE_E, 1, &parm_DRHost_usedefaultport, PRMoneDefaults,0,0,0,0,1,0,&condition_DRHostDisabled),
	PRM_Template(PRM_STRING_E, 1, &parm_DRHost_port, &default_DRHost_port,0,0,0,0,1,0,&condition_DRHostPortDisabled),
	PRM_Template()
};

static PRM_Template* getTemplates()
{
	if (!RenderSettingsPrmTemplate.size()) {
		// Render / Exporter settings
		//
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_CALLBACK, 1, &parm_render_interactive, 0, 0, 0, VRayRendererNode::RtStartSession));

		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_HEADING, 1, &parm_render_sep_render));
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_STRING_E, PRM_TYPE_DYNAMIC_PATH, 1, &parm_render_camera, &parm_render_camera_def));
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_ORD, 1, &parm_render_render_mode, PRMzeroDefaults, &parm_render_render_mode_menu));
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_ORD, 1, &parm_render_ipr_mode, PRMzeroDefaults, &parm_render_ipr_mode_menu));
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_ORD, 1, &parm_render_export_mode, PRMzeroDefaults, &parm_render_export_mode_menu));
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_ORD, 1, &parm_render_vfb_mode, PRMzeroDefaults, &parm_render_vfb_mode_menu));
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_TOGGLE_E, 1, &parm_recreate_renderer, PRMzeroDefaults));

		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_HEADING, 1, &parm_render_sep_export));
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_FILE_E, PRM_TYPE_DYNAMIC_PATH, 1, &parm_render_export_path, &parm_render_export_path_def));

		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_HEADING, 1, &parm_render_sep_networks));
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_STRING_E, PRM_TYPE_DYNAMIC_PATH, 1, &Parm::parm_render_net_render_channels, &Parm::PRMemptyStringDefault));
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_STRING_E, PRM_TYPE_DYNAMIC_PATH, 1, &Parm::parm_render_net_environment,     &Parm::PRMemptyStringDefault));

		// Standard ROP settings
		//
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_HEADING, 1, &parm_render_scripts));
		RenderSettingsPrmTemplate.push_back(theRopTemplates[ROP_TPRERENDER_TPLATE]);
		RenderSettingsPrmTemplate.push_back(theRopTemplates[ROP_PRERENDER_TPLATE]);
		RenderSettingsPrmTemplate.push_back(theRopTemplates[ROP_LPRERENDER_TPLATE]);
		RenderSettingsPrmTemplate.push_back(theRopTemplates[ROP_TPREFRAME_TPLATE]);
		RenderSettingsPrmTemplate.push_back(theRopTemplates[ROP_PREFRAME_TPLATE]);
		RenderSettingsPrmTemplate.push_back(theRopTemplates[ROP_LPREFRAME_TPLATE]);
		RenderSettingsPrmTemplate.push_back(theRopTemplates[ROP_TPOSTFRAME_TPLATE]);
		RenderSettingsPrmTemplate.push_back(theRopTemplates[ROP_POSTFRAME_TPLATE]);
		RenderSettingsPrmTemplate.push_back(theRopTemplates[ROP_LPOSTFRAME_TPLATE]);
		RenderSettingsPrmTemplate.push_back(theRopTemplates[ROP_TPOSTRENDER_TPLATE]);
		RenderSettingsPrmTemplate.push_back(theRopTemplates[ROP_POSTRENDER_TPLATE]);
		RenderSettingsPrmTemplate.push_back(theRopTemplates[ROP_LPOSTRENDER_TPLATE]);

		RenderSettingsSwitcherTabs.push_back(PRM_Default(RenderSettingsPrmTemplate.size(), "Globals"));

		// Renderer settings
		//
		Parm::addTabWithTabs("Camera",
							 CameraSettingsTabItemsDesc, CountOf(CameraSettingsTabItemsDesc),
							 CameraSettingsSwitcherTabs, CameraSettingsSwitcherName,
							 RenderSettingsPrmTemplate, RenderSettingsSwitcherTabs);

		Parm::addTabWithTabs("GI",
							 GiSettingsTabItemsDesc, CountOf(GiSettingsTabItemsDesc),
							 GiSettingsSwitcherTabs, GiSettingsSwitcherName,
							 RenderSettingsPrmTemplate, RenderSettingsSwitcherTabs);

		Parm::addTabWithTabs("Sampler",
							 SamplersSettingsTabItemsDesc, CountOf(SamplersSettingsTabItemsDesc),
							 SamplersSettingsSwitcherTabs, SamplersSettingsSwitcherName,
							 RenderSettingsPrmTemplate, RenderSettingsSwitcherTabs);

		Parm::addTabsItems(RenderSettingsTabItemsDesc, CountOf(RenderSettingsTabItemsDesc), RenderSettingsSwitcherTabs, RenderSettingsPrmTemplate);

		// DR Settings
		const int DRPrmIdx = RenderSettingsPrmTemplate.size();
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_TOGGLE_E, 1, &parm_DR_enabled, PRMzeroDefaults));
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_STRING_E, 1, &parm_DefaultDRHost_port, &default_DRHost_port,0,0,0,0,1,0,&condition_DRDisabled));
		RenderSettingsPrmTemplate.push_back(PRM_Template(PRM_MULTITYPE_LIST, DRHostPrmTemplate, 1, &parm_DRHost_count,0,&parm_DRHost_countrange,0,0,&condition_DRDisabled));
		RenderSettingsSwitcherTabs.push_back(PRM_Default(RenderSettingsPrmTemplate.size() - DRPrmIdx, "DR"));


		RenderSettingsPrmTemplate.push_back(PRM_Template()); // List terminator

		// Main switcher menu
		RenderSettingsPrmTemplate.insert(RenderSettingsPrmTemplate.begin(),
										 PRM_Template(PRM_SWITCHER,
													  RenderSettingsSwitcherTabs.size(),
													  &RenderSettingsSwitcherName,
													  &RenderSettingsSwitcherTabs[0]));
	}

	return &RenderSettingsPrmTemplate[0];
}


OP_TemplatePair* VRayRendererNode::getTemplatePair()
{
	static OP_TemplatePair *ropPair = 0;
	if (!ropPair) {
		OP_TemplatePair *base = new OP_TemplatePair(getTemplates());
		ropPair = new OP_TemplatePair(ROP_Node::getROPbaseTemplate(), base);
	}
	return ropPair;
}


static int getRendererMode(OP_Node &rop)
{
	int renderMode = rop.evalInt(parm_render_render_mode.getToken(), 0, 0.0);
	switch (renderMode) {
		case 0: renderMode = -1; break; // Production
		case 1: renderMode =  0; break; // RT CPU
		case 2: renderMode =  1; break; // RT GPU (OpenCL)
		case 3: renderMode =  4; break; // RT GPU (CUDA)
	}
	return renderMode;
}


static int getRendererIprMode(OP_Node &rop)
{
	int renderMode = rop.evalInt(parm_render_ipr_mode.getToken(), 0, 0.0);
	switch (renderMode) {
		case 0: renderMode =  0; break; // RT CPU
		case 1: renderMode =  1; break; // RT GPU (OpenCL)
		case 2: renderMode =  4; break; // RT GPU (CUDA)
	}
	return renderMode;
}


static VRayExporter::ExpWorkMode getExporterWorkMode(OP_Node &rop)
{
	return static_cast<VRayExporter::ExpWorkMode>(rop.evalInt(parm_render_export_mode.getToken(), 0, 0.0));
}


static std::string getExportFilepath(OP_Node &rop)
{
	UT_String exportFilepath;
	rop.evalString(exportFilepath, parm_render_export_path.getToken(), 0, 0.0);
	return exportFilepath.toStdString();
}


static int isBackground()
{
	return 0;
}


static int getFrameBufferType(OP_Node &rop)
{
	int fbType = isBackground() ? -1 : 0;

	if (fbType >= 0) {
		fbType = rop.evalInt(parm_render_vfb_mode.getToken(), 0, 0.0);
	}

	return fbType;
}


VRayRendererNode::VRayRendererNode(OP_Network *net, const char *name, OP_Operator *entry)
	: ROP_Node(net, name, entry)
	, m_exporter(this)
{
	Log::getLog().debug("VRayRendererNode()");
}


VRayRendererNode::~VRayRendererNode()
{
	Log::getLog().debug("~VRayRendererNode()");

#if 0
	m_exporter.delOpCallback(this, VRayRendererNode::RtCallbackRop);
#endif
}


bool VRayRendererNode::updateParmsFlags()
{
	bool changed = ROP_Node::updateParmsFlags();

	for (int t = 0; t < CountOf(RenderSettingsTabItemsDesc); ++t) {
		const Parm::TabItemDesc &tabItemDesc = RenderSettingsTabItemsDesc[t];

		UI::ActiveStateDeps::activateElements(tabItemDesc.pluginID, *this, changed, boost::str(Parm::FmtPrefix % tabItemDesc.pluginID));
	}

	for (int t = 0; t < CountOf(GiSettingsTabItemsDesc); ++t) {
		const Parm::TabItemDesc &tabItemDesc = GiSettingsTabItemsDesc[t];

		UI::ActiveStateDeps::activateElements(tabItemDesc.pluginID, *this, changed, boost::str(Parm::FmtPrefix % tabItemDesc.pluginID));
	}

	for (int t = 0; t < CountOf(SamplersSettingsTabItemsDesc); ++t) {
		const Parm::TabItemDesc &tabItemDesc = SamplersSettingsTabItemsDesc[t];

		UI::ActiveStateDeps::activateElements(tabItemDesc.pluginID, *this, changed, boost::str(Parm::FmtPrefix % tabItemDesc.pluginID));
	}

	for (int t = 0; t < CountOf(CameraSettingsTabItemsDesc); ++t) {
		const Parm::TabItemDesc &tabItemDesc = CameraSettingsTabItemsDesc[t];

		UI::ActiveStateDeps::activateElements(tabItemDesc.pluginID, *this, changed, boost::str(Parm::FmtPrefix % tabItemDesc.pluginID));
	}

	return changed;
}


void VRayRendererNode::RtCallbackRop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayRendererNode *rop = (VRayRendererNode*)callee;

	Log::getLog().debug("RtCallbackRop: %s from \"%s\"", OPeventToString(type), caller->getName().buffer());

	if (type == OP_NODE_PREDELETE) {
		caller->removeOpInterest(rop, VRayRendererNode::RtCallbackRop);
	}
}


int VRayRendererNode::RtStartSession(void *data, int /*index*/, float /*t*/, const PRM_Template* /*tplate*/)
{
	VRayRendererNode &rop = *reinterpret_cast<VRayRendererNode*>(data);
	rop.startIPR();
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
		const int reCreate = evalInt(parm_recreate_renderer.getToken(), 0, 0.0) ||
							 (wasRT != isRT) |\
							 (wasGPU != isGPU);

		m_exporter.setIPR(isRT);

		if (m_exporter.initRenderer(!isBackground(), reCreate)) {
			m_exporter.initExporter(getFrameBufferType(*this), nframes, tstart, tend);

			m_exporter.setDRSettings();
			m_exporter.setRendererMode(rendererMode);
			m_exporter.setWorkMode(getExporterWorkMode(*this));
			m_exporter.setExportFilepath(getExportFilepath(*this));

			m_exporter.exportSettings();

			error = m_exporter.getError();
		}
	}

	return error;
}


void VRayRendererNode::startIPR()
{
	if (initSession(true, 1, 0, 0)) {
		m_exporter.exportFrame(OPgetDirector()->getChannelManager()->getEvaluateTime(SYSgetSTID()));
	}
}


int VRayRendererNode::startRender(int nframes, fpreal tstart, fpreal tend)
{
	Log::getLog().debug("VRayRendererNode::startRender(%i, %.3f, %.3f)", nframes, tstart, tend);

	return initSession(false, nframes, tstart, tend);
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
									   /* Max # of inputs   */ 0);

	// Set icon
	rop->setIconName("ROP_vray");

	table->addOperator(rop);
}
