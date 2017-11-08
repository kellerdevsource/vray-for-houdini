//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H
#define VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H

#include "vfh_vray.h"
#include "vfh_plugin_attrs.h"

#include <QString>

namespace VRayForHoudini {

/// V-Ray Frame Buffer settings.
struct VFBSettings {
	int structVersion = 0;
	int vfbx = 0;
	int vfby = 0;
	int vfbWidth = 0;
	int vfbHeight = 0;
	int ccx = 0;
	int ccy = 0;
	int infox = 0;
	int infoy = 0;
	int lex = 0;
	int ley = 0;
	uint64 flags64 = 0;
	int flagsrollouts = 0;
	int posSaved = false;
	int isRenderRegionValid = false;
	int rrLeft = 0;
	int rrTop = 0;
	int rrWidth = 0;
	int rrHeight = 0;
	double rrLeftNorm = 0.0;
	double rrTopNorm = 0.0;
	double rrWidthNorm = 0.0;
	double rrHeightNorm = 0.0;

	/// Fills VFBSettings from state buffer.
	/// @param stateBuf Buffer obtained with VFB::getState().
	/// @param stateBufSize Buffer size.
	/// @param settings Output VFBSettings struct.
	static void fillVfbSettings(void *stateBuf, int stateBufSize, VFBSettings &settings);
};

/// Wraps around VRay::VRayRenderer to provide some commonly used
/// functionality and hide implementation details of initilizing
/// the renderer instance, reseting callbacks, creating and/or
/// updating plugins.
class VRayPluginRenderer
{
public:
	/// Test if we have a license for VRScans advanced parameters GUI
	/// @param err[out] - use it to diagnose licensing problems when the
	///        license test fails
	/// @retval true if we do, false on no license or error
	static bool hasVRScansGUILicense(VRay::ScannedMaterialLicenseError &err);

	VRayPluginRenderer();
	~VRayPluginRenderer();

	/// Create and initilize or reset the V-Ray renderer instance.
	/// @param enableVFB Enable VFB
	/// @param reInit Re-create V-Ray instance.
	/// @returns true on success.
	int initRenderer(int enableVFB, int reInit);

	/// Delete the V-Ray renderer instance (if any)
	void freeMem();

	/// Search for a plugin instance with the given name.
	/// @param pluginName[in] -
	/// @retval invalid Plugin object if not found or not
	///         initilized
	VRay::Plugin getPlugin(const char *pluginName);

	/// Create or update a plugin from a plugin description
	/// @param pluginDesc - plugin description with relevant properties set
	/// @retval invalid Plugin object if not successul
	VRay::Plugin exportPlugin(const Attrs::PluginDesc &pluginDesc);

	/// Update a plugin properties from a plugin description
	/// @param plugin - the plugin to update
	/// @param pluginDesc - plugin description with relevant properties set
	void exportPluginProperties(VRay::Plugin &plugin, const Attrs::PluginDesc &pluginDesc);

	/// Delete plugin for a given plugin description
	/// @param pluginDesc - plugin description with plugin name set
	void removePlugin(const Attrs::PluginDesc &pluginDesc, int checkExisting=true);

	/// Delete plugin with the given name
	/// @param pluginName - plugin name
	void removePlugin(const std::string &pluginName, int checkExisting=true);

	/// Delete plugin.
	/// @param plugin V-Ray plugin instance.
	void removePlugin(VRay::Plugin plugin);

	/// Commits any accumulated scene changes. This is necessary if
	/// the autoCommit is set to false on the renderer instance.
	void commit();

	/// Change the autoCommit state of the renderer. When true, every parameter
	/// change is applied immediately. Otherwise you have to call commit() yourself
	/// to apply changes. This allows you to batch changes together for efficiency.
	void setAutoCommit(const bool enable);

	/// When enabled, setting parameter values will add keyframe values for animation.
	/// Otherwise the old value is just overwritten. The default state is off.
	void setAnimation(bool on);

	/// Sets the current time for the scene. This is related to the current frame
	/// through the SettingsOutput plugin.
	void setCurrentTime(fpreal time);

	/// This method should only be used when doing out-of-process rendering
	/// i.e. VRay::RendererOptions::inProcess=false. It updates the camera plugin
	/// used on DR machines when adding/removing CameraPhysical or another specific
	/// camera.
	void setCamera(VRay::Plugin camera);

	/// Change the render mode to be used for when *next* rendering starts. If there is
	/// a current rendering running it will not be affected. You can switch between
	/// Production and RT mode with this without resetting the scene.
	/// Valid modes are Production, RT CPU, RT GPU (CUDA)
	void setRendererMode(VRay::RendererOptions::RenderMode mode);

	/// Sets the frame buffer width and height.
	void setImageSize(const int w, const int h);

	/// Show/hide VFB
	void showVFB(bool show=true, const char *title=nullptr);

	/// Removes all keyframe values at times less than 'toTime'
	void clearFrames(double toTime) const;

	/// Start rendering at the current time.
	/// @param locked[in] - when true this will force the current thread to block
	///        until rendering is done. By default this is a non-blocking call
	/// @retval 0 - no error
	int startRender(int locked=false);

	/// Start rendering an animation sequence in a separate thread.
	/// @param start[in] - animation start time
	/// @param end[in] - animation end time
	/// @param step[in] - animation time step
	/// @param locked[in] - when true this will force the current thread to block
	///        until rendering is done. By default this is a non-blocking call
	/// @retval 0 - no error
	int startSequence(int start, int end, int step, int locked=false);

	/// Flags the image rendering thread to stop and waits for it to join.
	void stopRender();

	/// Exports the current scene contents to the specified file. AppSDK
	/// serializes all plugins in text format.
	/// @param filepath The path to the file where the scene will be exported. The
	/// file path must contain the name and extension of the destination file.
	/// @param settings Additional options such as compression and file splitting
	/// @retval 0 - no error
	int exportScene(const std::string &filepath, VRay::VRayExportSettings &settings);

	/// Check if VRay::VRayRenderer is instantiated
	bool isVRayInit() const { return !!m_vray; }

	/// Checks rendering is in progress.
	bool isRendering() const;

	/// Get the actual renderer instance
	VRay::VRayRenderer &getVRay() const { return *m_vray; }

	/// Reset scene data.
	void reset();

	/// Checks if VFB is enabled for this instance.
	bool isVfbEnabled() const { return m_enableVFB; }

	/// Saves VFB state.
	/// @param stateData Output state as Base64 string.
	void saveVfbState(QString &stateData) const;

	/// Restores VFB state.
	/// @param stateData State data as Base64 string.
	void restoreVfbState(const QString &stateData) const;

	/// Obtains VFB state as VFBSettings.
	/// @param settings Output state as VFBSettings.
	void getVfbSettings(VFBSettings &settings) const;

private:
	/// V-Ray renderer instance.
	VRay::VRayRenderer *m_vray{nullptr};

	/// Flag indicating that we should use/init VFB related options.
	int m_enableVFB{false};
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H
