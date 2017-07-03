//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H
#define VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H

#include "vfh_defines.h"
#include "vfh_vray.h"
#include "vfh_log.h"
#include "vfh_plugin_attrs.h"

#include <boost/function.hpp>

namespace VRayForHoudini {

typedef boost::function<void (void)>                                                          CbVoid;
typedef boost::function<void (VRay::VRayRenderer&)>                                           CbOnRendererClose;
typedef boost::function<void (VRay::VRayRenderer&)>                                           CbOnImageReady;
typedef boost::function<void (VRay::VRayRenderer&, VRay::VRayImage*)>                         CbOnRTImageUpdated;
typedef boost::function<void (VRay::VRayRenderer&, int, int, int, int, const char*)>          CbOnBucketInit;
typedef boost::function<void (VRay::VRayRenderer&, int, int, int, int, const char*)>          CbOnBucketFailed;
typedef boost::function<void (VRay::VRayRenderer&, int, int, const char*, VRay::VRayImage*)>  CbOnBucketReady;
typedef boost::function<void (VRay::VRayRenderer&, const char*, int)>                         CbOnDumpMessage;
typedef boost::function<void (VRay::VRayRenderer&, const char*, int, int)>                    CbOnProgress;


template <typename CbT>
struct CbBase
{
	CbBase() {}

	typedef std::vector<CbVoid>  CbVoidArray;
	typedef std::vector<CbT>     CbTypeArray;

	void add(CbT cb)    { m_cbTyped.push_back(cb); }
	void add(CbVoid cb) { m_cbVoid.push_back(cb);  }

	void clear() {
		m_cbVoid.clear();
		m_cbTyped.clear();
	}

	CbVoidArray m_cbVoid;
	CbTypeArray m_cbTyped;

	VfhDisableCopy(CbBase)
};

typedef CbBase<CbOnRendererClose>   CbSetOnRendererClose;
typedef CbBase<CbOnImageReady>      CbSetOnImageReady;
typedef CbBase<CbOnRTImageUpdated>  CbSetOnRTImageUpdated;
typedef CbBase<CbOnBucketInit>      CbSetOnBucketInit;
typedef CbBase<CbOnBucketFailed>    CbSetOnBucketFailed;
typedef CbBase<CbOnBucketReady>     CbSetOnBucketReady;
typedef CbBase<CbOnDumpMessage>     CbSetOnDumpMessage;
typedef CbBase<CbOnProgress>        CbSetOnProgress;


struct CbCollection {
	CbCollection() {}

	void clear() {
		m_cbOnRendererClose.clear();
		m_cbOnImageReady.clear();
		m_cbOnRTImageUpdated.clear();
		m_cbOnBucketInit.clear();
		m_cbOnBucketFailed.clear();
		m_cbOnBucketReady.clear();
		m_cbOnDumpMessage.clear();
		m_cbOnProgress.clear();
	}

	CbSetOnRendererClose   m_cbOnRendererClose;
	CbSetOnImageReady      m_cbOnImageReady;
	CbSetOnRTImageUpdated  m_cbOnRTImageUpdated;
	CbSetOnBucketInit      m_cbOnBucketInit;
	CbSetOnBucketFailed    m_cbOnBucketFailed;
	CbSetOnBucketReady     m_cbOnBucketReady;
	CbSetOnDumpMessage     m_cbOnDumpMessage;
	CbSetOnProgress        m_cbOnProgress;

	VfhDisableCopy(CbCollection)
};

/// Wraps around VRay::VRayRenderer to provide some commonly used
/// functionality and hide implementation details of initilizing
/// the renderer instance, reseting callbacks, creating and/or
/// updating plugins.
class VRayPluginRenderer
{
public:
	/// Initilize AppSDK and V-Ray renderer context.
	/// You'll need to initilize once, before creating a V-Ray renderer instance.
	/// @retval true on success
	static bool initialize();

	/// Deinitilize AppSDK and V-Ray renderer context. Free the license.
	static void deinitialize();

	/// Test if we have a license for VRScans advanced parameters GUI
	/// @param err[out] - use it to diagnose licensing problems when the
	///        license test fails
	/// @retval true if we do, false on no license or error
	static bool hasVRScansGUILicense(VRay::ScannedMaterialLicenseError &err);

public:
	VRayPluginRenderer();
	~VRayPluginRenderer();

