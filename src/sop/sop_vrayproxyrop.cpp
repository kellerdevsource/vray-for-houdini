//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "sop_vrayproxyrop.h"
#include "vfh_log.h"
#include "vfh_prm_templates.h"
#include "vfh_vrayproxy_exporter.h"

#include <OP/OP_AutoLockInputs.h>
#include <UT/UT_Assert.h>


using namespace VRayForHoudini;


OP_NodeFlags & SOP::VRayProxyROP::flags()
{
	OP_NodeFlags &flags = SOP_Node::flags();
	flags.setTimeDep(true);
	return flags;
}


OP_ERROR SOP::VRayProxyROP::cookMySop(OP_Context &context)
{
	// We must lock our inputs before we try to access their geometry.
	// OP_AutoLockInputs will automatically unlock our inputs when we return.
	OP_AutoLockInputs inputs(this);
	if (inputs.lock(context) >= UT_ERROR_ABORT) {
		return error();
	}

	// make a copy of input 0's geometry if it different from our last cook
	int inputCanged;
	duplicateChangedSource(0, context, &inputCanged);

	// geometry has not changed from last time - don't bake it
	if (NOT(inputCanged)) {
		return error();
	}

	OP_Node *inpNode = getInput(0);
	if (NOT(inpNode)) {
		addError(SOP_MESSAGE, "Invalid input");
		return error();
	}

	SOP_Node *sopNode = inpNode->castToSOPNode();
	if (NOT(sopNode)) {
		addError(SOP_MESSAGE, "Invalid input");
		return error();
	}

	const fpreal t = context.getTime();

	UT_String filename;
	evalString(filename, "file", 0, t);
	if (NOT(filename.isstring())) {
		addError(SOP_MESSAGE, "Invalid file");
		return error();
	}

	VRayProxyExportOptions params;
	params.m_filename          = filename.buffer();
	params.m_previewType       = static_cast<VUtils::SimplificationType>(evalInt("simplificationType", 0, t));
	params.m_maxPreviewFaces   = std::max(evalInt("maxPreviewFaces", 0, t), 0);
	params.m_maxPreviewStrands = std::max(evalInt("maxPreviewStrands", 0, t), 0);
	params.m_maxFacesPerVoxel  = std::max(evalInt("maxFacesPerVoxel", 0, t), 0);
	params.m_exportColors      = (evalInt("exportColors", 0, t) != 0);
	params.m_exportVelocity    = (evalInt("exportVelocity", 0, t) != 0);
	params.m_velocityStart     = evalFloat("velocityStart", 0, t);
	params.m_velocityEnd       = evalFloat("velocityEnd", 0, t);
	params.m_exportPCL         = (evalInt("exportPCL", 0, t) != 0);
	params.m_pointSize         = evalFloat("pointSize", 0, t);

	VRayProxyExporter exporter(&sopNode, 1);
	exporter.setParams(params);
	VUtils::ErrorCode err = exporter.setContext(context);
	if (err.error()) {
		addError(SOP_MESSAGE, err.getErrorString().ptr());
		return error();
	}

	err = exporter.doExport();
	if (err.error()) {
		addError(SOP_MESSAGE, err.getErrorString().ptr());
	}

	return error();
}


OP_Node* SOP::VRayProxyROP::creator(OP_Network *parent, const char *name, OP_Operator *entry)
{
	return new VRayProxyROP(parent, name, entry);
}


PRM_Template *SOP::VRayProxyROP::getPrmTemplate()
{
	static Parm::PRMList prmList;
	if (prmList.size() <= 0) {

		const char *simplificationTypeItems[] = {
			"facesampling", "Face Sampling",
			"clustering",   "Simplify Clustering",
			"edgecollapse", "Simplify Edge Collapse",
			"combined",     "Simplify Combined",
			};

		prmList.addPrm(Parm::PRMFactory(PRM_FILE_E, "file", "Geometry File")
										.setTypeExtended(PRM_TYPE_DYNAMIC_PATH)
										.setDefault(0, "$HIP/$HIPNAME.vrmesh"));
		prmList.addPrm(Parm::PRMFactory(PRM_ORD_E, "simplificationType", "Simplification Type")
										.setChoiceListItems(PRM_CHOICELIST_SINGLE, simplificationTypeItems, CountOf(simplificationTypeItems))
										.setDefault(PRMthreeDefaults));
		prmList.addPrm(Parm::PRMFactory(PRM_INT_E, "maxPreviewFaces", "Max Preview Faces")
										.setDefault(100));
		prmList.addPrm(Parm::PRMFactory(PRM_INT_E, "maxPreviewStrands", "Max Preview Strands")
										.setDefault(100));
		prmList.addPrm(Parm::PRMFactory(PRM_INT_E, "maxFacesPerVoxel", "Max faces per voxel")
										.setDefault(20000));
		prmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "exportColors", "Export Colors")
										.setDefault(0.f));
		prmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "exportVelocity", "Export Velocity")
										.setDefault(0.f));
		prmList.addPrm(Parm::PRMFactory(PRM_FLT, "velocityStart", "Velocity Start")
										.setDefault(0.f));
		prmList.addPrm(Parm::PRMFactory(PRM_FLT, "velocityEnd", "Velocity End")
										.setDefault(0.05f));
		prmList.addPrm(Parm::PRMFactory(PRM_TOGGLE_E, "exportPCL", "Export Point Cloud")
										.setDefault(0.f));
		prmList.addPrm(Parm::PRMFactory(PRM_FLT_E, "pointSize", "Lowest level point size")
										.setDefault(2.f));
	}

	return prmList.getPRMTemplate();
}
