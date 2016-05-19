//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "rop_vrayproxyrop.h"
#include "vfh_log.h"
#include "vfh_prm_templates.h"
#include "vfh_export_vrayproxy.h"

#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Director.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_AutoLockInputs.h>
#include <OP/OP_Options.h>
#include <PRM/PRM_Include.h>
#include <CH/CH_LocalVariable.h>
#include <UT/UT_Assert.h>


using namespace VRayForHoudini;


VRayProxyROP::VRayProxyROP(OP_Network *parent, const char *name, OP_Operator *op):
	ROP_Node(parent, name, op)
{ }


VRayProxyROP::~VRayProxyROP()
{ }


int VRayProxyROP::startRender(int nframes, fpreal tstart, fpreal tend)
{
	m_tend = tend;
	m_nframes = nframes;
	if (error() < UT_ERROR_ABORT) {
		executePreRenderScript(tstart);
	}

	return ROP_CONTINUE_RENDER;
}


ROP_RENDER_CODE VRayProxyROP::renderFrame(fpreal time, UT_Interrupt *boss)
{
	OP_Context context(time);

	UT_String filename;
	evalString(filename, "filepath", 0, time);
	if (NOT(filename.isstring())) {
		addError(ROP_MESSAGE, "Invalid file");
		return ROP_ABORT_RENDER;
	}

	// We must lock our inputs before we try to access their geometry.
	// OP_AutoLockInputs will automatically unlock our inputs when we return.
	OP_AutoLockInputs inputs(this);
	if (inputs.lock(context) >= UT_ERROR_ABORT) {
		return ROP_ABORT_RENDER;
	}

	OP_Node *inpNode = getInput(0);
	if (NOT(inpNode)) {
		addError(ROP_MESSAGE, "Invalid input");
		return ROP_ABORT_RENDER;
	}

	SOP_Node *sopNode = inpNode->castToSOPNode();
	if (NOT(sopNode)) {
		addError(ROP_MESSAGE, "Invalid input");
		return ROP_ABORT_RENDER;
	}

	VRayProxyExportOptions params;
	params.m_filename          = filename.buffer();
	params.m_previewType       = static_cast<VUtils::SimplificationType>(evalInt("simplificationtype", 0, time));
	params.m_maxPreviewFaces   = std::max(evalInt("max_previewfaces", 0, time), 0);
	params.m_maxPreviewStrands = std::max(evalInt("max_previewstrands", 0, time), 0);
	params.m_maxFacesPerVoxel  = std::max(evalInt("max_facespervoxel", 0, time), 0);
	params.m_exportVelocity    = (evalInt("exp_velocity", 0, time) != 0);
	params.m_velocityStart     = evalFloat("velocity", 0, time);
	params.m_velocityEnd       = evalFloat("velocity", 1, time);
	params.m_exportPCL         = (evalInt("exp_pcl", 0, time) != 0);
	params.m_pointSize         = evalFloat("pointsize", 0, time);

	VRayProxyExporter exporter(&sopNode, 1);
	exporter.setParams(params);
	VUtils::ErrorCode err = exporter.setContext(context);
	if (err.error()) {
		addError(ROP_MESSAGE, err.getErrorString().ptr());
		return ROP_ABORT_RENDER;
	}

	// Execute the pre-render script.
	executePreFrameScript(time);

	// Do actual export
	err = exporter.doExport();
	if (err.error()) {
		addError(ROP_MESSAGE, err.getErrorString().ptr());
		return ROP_ABORT_RENDER;
	}

	// Execute the post-render script.
	if (error() < UT_ERROR_ABORT) {
		executePostFrameScript(time);
	}

	return ROP_CONTINUE_RENDER;
}


ROP_RENDER_CODE VRayProxyROP::endRender()
{
	if (error() < UT_ERROR_ABORT) {
		executePostRenderScript(m_tend);
	}

	return ROP_CONTINUE_RENDER;
}


