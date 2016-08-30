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
#include "vfh_prm_json.h"
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

static const char *res_fraction_items[] = {
	"0.1",       "1/10 (One Tenth Resolution)",
	"0.2",       "1/5 (One Fifth Resolution)",
	"0.25",      "1/4 (Quarter Resolution)",
	"0.3333333", "1/3 (One Third Resolution)",
	"0.5",       "1/2 (Half Resolution)",
	"0.6666666", "2/3 (Two Thirds Resolution)",
	"0.75",      "3/4 (Three Quarter Resolution)",
	"specific",  "User Specified Resolution",
};

static const int res_override_items[] = {
	1280, 720
};


static PRM_Template* getCameraOverridesTemplate()
{
	static Parm::PRMList camOverrides;
	if (camOverrides.empty()) {
		camOverrides.addPrm(
					Parm::PRMFactory(PRM_STRING_E, "render_camera", "Camera")
							.setTypeExtended(PRM_TYPE_DYNAMIC_PATH)
							.setDefault("/obj/cam1")
					);
		camOverrides.addPrm(
					Parm::PRMFactory(PRM_TOGGLE_E, "override_camerares", "Override Camera Resolution")
							.setDefault(PRMzeroDefaults)
					);
		camOverrides.addPrm(
					Parm::PRMFactory(PRM_ORD_E, "res_fraction", "Resolution Scale")
							.setChoiceListItems(PRM_CHOICELIST_SINGLE, res_fraction_items, CountOf(res_fraction_items))
							.setDefault("0.5")
							.addConditional("{ override_camerares == 0 }", PRM_CONDTYPE_HIDE)
							.addConditional("{ override_camerares == 0 }", PRM_CONDTYPE_DISABLE)
					);
		camOverrides.addPrm(
					Parm::PRMFactory(PRM_INT_E, "res_override", "Resolution")
							.setVectorSize(2)
							.setDefaults( res_override_items, CountOf(res_override_items))
							.addConditional("{ override_camerares == 0 }", PRM_CONDTYPE_HIDE)
							.addConditional("{ override_camerares == 0 } { res_fraction != \"specific\" }", PRM_CONDTYPE_DISABLE)
					);
		camOverrides.addPrm(
					Parm::PRMFactory(PRM_INT_E, "aspect_override", "Pixel Aspect Ratio")
							.setDefault(1)
							.setRange(PRM_RANGE_UI, 0, PRM_RANGE_UI, 2)
							.addConditional("{ override_camerares == 0 }", PRM_CONDTYPE_HIDE)
							.addConditional("{ override_camerares == 0 } { res_fraction != \"specific\" }", PRM_CONDTYPE_DISABLE)
					);
	}

	return camOverrides.getPRMTemplate();
}


