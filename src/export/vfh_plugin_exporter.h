//
// Copyright (c) 2015-2018, Chaos Software Ltd
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

struct SettingsRTEngine
{
    /// Maximum trace depth for reflections/refractions etc.
    int trace_depth{5};

    /// Maximum trace depth for GI.
    int gi_depth{3};

    /// Number of samples to transfer over the network for RT-CPU.
    int cpu_bundle_size{64};

    /// Number of samples per pixel for RT-CPU.
    int cpu_samples_per_pixel{4};

    /// Number of samples to transfer over the network for RT-GPU.
    int gpu_bundle_size{256};

    /// Number of samples per pixel for RT-GPU.
    int gpu_samples_per_pixel{16};

    /// When true, RT GPU tries to utilize the GPUs with attached displays less. If this is true, it works best with gpu_samples_per_pixel=1 and gpu_bundle_size=64 (or less).
    bool low_gpu_thread_priority{false};

    /// true to enable coherent tracing of gi/reflections/refractions etc.
    bool coherent_tracing{false};

    /// Non-zero to enable side-by-side stereo rendering.
    bool stereo_mode{false};

    /// Distance between the two cameras for stereo mode.
    float stereo_eye_distance{6.5f};

    /// Focus mode (0 - none, 1 - rotation, 2 - shear).
    int stereo_focus{2};

    /// OpenCL Single Kernel maximum texture size - bigger textures are scaled to fit this size.
    int opencl_texsize{512};

    /// Textures transfer mode for the GPU.
    int opencl_resizeTextures{1};

    /// Format for the textures on the GPU (0 - 32-bit float per channel; 1 - 16-bit half float per channel; 2 - 8-bit per channel).
    int opencl_textureFormat{1};

    /// Progressive increase for 'Rays per pixel' (from 1 to real value). Use this for faster feadback.
    int progressive_samples_per_pixel{0};

    /// Non-zero to use undersampling, 0 otherwise. The value specifies the blur resolution. Value of n means 1/(2^n) initial resolution in each dimension.
    int undersampling{4};

    /// If true, RT will produce only RGBA. Default is false.
    bool disable_render_elements{false};

    /// Max render time (0 = inf).
    float max_render_time{0.0f};

    /// Max paths per pixel (0 = inf).
    int max_sample_level{10000};

    /// Noise threshold for the image sampler (0 = inf).
    float noise_threshold{0.001f};

    /// Show aa mask.
    bool enable_mask{false};

    /// Max time, in milliseconds, between (partial) image updates (0=disable partial image updates).
    int max_draw_interval{0};

    /// Min time, in milliseconds, between image updates (0=show all frames).
    int min_draw_interval{0};

    /// Flag used to disable some production-only features in interactive mode.
    int interactive{0};

    /// When using C++/CPU (CUDA), the noise pattern of the render is different compared to when using only CUDA GPU devices. If you want to mix CUDA C++/CPU renders and CUDA GPU renders, this should be set to 1. Otherwise, if you are using only CUDA GPU devices, this should be set to 0 (since the render results will be cleaner).
    int enable_cpu_interop{0};
};

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
	/// @param plugin The plugin to update.
	/// @param pluginDesc Plugin description with relevant properties.
	void exportPluginProperties(VRay::Plugin &plugin, const Attrs::PluginDesc &pluginDesc);

	/// Delete plugin with the given name
	/// @param pluginName - plugin name
	void removePlugin(const QString &pluginName) const;

	/// Delete plugin.
	/// @param plugin V-Ray plugin instance.
	void removePlugin(const VRay::Plugin &plugin) const;

	/// Commits any accumulated scene changes. This is necessary if
	/// the autoCommit is set to false on the renderer instance.
	void commit();

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
	void setRendererMode(const SettingsRTEngine &settingsRTEngine, VRay::VRayRenderer::RenderMode mode);

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

	/// Flags the image rendering thread to stop and waits for it to join.
	void stopRender();

	/// Exports the current scene contents to the specified file. AppSDK
	/// serializes all plugins in text format.
	/// @param filepath The path to the file where the scene will be exported. The
	/// file path must contain the name and extension of the destination file.
	/// @param settings Additional options such as compression and file splitting
	/// @retval 0 - no error
	int exportScene(const QString &filepath, VRay::VRayExportSettings &settings);

	/// Check if VRay::VRayRenderer is instantiated
	bool isVRayInit() const { return !!m_vray; }

	/// Checks rendering is in progress.
	bool isRendering() const;

	/// Get the actual renderer instance
	VRay::VRayRenderer &getVRay() const { return *m_vray; }

	/// Reset scene data.
	void reset();

	/// Unbind renderer callbacks.
	/// @param unbindUiButtons If false - removes callbacks except OnVFBClosed and OnRenderLast,
	/// removes all otherwise.
	void unbindCallbacks(int unbindUiButtons = false) const;

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