PRM_Template *VRayProxyROP::getMyPrmTemplate()
{
	static Parm::PRMList myPrmList;
	if (NOT(myPrmList.empty())) {
		return myPrmList.getPRMTemplate();
	}

	const char *simplificationTypeItems[] = {
		"facesampling", "Face Sampling",
		"clustering",   "Simplify Clustering",
		"edgecollapse", "Simplify Edge Collapse",
		"combined",     "Simplify Combined",
		};

	const char *xformTypeItems[] = {
		"0", "None",
		"1", "World Transform",
		};

	const fpreal velrangeDefaults[] = {
		0.0, 0.05,
		};

	myPrmList.addPrm(Parm::PRMFactory(PRM_FILE, "filepath", "Geometry File")
									.setTypeExtended(PRM_TYPE_DYNAMIC_PATH)
									.setDefault("$HIP/$HIPNAME.vrmesh")
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "overwrite", "Overwrite Existing File(s)")
									.setDefault(PRMoneDefaults)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "mkpath", "Create Intermediate Dirs")
									.setDefault(PRMzeroDefaults)
					 );

	myPrmList.addPrm(Parm::PRMFactory(PRM_SEPARATOR, "_sep1"));

	myPrmList.addPrm(Parm::PRMFactory(PRM_JOINED_TOGGLE, "use_soppath", "")
									.setDefault(PRMzeroDefaults)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_STRING_OPREF, "soppath", "SOP Path")
									.setTypeExtended(PRM_TYPE_DYNAMIC_PATH)
									.setDefault(PRMzeroDefaults)
									.setSpareData(&PRM_SpareData::sopPath)
									.addConditional("{ use_soppath == 0 }", PRM_CONDTYPE_DISABLE)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_STRING_OPREF, "root", "Root Object")
									.setTypeExtended(PRM_TYPE_DYNAMIC_PATH)
									.setDefault("/obj")
									.setSpareData(&PRM_SpareData::objPath)
									.addConditional("{ use_soppath == 1 }", PRM_CONDTYPE_DISABLE)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_STRING_OPLIST, "objects", "Objects")
									.setTypeExtended(PRM_TYPE_DYNAMIC_PATH_LIST)
									.setDefault("*")
									.setSpareData(&PRM_SpareData::objGeometryPath)
									.addConditional("{ use_soppath == 1 }", PRM_CONDTYPE_DISABLE)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "save_hidden", "Save Hidden And Templated Geometry")
									.setDefault(PRMzeroDefaults)
									.addConditional("{ use_soppath == 1 }", PRM_CONDTYPE_DISABLE)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "exp_separately", "Export Each Object In Separate File ")
									.setDefault(PRMzeroDefaults)
									.addConditional("{ use_soppath == 1 }", PRM_CONDTYPE_DISABLE)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "last_as_preview", "Use Last As Preview")
									.setDefault(PRMzeroDefaults)
									.addConditional("{ use_soppath == 1 }", PRM_CONDTYPE_DISABLE)
					 );

	myPrmList.addPrm(Parm::PRMFactory(PRM_SEPARATOR, "_sep2"));

	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD_E, "xformtype", "Transform")
									.setChoiceListItems(PRM_CHOICELIST_SINGLE, xformTypeItems, CountOf(xformTypeItems))
									.setDefault(PRMzeroDefaults)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "exp_velocity", "Export Velocity")
									.setDefault(PRMzeroDefaults)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_STARTEND, "velocity", "Velocity Range")
									.setVectorSize(2)
									.setDefaults(velrangeDefaults, CountOf(velrangeDefaults))
									.addConditional("{ exp_velocity == 0 }", PRM_CONDTYPE_DISABLE)
					 );

	myPrmList.addPrm(Parm::PRMFactory(PRM_SEPARATOR, "_sep3"));

	myPrmList.addPrm(Parm::PRMFactory(PRM_ORD_E, "simplificationtype", "Simplification Type")
									.setChoiceListItems(PRM_CHOICELIST_SINGLE, simplificationTypeItems, CountOf(simplificationTypeItems))
									.setDefault(PRMthreeDefaults)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_INT_E, "max_previewfaces", "Max Preview Faces")
									.setDefault(100.)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_INT_E, "max_previewstrands", "Max Preview Strands")
									.setDefault(100.)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "voxelpermesh", "One Voxel Per Mesh")
									.setDefault(PRMzeroDefaults)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_INT_E, "max_facespervoxel", "Max faces per voxel")
									.setDefault(20000.)
									.addConditional("{ voxelpermesh == 1 }", PRM_CONDTYPE_DISABLE)
					 );

	myPrmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "exp_pcl", "Export Point Clouds")
									.setDefault(PRMzeroDefaults)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_FLT_E, "pointsize", "Lowest level point size")
									.setDefault(2.f)
									.addConditional("{ exp_pcl == 0 }", PRM_CONDTYPE_DISABLE)
					 );

	myPrmList.addPrm(Parm::PRMFactory(PRM_SEPARATOR, "_sep4"));

	myPrmList.addPrm( theRopTemplates[ROP_TPRERENDER_TPLATE] );
	myPrmList.addPrm( theRopTemplates[ROP_PRERENDER_TPLATE] );
	myPrmList.addPrm( theRopTemplates[ROP_LPRERENDER_TPLATE] );
	myPrmList.addPrm( theRopTemplates[ROP_TPREFRAME_TPLATE] );
	myPrmList.addPrm( theRopTemplates[ROP_PREFRAME_TPLATE] );
	myPrmList.addPrm( theRopTemplates[ROP_LPREFRAME_TPLATE] );
	myPrmList.addPrm( theRopTemplates[ROP_TPOSTFRAME_TPLATE] );
	myPrmList.addPrm( theRopTemplates[ROP_POSTFRAME_TPLATE] );
	myPrmList.addPrm( theRopTemplates[ROP_LPOSTFRAME_TPLATE] );
	myPrmList.addPrm( theRopTemplates[ROP_TPOSTRENDER_TPLATE] );
	myPrmList.addPrm( theRopTemplates[ROP_POSTRENDER_TPLATE] );
	myPrmList.addPrm( theRopTemplates[ROP_LPOSTRENDER_TPLATE] );

	return myPrmList.getPRMTemplate();
}