static void addParmObjects(Parm::PRMList &myPrmList)
{
	myPrmList.addPrm(
			Parm::PRMFactory(PRM_STRING_E, "vobject", "Candidate Objects")
			.setTypeExtended(PRM_TYPE_DYNAMIC_PATH_LIST)
			.setDefault( "*" )
			.addSpareData("opfilter", "!!OBJ/GEOMETRY!!")
			.addSpareData("oprelative", "/obj")
			.addSpareData(RT_UPDATE_SPARE_TAG, RT_UPDATE_SPARE_TAG)
			.getPRMTemplate()
			);
	myPrmList.addPrm(
			Parm::PRMFactory(PRM_STRING_E, "forceobject", "Force Objects")
			.setTypeExtended(PRM_TYPE_DYNAMIC_PATH_LIST)
			.setDefault(PRMzeroDefaults)
			.addSpareData("opfilter", "!!OBJ/GEOMETRY!!")
			.addSpareData("oprelative", "/obj")
			.addSpareData(RT_UPDATE_SPARE_TAG, RT_UPDATE_SPARE_TAG)
			.getPRMTemplate()
			);
	myPrmList.addPrm(
			Parm::PRMFactory(PRM_STRING_E, "matte_objects", "Forced Matte")
			.setTypeExtended(PRM_TYPE_DYNAMIC_PATH_LIST)
			.setDefault(PRMzeroDefaults)
			.addSpareData("opfilter", "!!OBJ/GEOMETRY!!")
			.addSpareData("oprelative", "/obj")
			.addSpareData(RT_UPDATE_SPARE_TAG, RT_UPDATE_SPARE_TAG)
			.getPRMTemplate()
			);
	myPrmList.addPrm(
			Parm::PRMFactory(PRM_STRING_E, "phantom_objects", "Forced Phantom")
			.setTypeExtended(PRM_TYPE_DYNAMIC_PATH_LIST)
			.setDefault(PRMzeroDefaults)
			.addSpareData("opfilter", "!!OBJ/GEOMETRY!!")
			.addSpareData("oprelative", "/obj")
			.addSpareData(RT_UPDATE_SPARE_TAG, RT_UPDATE_SPARE_TAG)
			.getPRMTemplate()
			);
	myPrmList.addPrm(
			Parm::PRMFactory(PRM_STRING_E, "excludeobject", "Exclude Objects")
			.setTypeExtended(PRM_TYPE_DYNAMIC_PATH_LIST)
			.setDefault(PRMzeroDefaults)
			.addSpareData("opfilter", "!!OBJ/GEOMETRY!!")
			.addSpareData("oprelative", "/obj")
			.addSpareData(RT_UPDATE_SPARE_TAG, RT_UPDATE_SPARE_TAG)
			.getPRMTemplate()
			);

	myPrmList.addPrm(
			Parm::PRMFactory(PRM_SEPARATOR, "obj_light_sep")
			.getPRMTemplate()
			);

	myPrmList.addPrm(
			Parm::PRMFactory(PRM_STRING_E, "sololight", "Solo Light")
			.setTypeExtended(PRM_TYPE_DYNAMIC_PATH_LIST)
			.setDefault(PRMzeroDefaults)
			.addSpareData("opfilter", "!!OBJ/LIGHT!!")
			.addSpareData("oprelative", "/obj")
			.addSpareData(RT_UPDATE_SPARE_TAG, RT_UPDATE_SPARE_TAG)
			.getPRMTemplate()
			);
	myPrmList.addPrm(
			Parm::PRMFactory(PRM_STRING_E, "alights", "Candidate Lights")
			.setTypeExtended(PRM_TYPE_DYNAMIC_PATH_LIST)
			.setDefault( "*" )
			.addSpareData("opfilter", "!!OBJ/LIGHT!!")
			.addSpareData("oprelative", "/obj")
			.addSpareData(RT_UPDATE_SPARE_TAG, RT_UPDATE_SPARE_TAG)
			.getPRMTemplate()
			);
	myPrmList.addPrm(
			Parm::PRMFactory(PRM_STRING_E, "forcelights", "Force Lights")
			.setTypeExtended(PRM_TYPE_DYNAMIC_PATH_LIST)
			.setDefault(PRMzeroDefaults)
			.addSpareData("opfilter", "!!OBJ/LIGHT!!")
			.addSpareData("oprelative", "/obj")
			.addSpareData(RT_UPDATE_SPARE_TAG, RT_UPDATE_SPARE_TAG)
			.getPRMTemplate()
			);
	myPrmList.addPrm(
			Parm::PRMFactory(PRM_STRING_E, "excludelights", "Exclude Lights")
			.setTypeExtended(PRM_TYPE_DYNAMIC_PATH_LIST)
			.setDefault(PRMzeroDefaults)
			.addSpareData("opfilter", "!!OBJ/LIGHT!!")
			.addSpareData("oprelative", "/obj")
			.addSpareData(RT_UPDATE_SPARE_TAG, RT_UPDATE_SPARE_TAG)
			.getPRMTemplate()
			);
	myPrmList.addPrm(
			Parm::PRMFactory(PRM_TOGGLE_E, "soho_autoheadlight", "Headlight Creation")
			.setTypeExtended(PRM_TYPE_DYNAMIC_PATH_LIST)
			.setDefault(PRMoneDefaults)
			.addSpareData(RT_UPDATE_SPARE_TAG, RT_UPDATE_SPARE_TAG)
			.getPRMTemplate()
			);

	myPrmList.addPrm(
			Parm::PRMFactory(PRM_SEPARATOR, "light_fog_sep")
			.getPRMTemplate()
			);

	myPrmList.addPrm(
			Parm::PRMFactory(PRM_STRING_E, "vfog", "Visible Fog")
			.setTypeExtended(PRM_TYPE_DYNAMIC_PATH_LIST)
			.setDefault( "*" )
			.addSpareData("opfilter", "!!OBJ/FOG!!")
			.addSpareData("oprelative", "/obj")
			.addSpareData(RT_UPDATE_SPARE_TAG, RT_UPDATE_SPARE_TAG)
			.getPRMTemplate()
			);
}


