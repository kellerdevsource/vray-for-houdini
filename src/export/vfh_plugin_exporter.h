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

#include "vfh_defines.h"
#include "vfh_vray.h"
#include "vfh_log.h"
#include "vfh_plugin_attrs.h"

#include <boost/function.hpp>
#include <QString>

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
typedef boost::function<void (VRay::VRayRenderer&, bool isRendering)>                         CbOnRenderLast;
typedef boost::function<void (VRay::VRayRenderer&)>                                           CbOnVFBClosed;


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
typedef CbBase<CbOnRenderLast>      CbSetOnRenderLast;
typedef CbBase<CbOnVFBClosed>       CbSetOnVFBClosed;


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
		onRenderLast.clear();
		onVFBClosed.clear();
	}

	CbSetOnRendererClose   m_cbOnRendererClose;
	CbSetOnImageReady      m_cbOnImageReady;
	CbSetOnRTImageUpdated  m_cbOnRTImageUpdated;
	CbSetOnBucketInit      m_cbOnBucketInit;
	CbSetOnBucketFailed    m_cbOnBucketFailed;
	CbSetOnBucketReady     m_cbOnBucketReady;
	CbSetOnDumpMessage     m_cbOnDumpMessage;
	CbSetOnProgress        m_cbOnProgress;

	CbSetOnRenderLast onRenderLast;
	CbSetOnVFBClosed onVFBClosed;

	VfhDisableCopy(CbCollection)
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
	void setRendererMode(int mode);

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

	void addCbOnVfbClose(CbOnVFBClosed cb) { m_callbacks.onVFBClosed.add(cb); }
	void addCbOnVfbClose(CbVoid cb)        { m_callbacks.onVFBClosed.add(cb); }

	void addCbOnRenderLast(CbOnRenderLast cb) { m_callbacks.onRenderLast.add(cb); }
	void addCbOnRenderLast(CbVoid cb)         { m_callbacks.onRenderLast.add(cb); }

	/// Clear registered render callbacks
	void resetCallbacks();

	/// Check if VRay::VRayRenderer is instantiated
	bool isVRayInit() const { return !!m_vray; }

	/// Get the actual renderer instance
	VRay::VRayRenderer& getVRay() { return *m_vray; }

	/// Reset scene data.
	void reset() const;

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
	VRay::VRayRenderer *m_vray;

	/// A collection of registered render callbacks.
	CbCollection m_callbacks;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H
