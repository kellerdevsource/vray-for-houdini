//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
#include "vfh_attr_utils.h"

#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>
#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Director.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_AutoLockInputs.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_BundleList.h>
#include <OP/OP_Options.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <CH/CH_LocalVariable.h>
#include <FS/FS_Info.h>
#include <FS/FS_FileSystem.h>
#include <UT/UT_Assert.h>


using namespace VRayForHoudini;


VRayProxyROP::VRayProxyROP(OP_Network *parent, const char *name, OP_Operator *op):
	ROP_Node(parent, name, op)
{ }


VRayProxyROP::~VRayProxyROP()
{ }


int VRayProxyROP::startRender(int nframes, fpreal tstart, fpreal tend)
{
	UT_String filepath;
	evalString(filepath, "filepath", 0, tstart);
	if (NOT(filepath.isstring())) {
		addError(ROP_MISSING_FILE, "Invalid file specified");
		return ROP_ABORT_RENDER;
	}

	m_sopList.clear();
	if (getSOPList(tstart, m_sopList) <= 0) {
		addError(ROP_NO_OUTPUT, "No geometry found for export");
		return ROP_ABORT_RENDER;
	}

	m_options.m_mkpath = evalInt("mkpath", 0, tstart);

	UT_String dirpath;
	UT_String filename;
	filepath.splitPath(dirpath, filename);

	// create parent dirs if necessary
	FS_Info fsInfo(dirpath);
	if (NOT(fsInfo.exists())) {
		FS_FileSystem fsys;
		if (   NOT(m_options.m_mkpath)
			|| NOT(fsys.createDir(dirpath)))
		{
			addError(ROP_CREATE_DIRECTORY_FAIL, "Failed to create parent directory");
			return ROP_ABORT_RENDER;
		}
	}

	m_options.m_overwrite          = evalInt("overwrite", 0, tstart);
	m_options.m_exportAsSingle     = (evalInt("exp_separately", 0, tstart) == 0);
	m_options.m_lastAsPreview      = evalInt("lastaspreview", 0, tstart);
	m_options.m_animStart          = tstart;
	m_options.m_animEnd            = tend;
	m_options.m_animFrames         = nframes;

	UT_String prefix;
	UT_String frame;
	UT_String suffix;
	m_options.m_exportAsAnimation  = ( m_options.m_animFrames > 1)
									&& NOT(filepath.parseNumberedFilename(prefix, frame, suffix));

	if (error() < UT_ERROR_ABORT) {
		executePreRenderScript(tstart);
	}

	return ROP_CONTINUE_RENDER;
}

VUtils::ErrorCode VRayProxyROP::doExport(const SOPList &sopList)
{
	VRayProxyExporter exporter(m_options, sopList, this);

	VUtils::ErrorCode err = exporter.init();
	if (!err.error()) {
		err = exporter.doExportFrame();
	}

	return err;
}

ROP_RENDER_CODE VRayProxyROP::renderFrame(fpreal time, UT_Interrupt *boss)
{
	if (NOT(getExportOptions(time, m_options))) {
		addError(ROP_BAD_COMMAND, "Invalid parameters");
		return ROP_ABORT_RENDER;
	}

	// Execute the pre-render script.
	executePreFrameScript(time);

	if (error() < UT_ERROR_ABORT) {
		if (m_options.m_exportAsSingle) {
			doExport(m_sopList);
		}
		else {
			for (int sopIdx = 0; sopIdx < m_sopList.size(); ++sopIdx) {
				SOPList singleItem;
				singleItem.append(m_sopList(sopIdx));

				doExport(singleItem);
			}
		}
	}

	// Execute the post-render script.
	if (error() < UT_ERROR_ABORT) {
		executePostFrameScript(time);
	}

	return error() >= UT_ERROR_ABORT ? ROP_ABORT_RENDER : ROP_CONTINUE_RENDER;
}


ROP_RENDER_CODE VRayProxyROP::endRender()
{
	if (error() < UT_ERROR_ABORT) {
		executePostRenderScript(m_options.m_animEnd);
	}

	return ROP_CONTINUE_RENDER;
}