static void addParmGlobals(Parm::PRMList &myPrmList)
{
	myPrmList.addPrm(PRM_Template(PRM_CALLBACK, 1, &parm_render_interactive, 0, 0, 0, VRayRendererNode::RtStartSession));

	myPrmList.addPrm(PRM_Template(PRM_HEADING, 1, &parm_render_sep_render));

	myPrmList.addPrm(PRM_Template(PRM_ORD, 1, &parm_render_render_mode, PRMzeroDefaults, &parm_render_render_mode_menu));
	myPrmList.addPrm(PRM_Template(PRM_ORD, 1, &parm_render_ipr_mode, PRMzeroDefaults, &parm_render_ipr_mode_menu));
	myPrmList.addPrm(PRM_Template(PRM_ORD, 1, &parm_render_export_mode, PRMzeroDefaults, &parm_render_export_mode_menu));
	myPrmList.addPrm(PRM_Template(PRM_ORD, 1, &parm_render_vfb_mode, PRMzeroDefaults, &parm_render_vfb_mode_menu));
	myPrmList.addPrm(PRM_Template(PRM_TOGGLE_E, 1, &parm_recreate_renderer, PRMzeroDefaults));

	myPrmList.addPrm(PRM_Template(PRM_HEADING, 1, &parm_render_sep_export));
	myPrmList.addPrm(PRM_Template(PRM_FILE_E, PRM_TYPE_DYNAMIC_PATH, 1, &parm_render_export_path, &parm_render_export_path_def));
	myPrmList.addPrm(
			Parm::PRMFactory(PRM_TOGGLE_E, "exp_separatefiles", "Export Each Frame In Separate File")
			.setDefault( PRMzeroDefaults )
			.addConditional("{ render_export_mode == \"Render\" }", PRM_CONDTYPE_DISABLE)
			.setInvisible(true)
			.getPRMTemplate()
			);
	myPrmList.addPrm(
			Parm::PRMFactory(PRM_TOGGLE_E, "exp_hexdata", "Export Data In Hex Format")
			.setDefault( PRMoneDefaults )
			.addConditional("{ render_export_mode == \"Render\" }", PRM_CONDTYPE_DISABLE)
			.getPRMTemplate()
			);
	myPrmList.addPrm(
			Parm::PRMFactory(PRM_TOGGLE_E, "exp_compressed", "Export Compressed")
			.setDefault( PRMoneDefaults )
			.addConditional("{ render_export_mode == \"Render\" }", PRM_CONDTYPE_DISABLE)
			.getPRMTemplate()
			);


	myPrmList.addPrm(PRM_Template(PRM_HEADING, 1, &parm_render_sep_networks));
	myPrmList.addPrm(PRM_Template(PRM_STRING_E, PRM_TYPE_DYNAMIC_PATH, 1, &Parm::parm_render_net_render_channels, &Parm::PRMemptyStringDefault));
	myPrmList.addPrm(PRM_Template(PRM_STRING_E, PRM_TYPE_DYNAMIC_PATH, 1, &Parm::parm_render_net_environment,     &Parm::PRMemptyStringDefault));

	// Standard ROP settings
	//
	myPrmList.addPrm(PRM_Template(PRM_HEADING, 1, &parm_render_scripts));
	myPrmList.addPrm(theRopTemplates[ROP_TPRERENDER_TPLATE]);
	myPrmList.addPrm(theRopTemplates[ROP_PRERENDER_TPLATE]);
	myPrmList.addPrm(theRopTemplates[ROP_LPRERENDER_TPLATE]);
	myPrmList.addPrm(theRopTemplates[ROP_TPREFRAME_TPLATE]);
	myPrmList.addPrm(theRopTemplates[ROP_PREFRAME_TPLATE]);
	myPrmList.addPrm(theRopTemplates[ROP_LPREFRAME_TPLATE]);
	myPrmList.addPrm(theRopTemplates[ROP_TPOSTFRAME_TPLATE]);
	myPrmList.addPrm(theRopTemplates[ROP_POSTFRAME_TPLATE]);
	myPrmList.addPrm(theRopTemplates[ROP_LPOSTFRAME_TPLATE]);
	myPrmList.addPrm(theRopTemplates[ROP_TPOSTRENDER_TPLATE]);
	myPrmList.addPrm(theRopTemplates[ROP_POSTRENDER_TPLATE]);
	myPrmList.addPrm(theRopTemplates[ROP_LPOSTRENDER_TPLATE]);
}


static void addParmDR(Parm::PRMList &myPrmList)
{
	myPrmList.addPrm(PRM_Template(PRM_TOGGLE_E, 1, &parm_DR_enabled, PRMzeroDefaults));
	myPrmList.addPrm(PRM_Template(PRM_STRING_E, 1, &parm_DefaultDRHost_port, &default_DRHost_port,0,0,0,0,1,0,&condition_DRDisabled));
	myPrmList.addPrm(PRM_Template(PRM_MULTITYPE_LIST, DRHostPrmTemplate, 1, &parm_DRHost_count,0,&parm_DRHost_countrange,0,0,&condition_DRDisabled));
}