	/// Create and initilize or reset the V-Ray renderer instance.
	/// @param hasUI[in] - true to enable the VFB, false if VFB is not needed
	///        for example when running Houdini in non-GUI mode
	/// @param reInit[in] - true to re-create the V-Ray renderer instance
	///        otherwise it only resets the existing instance
	/// @retval true on success
	int initRenderer(int hasUI, int reInit);

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
	void removePlugin(const Attrs::PluginDesc &pluginDesc);

	/// Delete plugin with the given name
	/// @param pluginName - plugin name
	void removePlugin(const std::string &pluginName);

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
	void setRendererMode(int mode);

	/// Sets the frame buffer width and height.
	void setImageSize(const int w, const int h);

	/// Show/hide VFB
	void showVFB(bool show=true, const char *title=nullptr);

	/// Removes all keyframe values at times less than 'toTime'
	void clearFrames(float toTime);

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

	/// Register callbacks on different events dispatched from the renderer.
	/// More than one callback per event can be registered. The order of invocation
	/// will follow the order of registration. Each event supports 2 types of callbacks:
	/// 1. a callback that does require any arguments
	/// 2. a callback that accepts strongly typed arguments
	void addCbOnRendererClose(CbOnRendererClose cb)   { m_callbacks.m_cbOnRendererClose.add(cb); }
	void addCbOnRendererClose(CbVoid cb)              { m_callbacks.m_cbOnRendererClose.add(cb); }
	void addCbOnImageReady(CbOnImageReady cb)         { m_callbacks.m_cbOnImageReady.add(cb); }
	void addCbOnImageReady(CbVoid cb)                 { m_callbacks.m_cbOnImageReady.add(cb); }
	void addCbOnRTImageUpdated(CbOnRTImageUpdated cb) { m_callbacks.m_cbOnRTImageUpdated.add(cb); }
	void addCbOnRTImageUpdated(CbVoid cb)             { m_callbacks.m_cbOnRTImageUpdated.add(cb); }
	void addCbOnBucketInit(CbOnBucketInit cb)         { m_callbacks.m_cbOnBucketInit.add(cb); }
	void addCbOnBucketInit(CbVoid cb)                 { m_callbacks.m_cbOnBucketInit.add(cb); }
	void addCbOnBucketReady(CbOnBucketReady cb)       { m_callbacks.m_cbOnBucketReady.add(cb); }
	void addCbOnBucketReady(CbVoid cb)                { m_callbacks.m_cbOnBucketReady.add(cb); }
	void addCbOnBucketFailed(CbOnBucketFailed cb)     { m_callbacks.m_cbOnBucketFailed.add(cb); }
	void addCbOnBucketFailed(CbVoid cb)               { m_callbacks.m_cbOnBucketFailed.add(cb); }
	void addCbOnDumpMessage(CbOnDumpMessage cb)       { m_callbacks.m_cbOnDumpMessage.add(cb); }
	void addCbOnDumpMessage(CbVoid cb)                { m_callbacks.m_cbOnDumpMessage.add(cb); }
	void addCbOnProgress(CbOnProgress cb)             { m_callbacks.m_cbOnProgress.add(cb); }
	void addCbOnProgress(CbVoid cb)                   { m_callbacks.m_cbOnProgress.add(cb); }

	/// Clear registered render callbacks
	void resetCallbacks();

	/// Check if VRay::VRayRenderer is instantiated
	bool isVRayInit() const { return !!m_vray; }

	/// Get the actual renderer instance
	VRay::VRayRenderer& getVRay() { return *m_vray; }

private:
	struct RenderRegion {
		RenderRegion()
			: left(-1)
			, top(-1)
			, width(-1)
			, height(-1)
			, saved(false)
		{}

		bool isValid() const {
			return saved && left >= 0 && top >= 0 && width > 0 && height > 0;
		}

		int left;
		int top;
		int width;
		int height;
		bool saved;
	};

	RenderRegion                  m_savedRegion; ///< Saves render regions between renders
	VRay::VRayRenderer           *m_vray; ///< V-Ray renderer instance
	CbCollection                  m_callbacks; ///< collection of registered render callbacks
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H