OP_TemplatePair* VRayProxyROP::getTemplatePair()
{
	static OP_TemplatePair *myTemplatePair = nullptr;
	if (NOT(myTemplatePair)) {
		OP_TemplatePair *myPrms = new OP_TemplatePair(getMyPrmTemplate());
		myTemplatePair = new OP_TemplatePair(ROP_Node::getROPbaseTemplate(), myPrms);
	}

	return myTemplatePair;
}


OP_VariablePair * VRayProxyROP::getVariablePair()
{
	static OP_VariablePair *myVariablePair = nullptr;
	if (NOT(myVariablePair)) {
		myVariablePair = new OP_VariablePair(ROP_Node::myVariableList);
	}

	return myVariablePair;
}


void VRayProxyROP::register_ropoperator(OP_OperatorTable *table)
{
	OP_Operator *op = new OP_Operator(
				"rop_vrayproxy",
				"V-Ray Proxy ROP",
				VRayProxyROP::creator,
				VRayProxyROP::getTemplatePair(),
				0,
				9999,
				VRayProxyROP::getVariablePair(),
				OP_FLAG_GENERATOR,
				nullptr,
				1
				);

	op->setIconName("ROP_vray");
	table->addOperator(op);
}


void VRayProxyROP::register_sopoperator(OP_OperatorTable *table)
{
	OP_Operator *op = new OP_Operator(
				"rop_vrayproxy",
				"V-Ray Proxy ROP",
				VRayProxyROP::creator,
				VRayProxyROP::getTemplatePair(),
				0,
				1,
				VRayProxyROP::getVariablePair(),
				OP_FLAG_GENERATOR | OP_FLAG_MANAGER,
				nullptr,
				0
				);

	op->setIconName("ROP_vray");
	table->addOperator(op);
}


OP_Node * VRayProxyROP::creator(OP_Network *parent, const char *name, OP_Operator *op)
{
	return new VRayProxyROP(parent, name, op);
}