static PRM_Template* getTemplates()
{
	static Parm::PRMList myPrmList;
	if (myPrmList.empty()) {
		myPrmList.reserve(400);

		myPrmList.switcherBegin("VRayRenderSettings");

		// Globals Tab
		myPrmList.addFolder("Globals");
		addParmGlobals(myPrmList);

		// Objects Tab
		myPrmList.addFolder("Objects");
		addParmObjects(myPrmList);

		// Renderer settings
		//

		// Camera tab
		myPrmList.addFolder("Camera");
		myPrmList.switcherBegin("VRayCameraSettings");

		myPrmList.addFolder("Camera");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsCamera").c_str() );

		myPrmList.addFolder("Depth Of Field");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsCameraDof").c_str() );

		myPrmList.addFolder("Motion Blur");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsMotionBlur").c_str() );

		myPrmList.addFolder("Stereo");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("VRayStereoscopicSettings").c_str() );

		myPrmList.switcherEnd();

		// GI tab
		myPrmList.addFolder("GI");
		myPrmList.switcherBegin("VRayGiSettings");

		myPrmList.addFolder("GI");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsGI").c_str() );

		myPrmList.addFolder("Brute Force");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsDMCGI").c_str() );

		myPrmList.addFolder("Irradiance Map");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsIrradianceMap").c_str() );

		myPrmList.addFolder("Light Cache");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsLightCache").c_str() );

		myPrmList.switcherEnd();

		// Sampler tab
		myPrmList.addFolder("Sampler");
		myPrmList.switcherBegin("VRaySamplersSettings");

		myPrmList.addFolder("DMC");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsDMCSampler").c_str() );

		myPrmList.addFolder("AA");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsImageSampler").c_str() );

		myPrmList.switcherEnd();

		// Options tab
		myPrmList.addFolder("Options");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsOptions").c_str() );

		// Output tab
		myPrmList.addFolder("Output");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsOutput").c_str() );

		// Color Mapping tab
		myPrmList.addFolder("Color Mapping");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsColorMapping").c_str() );

		// Raycaster tab
		myPrmList.addFolder("Raycaster");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsRaycaster").c_str() );

		// Regions tab
		myPrmList.addFolder("Regions");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsRegionsGenerator").c_str() );

		// RT tab
		myPrmList.addFolder("RT");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsRTEngine").c_str() );

		// Caustics tab
		myPrmList.addFolder("Caustics");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsCaustics").c_str() );

		// Displacement tab
		myPrmList.addFolder("Displacement");
		myPrmList.addFromFile( Parm::PRMList::getUIPluginPath("SettingsDefaultDisplacement").c_str() );

		// DR Tab
		myPrmList.addFolder("DR");
		addParmDR(myPrmList);

		myPrmList.switcherEnd();
	}

	return myPrmList.getPRMTemplate();
}


OP_TemplatePair* VRayRendererNode::getTemplatePair()
{
	static OP_TemplatePair *ropPair = nullptr;
	if (!ropPair) {
		OP_TemplatePair *base = new OP_TemplatePair(getCameraOverridesTemplate(), new OP_TemplatePair(getTemplates()));
		ropPair = new OP_TemplatePair(ROP_Node::getROPbaseTemplate(), base);
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


static int isBackground()
{
	return NOT(HOU::isUIAvailable());
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
			long prmIdx = reinterpret_cast< long >(data);
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
	VRayRendererNode &rop = *reinterpret_cast< VRayRendererNode* >(data);
	rop.startIPR(t);
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
		const int reCreate = evalInt(parm_recreate_renderer.getToken(), 0, 0.0) ||
							 (wasRT != isRT) |\
							 (wasGPU != isGPU);

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
	if (   prm
		&& prm->getSparePtr())
	{
		opcreator = UTverify_cast< OP_Network * >(OPgetDirector()->findNode(prm->getSparePtr()->getOpRelative()));
		opfilter = prm->getSparePtr()->getOpFilter();
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
			OP_Node *light = list(i);
			UT_String name = light->getFullPath();

			fpreal dimmer = 0.0;
			light->evalParameterOrProperty("dimmer", 0, m_tstart, dimmer);
			if (dimmer > 0) {
				bundle->addOp(light);
			}
		}
	}

	OP_Bundle *exbundle = getBundleFromOpNodePrm(this, "excludelights", m_tstart);
	if (exbundle) {
		OP_NodeList list;
		exbundle->getMembers(list);
		for (exint i = 0; i < list.size(); ++i) {
			OP_Node *light = list(i);
			UT_String name = light->getFullPath();

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
			OP_Node *node = list(i);
			UT_String name = node->getFullPath();

			if (node->getVisible()) {
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
			UT_String name = node->getFullPath();

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
