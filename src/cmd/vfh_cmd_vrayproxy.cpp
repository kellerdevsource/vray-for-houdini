//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_cmd_vrayproxy.h"
#include "vfh_export_vrayproxy.h"

#include <OP/OP_Director.h>
#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_BundleList.h>
#include <PRM/PRM_SpareData.h>


using namespace VRayForHoudini;


enum CMDError {
	CMD_ARG_OK = 0,
	CMD_ARG_NOT_FOUND = 1
};


/// Parse command line arguments passed to vrayproxy cmd
/// and return export options for the vray proxy exporter
/// @param[in] args - command line arguments passed to vrayproxy cmd
/// @param[out] options - command line arguments parsed as proxy export options
/// @retval CMD_ARG_OK == no error or error code for invalid arguments/error while parsing
static CMDError parseExportOptions(const CMD_Args &args, VRayProxyExportOptions &options)
{
	if ( NOT(args.found('n'))) {
		return CMD_ARG_NOT_FOUND;
	}

	options.m_filepath = args.argp('n', 0);
	options.m_mkpath = args.found('c');
	options.m_overwrite = args.found('f');

	options.m_exportAsAnimation = args.found('a');
	if (options.m_exportAsAnimation) {
		int animStart = args.iargp('a', 0);
		int animEnd = args.iargp('a', 1);
		animStart = std::max(std::min(animStart, animEnd), 1);
		animEnd = std::max(std::max(animStart, animEnd), 1);

		options.m_context.setFrame(static_cast< long >(animStart));
		options.m_animStart = options.m_context.getTime();
		options.m_context.setFrame(static_cast< long >(animEnd));
		options.m_animEnd = options.m_context.getTime();

		options.m_context.setTime(options.m_animStart);
		options.m_animFrames = animEnd - animStart + 1;
	}
	else {
		options.m_animStart = options.m_animEnd = CHgetEvalTime();
		options.m_context.setTime(options.m_animStart);
		options.m_animFrames = 1;
	}

	options.m_exportAsSingle = NOT(args.found('m'));
	if (options.m_exportAsSingle) {
		options.m_lastAsPreview = args.found('l');
	}

	options.m_applyTransform = args.found('t');

	options.m_exportVelocity = args.found('v');
	if (options.m_exportVelocity) {
		options.m_velocityStart = args.fargp('v', 0);
		options.m_velocityEnd = args.fargp('v', 1);
		options.m_velocityStart = std::min(options.m_velocityStart, options.m_velocityEnd);
		options.m_velocityEnd = std::max(options.m_velocityStart, options.m_velocityEnd);
	}

	int simplType = (args.found('T'))? args.iargp('T', 0) : -1;
	switch (simplType) {
		case 0:
		{
			options.m_simplificationType = VUtils::SIMPLIFY_FACE_SAMPLING;
			break;
		}
		case 1:
		{
			options.m_simplificationType = VUtils::SIMPLIFY_CLUSTERING;
			break;
		}
		case 2:
		{
			options.m_simplificationType = VUtils::SIMPLIFY_EDGE_COLLAPSE;
			break;
		}
		case 3:
		{
			options.m_simplificationType = VUtils::SIMPLIFY_COMBINED;
			break;
		}
		default:
		{
			options.m_simplificationType = VUtils::SIMPLIFY_COMBINED;
		}
	}

	if (args.found('F')) {
		options.m_maxPreviewFaces = args.iargp('F', 0);
	}

	if (args.found('H')) {
		options.m_maxPreviewStrands = args.iargp('H', 0);
	}

	if (args.found('X')) {
		options.m_maxFacesPerVoxel = args.iargp('X', 0);
	}

	options.m_exportPCLs= args.found('P');
	if (options.m_exportPCLs) {
		options.m_pointSize = args.fargp('P', 0);
	}

	return CMD_ARG_OK;
}


/// Return list of SOP nodes passed to vrayproxy command that should be exported to a .vrmesh file
/// @param[in] args - command line arguments passed to vrayproxy cmd
/// @param[out] sopList - SOP nodes matching the criterias from command line aruments
///                       will be appended to this list
/// @retval number of SOPs found
static int getSOPList(CMD_Args &args, SOPList &sopList)
{
	int ignoreHidden = NOT(args.found('i'));

	UT_String nodeMask;
	// strip known options from cmd
	args.stripOptions(CMD::vrayproxyFormat);
	// take remaining args as node mask
	args.makeCommandLine(nodeMask, 1);

	int nSOPs = sopList.size();

	// get a manager that contains objects
	OP_Network *rootnet = OPgetDirector()->getManager("obj");

	UT_ASSERT( rootnet );

	// create internal bundle that will contain all nodes matching nodeMask
	UT_String bundleName;
	OP_Bundle *bundle = OPgetDirector()->getBundles()->getPattern(bundleName,
																  rootnet,
																  rootnet,
																  nodeMask,
																  PRM_SpareData::objGeometryPath.getOpFilter());

	// get the node list for processing
	for (int i = 0; i < bundle->entries(); ++i) {
		OP_Node *opnode = bundle->getNode(i);

		UT_ASSERT( opnode );

		OBJ_Node *objNode = opnode->castToOBJNode();
		if (NOT(objNode)) {
			continue;
		}

		UT_String fullpath;
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

	// release the internal bundle created by getPattern()
	OPgetDirector()->getBundles()->deReferenceBundle(bundleName);

	return sopList.size() - nSOPs;
}


void CMD::vrayproxy(CMD_Args &args)
{
	VRayProxyExportOptions options;
	if (parseExportOptions(args, options) != CMD_ARG_OK) {
		args.err() << "ERROR Invalid usage: No filepath specified." << std::endl;
		return;
	}

	SOPList sopList;
	if (getSOPList(args, sopList) <= 0) {
		args.err() << "ERROR Invalid usage: No valid geometry specified." << std::endl;
		return;
	}

	VUtils::ErrorCode err = VRayProxyExporter::doExport(options, sopList);
	if (err.error()) {
		args.err() << err.getErrorString().ptr() << "\n";
	}

	args.out() << "vrayproxy DONE" << std::endl;
}
