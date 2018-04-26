//
// Copyright (c) 2015-2018, Chaos Software Ltd
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

#include "getenvvars.h"
#include "voxelsubdivider.h"

#include <ROP/ROP_Error.h>
#include <OBJ/OBJ_Node.h>
#include <FS/FS_Info.h>
#include <FS/FS_FileSystem.h>
#include <UT/UT_Assert.h>

#include <QProcess>
#include <QStringList>
#include <CH/CH_Segment.h>

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
	exporter.getObjectExporter().setPartitionAttribute(m_options.m_partitionAttribute);
	return VUtils::ErrorCode();
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

	float startFrame = FLT_MAX;
	float endFrame = -FLT_MAX;
	for (int f = 0; f < options.m_animFrames; ++f) {
		if (err.error()) {
			break;
		}

		options.m_context.setTime(options.m_animStart);
		options.m_context.setFrame(options.m_context.getFrame() + f);

		// find first and last float frame with data
		startFrame = VUtils::Min(startFrame, static_cast<float>(options.m_context.getFloatFrame()));
		endFrame = VUtils::Max(endFrame, static_cast<float>(options.m_context.getFloatFrame()));

		// set context here so we have frame data when exporting
		exporter.setContext(options.m_context);

		// export all items in the sop list
		for (int i = 0; i < sopList.size(); ++i) {
			SOP_Node *sopNode = sopList(i);
			if (sopNode) {
				OBJ_Node *objNode = CAST_OBJNODE(sopNode->getParentNetwork());
				if (objNode) {
					const VRay::Plugin &nodePlugin = exporter.getObjectExporter().exportNode(*objNode, sopNode);
					if (nodePlugin.isEmpty()) {
						err.setError(__FUNCTION__,
							SOP_ERR_FILEGEO,
							"Could not export \"%s\" as proxy.", sopNode->getName());
					}
				}
			}
		}
	}

	// convert data to .vrmesh file
	convertData(startFrame, endFrame);

	return err;
}

VUtils::ErrorCode VRayProxyExporter::convertData(float start, float end)
{
	VUtils::ErrorCode err;

	SOP_Node *sopNode = sopList(0);

	UT_String vrmeshPath = m_options.getFilepath(*sopNode);
	const bool isAppendMode = m_options.isAppendMode();

	FS_Info fsInfo(vrmeshPath);
	if (fsInfo.fileExists() &&
		!isAppendMode &&
		!m_options.m_overwrite)
	{
		err.setError(__FUNCTION__,
					 ROP_FILE_EXISTS,
					 "File already exists: %s", vrmeshPath.buffer());
		return err;
	}

	QString vrscenePath(vrmeshPath.buffer());
	vrscenePath = vrscenePath.replace(SL(".vrmesh"), SL(".vrscene"));

	VRay::VRayExportSettings settings;
	settings.compressed = true;
	settings.currentFrameOnly = false;
	settings.renderElementsSeparateFolders = false;
	if (exporter.exportVrscene(vrscenePath, settings) != 0) {
		err.setError(__FUNCTION__,
		             ROP_SAVE_ERROR,
		             "Failed to write intermediate file: %s", _toChar(vrscenePath));
		return err;
	}
	exporter.getRenderer().reset();

	// NOTE: Keep the order of theese, the enum value is used to index the array
	static const char * simplificationType[] = {
		"face_sampling", // VUtils::SimplificationType::SIMPLIFY_FACE_SAMPLING
		"clustering",    // VUtils::SimplificationType::SIMPLIFY_CLUSTERING
		"edge_collapse", // VUtils::SimplificationType::SIMPLIFY_EDGE_COLLAPSE
		"combined",      // VUtils::SimplificationType::SIMPLIFY_COMBINED
	};


	QStringList arguments;
	arguments << vrscenePath << vrmeshPath.buffer()
		<< "-vrsceneWholeScene"
		<< "-facesPerVoxel" << QString::number(m_options.m_maxFacesPerVoxel > 0 ? m_options.m_maxFacesPerVoxel : INT_MAX)
		<< "-previewFaces"  << QString::number(m_options.m_maxPreviewFaces)
		<< "-previewHairs"  << QString::number(m_options.m_maxPreviewStrands);

	if (m_options.m_simplificationType >= VUtils::SimplificationType::SIMPLIFY_FACE_SAMPLING
		&& m_options.m_simplificationType <= VUtils::SimplificationType::SIMPLIFY_COMBINED) {
		arguments << "-previewType" << simplificationType[m_options.m_simplificationType];
	}

	const int frameStart = static_cast<int>(start);
	// Round up here because if we have mblur data that is not on integer frame we will need to export it too
	const int frameEnd = static_cast<int>(end + 0.5);

	arguments << "-vrsceneFrames" << (QString::number(frameStart) + "-" + QString::number(frameEnd));

	static VUtils::GetEnvVar appsdkPathVar("VRAY_APPSDK", "");
	const QString appsdkPath = QString(appsdkPathVar.getValue().ptr());
	if (appsdkPath.isEmpty() || appsdkPath.isNull()) {
		err.setError(__FUNCTION__,
		             ROP_EXECUTE_ERROR,
			         "Missing env variable VFH_THREADED_LOGGER");
		return err;
	}

#ifdef WIN32
	const QString ply2vrmeshExe = "ply2vrmesh.exe";
#else
	const QString ply2vrmeshExe = "ply2vrmesh.bin";
#endif

	Log::getLog().debug("ply2vrmesh %s", _toChar(arguments.join(" ")));

	QProcess ply2vrmesh;
	ply2vrmesh.start(appsdkPath + "/bin/" + ply2vrmeshExe, arguments);
	const bool started = ply2vrmesh.waitForStarted();
	const bool finished = ply2vrmesh.waitForFinished(-1);

	if (!started || !finished) {
		err.setError(__FUNCTION__, ROP_EXECUTE_ERROR,
			"Failed to execute ply2vrmesh (started %s, finished %s)",
			                               started ? "true" : "false",
			                               finished ? "true" : "false");
	}

	// We split this in lines since vfh_log has 1k max message size
	const auto outLines = ply2vrmesh.readAll().split('\n');
	for (const auto & line : outLines) {
		Log::getLog().debug("pl2vrmesh: %s", line.toStdString().c_str());
	}

	// Keep the .vrscene file arround when debugging - usefull to check if it is missing data
	// or if the ply2vrmesh did not convert correctly
#ifndef VFH_DEBUG
	std::remove(vrscenePath);
#endif

	return err;
}
