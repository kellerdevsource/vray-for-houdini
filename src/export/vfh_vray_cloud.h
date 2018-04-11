//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//  https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VFH_VRAY_CLOUD_H
#define VRAY_FOR_HOUDINI_VFH_VRAY_CLOUD_H

#include "vfh_vray.h"

#include <QStringList>
#include <QPair>

namespace VRayForHoudini {

class JobFilePath
{
public:
	/// Creates new file path at temporal writable location.
	explicit JobFilePath(const QString &filePath);

	/// Removes created file path if exists.
	~JobFilePath();

	/// Checks if file path is valid.
	bool isValid() const { return !filePath.isEmpty(); }

	/// Returns file path.
	QString getFilePath() const { return filePath; }

	/// Creates new file path.
	static QString createFilePath();

	/// Remove file path.
	static void removeFilePath(const QString &filePath);

private:
	/// Temporary file path for export.
	const QString filePath;
};

namespace Cloud {

struct Job {
	typedef QPair<int, int> FrameRange;

	explicit Job(const QString &sceneFile);

	/// Sets project name filtering incompatible characters.
	void setProject(const QString &value);

	/// Returns project name.
	QString getProject() const { return project; }

	/// Sets job name filtering incompatible characters.
	void setName(const QString &value);

	/// Returns project name.
	QString getName() const { return name; }

	/// Path to a .vrscene file (required).
	const QString sceneFile;

	/// Render Mode (bucket, progressive).
	QString renderMode = "bucket";

	/// Width of the rendered image.
	int width{1920};

	/// Height of the rendered image.
	int height{1080};

	/// Render animation instead of a single shot.
	int animation{false};

	/// Frame Range of the animation.
	FrameRange frameRange{0, 0};

	/// Frame Step of the animation.
	int frameStep{1};

	/// Render VR-enabled image.
	int vr{false};

	/// Ignore warnings in scene file processing.
	int ignoreWarnings{true};

	/// Do not display progress while submitting a job.
	int noProgress{false};

	/// Path to a color corrections file.
	QString colorCorrectionsFile;

	/// Returns RenderMode as string argument value.
	/// @param renderMode Render mode.
	static QString renderModeToString(const VRay::RendererOptions::RenderMode renderMode) {
		switch (renderMode) {
			case VRay::RendererOptions::RENDER_MODE_PRODUCTION: return "-1";
			case VRay::RendererOptions::RENDER_MODE_RT_CPU: return "0";
			case VRay::RendererOptions::RENDER_MODE_RT_GPU_OPENCL: return "1";
			case VRay::RendererOptions::RENDER_MODE_RT_GPU_CUDA: return "4";
			case VRay::RendererOptions::RENDER_MODE_PRODUCTION_OPENCL: return "101";
			case VRay::RendererOptions::RENDER_MODE_PRODUCTION_CUDA: return "104";
			default:
				return "-1";
		}
	}

	/// Converts settings values to command line arguments.
	/// @param job Job settings.
	/// @param[out] arguments Command line arguments list.
	static void toArguments(const Job &job, QStringList &arguments);

private:
	/// Name of the rendering project.
	QString project;

	/// Name of the rendering job.
	QString name;
};

/// Submits job to the V-Ray Cloud using command line V-Ray Cloud Client.
/// @param job Job settings.
int submitJob(const Job &job);

/// Check if V-Ray Cloud Client is available at the system.
int isClientAvailable();

} // namespace Cloud
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VFH_VRAY_CLOUD_H