int VRayProxyROP::getSOPList(fpreal time, SOPList &sopList)
{
	int nSOPs = sopList.size();
	OP_Context context(time);

	// We must lock our inputs before we try to access their geometry.
	// OP_AutoLockInputs will automatically unlock our inputs when we return.
	OP_AutoLockInputs inplock(this);
	if (inplock.lock(context) < UT_ERROR_ABORT) {
		OP_Node *inpNode = getInput(0);
		if (inpNode) {
			SOP_Node *sopNode = inpNode->castToSOPNode();
			if (sopNode) {
				sopList.append(sopNode);
			}
		}
	}

	// if no sops from input filter sop nodes from params
	if (nSOPs >= sopList.size()) {
		if (evalInt("use_soppath", 0, time)) {
			UT_String soppath;
			evalString(soppath, "soppath", 0, time);
			SOP_Node *sopNode = getSOPNodeFromPath(soppath, time);
			if (sopNode) {
				sopList.append(sopNode);
			}
		}
		else {
			// get a manager that contains objects
			UT_String root;
			evalString(root, "root", 0, time);
			OP_Network *rootnet = getOBJNodeFromPath(root, time);
			rootnet = (rootnet)? rootnet : OPgetDirector()->getManager("obj");

			UT_ASSERT( rootnet );

			// create internal bundle that will contain all nodes matching nodeMask
			UT_String nodeMask;
			evalString(nodeMask, "objects", 0, time);

			UT_String bundleName;
			OP_Bundle *bundle = OPgetDirector()->getBundles()->getPattern(bundleName,
																		  rootnet,
																		  rootnet,
																		  nodeMask,
																		  PRM_SpareData::objGeometryPath.getOpFilter());

			// get the node list for processing
			OP_NodeList nodeList;
			bundle->getMembers(nodeList);
			// release the internal bundle created by getPattern()
			OPgetDirector()->getBundles()->deReferenceBundle(bundleName);

			int ignoreHidden = NOT(evalInt("save_hidden", 0, time));
			for (OP_Node *opnode : nodeList) {
				UT_String fullpath;

				OBJ_Node *objNode = opnode->castToOBJNode();
				if (NOT(objNode)) {
					continue;
				}

				objNode->getFullPath(fullpath);

				SOP_Node *sopNode = objNode->getRenderSopPtr();
				if (NOT(sopNode)) {
					continue;
				}

				if (   sopNode->getTemplate()
					|| NOT(sopNode->getVisible()) )
				{
					if (ignoreHidden) {
						continue;
					}
				}

				sopNode->getFullPath(fullpath);

				sopList.append(sopNode);
			}
		}
	}

	return sopList.size() - nSOPs;
}


int VRayProxyROP::getExportOptions(fpreal time, VRayProxyExportOptions &options) const
{
	options.m_context.setTime(time);

	evalString(options.m_filepath, "filepath", 0, time);

	options.m_applyTransform     = evalInt("xformtype", 0, time);
	options.m_exportVelocity     = evalInt("exp_velocity", 0, time);
	options.m_velocityStart      = evalFloat("velocity", 0, time);
	options.m_velocityEnd        = evalFloat("velocity", 1, time);
	options.m_simplificationType = static_cast<VUtils::SimplificationType>(evalInt("simplificationtype", 0, time));
	options.m_maxPreviewFaces    = SYSmax(evalInt("max_previewfaces", 0, time), 0);
	options.m_maxPreviewStrands  = SYSmax(evalInt("max_previewstrands", 0, time), 0);
	options.m_maxFacesPerVoxel   = (evalInt("voxelpermesh", 0, time))? 0 : SYSmax(evalInt("max_facespervoxel", 0, time), 0);
	options.m_exportPCLs         = evalInt("exp_pcls", 0, time);
	options.m_pointSize          = evalFloat("pointsize", 0, time);

	return true;
}


PRM_Template *VRayProxyROP::getMyPrmTemplate()
{
	static Parm::PRMList myPrmList;
	if (!myPrmList.empty()) {
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
	myPrmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "lastaspreview", "Use Last As Preview")
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

	myPrmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "exp_pcls", "Export Point Clouds")
									.setDefault(PRMzeroDefaults)
					 );
	myPrmList.addPrm(Parm::PRMFactory(PRM_FLT_E, "pointsize", "Lowest level point size")
									.setDefault(0.5f)
									.addConditional("{ exp_pcls == 0 }", PRM_CONDTYPE_DISABLE)
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
