//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_vrayproxy.h"
#include "vfh_export_mesh.h"
#include "vfh_export_hair.h"

#include <ROP/ROP_Error.h>
#include <OBJ/OBJ_Node.h>
#include <FS/FS_Info.h>
#include <FS/FS_FileSystem.h>
#include <UT/UT_Assert.h>

#include <uni.h>
#include <voxelsubdivider.h>

using namespace VRayForHoudini;

VRayProxyExportOptions::VRayProxyExportOptions()
	: m_filepath(UT_String::ALWAYS_DEEP)
	, m_mkpath(true)
	, m_overwrite(false)
	, m_exportAsSingle(true)
	, m_exportAsAnimation(false)
	, m_animStart(0)
	, m_animEnd(0)
	, m_animFrames(0)
	, m_lastAsPreview(false)
	, m_applyTransform(false)
	, m_exportVelocity(false)
	, m_velocityStart(0.f)
	, m_velocityEnd(0.05f)
	, m_simplificationType(VUtils::SIMPLIFY_COMBINED)
	, m_maxPreviewFaces(100)
	, m_maxPreviewStrands(100)
	, m_maxFacesPerVoxel(0)
	, m_exportPCLs(false)
	, m_pointSize(0.5f)
{}

bool VRayProxyExportOptions::isAppendMode() const
{
	return m_exportAsAnimation &&
	       m_animFrames > 1 &&
	       m_context.getTime() > m_animStart;
}

UT_String VRayProxyExportOptions::getFilepath(const SOP_Node &sop) const
{
	UT_ASSERT(m_filepath.isstring());

	UT_String filepath = m_filepath;

	if (NOT(filepath.matchFileExtension(".vrmesh"))) {
		filepath += ".vrmesh";
	}

	if (NOT(m_exportAsSingle)) {
		UT_String soppathSuffix;
		sop.getFullPath(soppathSuffix);
		soppathSuffix.forceAlphaNumeric();
		soppathSuffix += ".vrmesh";
		filepath.replaceSuffix(".vrmesh", soppathSuffix);
	}

	return filepath;
}

VRayProxyExporter::VRayProxyExporter(const VRayProxyExportOptions &options, const SOPList &sopList, ROP_Node *ropNode)
	: sopList(sopList)
	, m_rop(ropNode)
	, m_options(options)
	, exporter(ropNode)
{}

VUtils::ErrorCode VRayProxyExporter::init()
{
	exporter.getRenderer().initRenderer(false, true);
	VUtils::ErrorCode err;

	for (int i = 0; i < sopList.size(); ++i) {
		SOP_Node *sopNode = sopList(i);
		if (sopNode) {
			OBJ_Node *objNode = CAST_OBJNODE(sopNode->getParentNetwork());
			if (objNode) {
				if (!exporter.exportSOP(objNode, sopNode)) {
					err.setError(__FUNCTION__,
						SOP_ERR_FILEGEO,
						"Could not export \"%s\" as proxy.", sopNode->getName().c_str());
				}
			}
		}
	}

	return err;
}


static VUtils::ErrorCode _doExport(VRayProxyExportOptions &options, const SOPList &sopList)
{
	VRayProxyExporter exporter(options, sopList, nullptr);

	VUtils::ErrorCode err = exporter.init();
	if (!err.error()) {
		err = exporter.doExportFrame();
	}

	return err;
}

VUtils::ErrorCode VRayProxyExporter::doExport(VRayProxyExportOptions &options, const SOPList &sopList)
{
	VUtils::ErrorCode err;

	if (NOT(options.m_filepath.isstring())) {
		err.setError(__FUNCTION__,
					 ROP_MISSING_FILE,
					 "Invalid file specified.");
		return err;
	}

	if (sopList.size() <= 0) {
		err.setError(__FUNCTION__,
					 ROP_BAD_CONTEXT,
					 "No geometry valid found.");
		return err;
	}

	UT_String dirpath;
	UT_String filename;
	options.m_filepath.splitPath(dirpath, filename);

	// create parent dirs if necessary
	FS_Info fsInfo(dirpath);
	if (NOT(fsInfo.exists())) {
		FS_FileSystem fsys;
		if (   NOT(options.m_mkpath)
			|| NOT(fsys.createDir(dirpath)))
		{
			err.setError(__FUNCTION__,
						 ROP_CREATE_DIRECTORY_FAIL,
						 "Failed to create parent directory.");
			return err;
		}
	}

	for (int f = 0; f < options.m_animFrames; ++f) {
		if (err.error()) {
			break;
		}

		options.m_context.setTime(options.m_animStart);
		options.m_context.setFrame(options.m_context.getFrame() + f);

		if (options.m_exportAsSingle) {
			err = _doExport(options, sopList);
		}
		else {
			for (int sopIdx = 0; sopIdx < sopList.size(); ++sopIdx) {
				SOPList singleItem;
				singleItem.append(sopList(sopIdx));

				err = _doExport(options, singleItem);
			}
		}
	}

	return err;
}

VUtils::ErrorCode VRayProxyExporter::doExportFrame()
{
	VUtils::ErrorCode err;

	SOP_Node *sopNode = sopList(0);

	UT_String filepath = m_options.getFilepath(*sopNode);
	bool isAppendMode = m_options.isAppendMode();

	FS_Info fsInfo(filepath);
	if (   fsInfo.fileExists()
		&& NOT(isAppendMode)
		&& NOT(m_options.m_overwrite)
		)
	{
		err.setError(__FUNCTION__,
					 ROP_FILE_EXISTS,
					 "File already exists: %s", filepath.buffer());
		return err;
	}
	std::string sceneName = filepath.toStdString();
	sceneName.replace(sceneName.begin() + sceneName.rfind(".vrmesh"), sceneName.end(), ".vrscene");

	VRay::VRayExportSettings settings;
	settings.compressed = true;
	settings.framesInSeparateFiles = false;
	settings.renderElementsSeparateFolders = false;
	if (exporter.exportVrscene(sceneName, settings) != 0) {
		err.setError(__FUNCTION__,
		             ROP_SAVE_ERROR,
		             "Failed to write intermediate file: %s", sceneName.c_str());
		return err;
	}

	// TODO: run ply2vrmesh for the exported vrscene
	//std::remove(sceneName.c_str());

//	VUtils::SubdivisionParams subdivParams;
//	subdivParams.facesPerVoxel = (m_options.m_maxFacesPerVoxel > 0)? m_options.m_maxFacesPerVoxel : INT_MAX;

	return err;
}
