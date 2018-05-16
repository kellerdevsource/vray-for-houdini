//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORTER_H
#define VRAY_FOR_HOUDINI_EXPORTER_H

#include "vfh_typedefs.h"
#include "vfh_vray.h"
#include "vfh_plugin_exporter.h"
#include "vfh_plugin_info.h"
#include "vfh_export_view.h"
#include "vfh_hashes.h"
#include "vfh_export_geom.h"
#include "vfh_log.h"
#include "vfh_export_context.h"

#include <OP/OP_Node.h>
#include <OBJ/OBJ_Node.h>
#include <ROP/ROP_Node.h>
#include <GSTY/GSTY_BundleMap.h>

namespace VRayForHoudini {

class VRayRendererNode;

enum class VfhSessionType {
	production = 0, ///< Production rendering mode. Same as "Render To Disk".
	rt, ///< RT session with VFB. 
	ipr, ///< IPR mode. Used for "Render View" and viewport rendering.
	cloud, ///< Submit scene to V-Ray Cloud.
};

struct VfhSessionData {
	/// Scene's active take.
	/// Used to restore scene take after IPR / RT sessions.
	TAKE_Take *sceneTake = nullptr;

	/// Session type.
	VfhSessionType type = VfhSessionType::production;
};

enum class ReturnValue {
	Error,
	Success
};

enum BitmapBufferColorSpace {
	bitmapBufferColorSpaceLinear = 0,
	bitmapBufferColorGammaCorrected = 1,
	bitmapBufferColorSRGB = 2,
};

enum class VRayPluginID {
	SunLight = 0,
	LightDirect,
	LightAmbient,
	LightOmni,
	LightSphere,
	LightSpot,
	LightRectangle,
	LightMesh,
	LightIES,
	LightDome,
	VRayClipper,
	MAX_PLUGINID
};

enum class VRayPluginType {
	UNKNOWN = 0,
	GEOMETRY,
	LIGHT,
	BRDF,
	MATERIAL,
	TEXTURE,
	CUSTOM_TEXTURE,
	UVWGEN,
	RENDERCHANNEL,
	EFFECT,
	OBJECT,
	SETTINGS,
};

enum VRayLightType {
	VRayLightOmni      = 0,
	VRayLightRectangle = 2,
	VRayLightSphere    = 4,
	VRayLightDome      = 6,
	VRayLightSun       = 8,
};

struct OpInterestItem {
	OpInterestItem(OP_Node *op_node=nullptr, OP_EventMethod cb=nullptr, void *cb_data=nullptr)
		: op_node(op_node)
		, cb(cb)
		, cb_data(cb_data)
	{}

	OP_Node *op_node;
	OP_EventMethod cb;
	void *cb_data;
};

typedef std::vector<OpInterestItem> CbItems;
typedef VUtils::StringHashMap<PluginList> StringPluginSetHashMap;
typedef VUtils::StringHashMap<VRay::Plugin> StringPluginMap;
typedef VUtils::HashMap<VRay::Plugin, OP_NodeList> PluginNodeListMap;
typedef VUtils::Table<VRay::Plugin> PluginTable;
typedef VUtils::Table<PluginTable*> PluginTables;

struct ConnectedPluginInfo {
	explicit ConnectedPluginInfo(VRay::Plugin plugin = VRay::Plugin(), const QString &output = "")
		: plugin(plugin)
		, output(output)
	{}

	/// Connected plugin.
	VRay::Plugin plugin;

	/// Connected output. May be empty.
	QString output;
};

/// Vfh main exporter. This is the main class responsible for translating
/// Houdini geometry and nodes to V-Ray plugins, initializing and starting
/// the rendering process.
class VRayExporter
{
public:
	friend class ObjectExporter;

	/// Available work modes for the exporter
	// NOTE: Keep in sync with "render_export_mode"
	enum ExpWorkMode {
		ExpRender = 0, ///< only render
		ExpExportRender, ///< export a vrscene and render
		ExpExport, ///< export a vrscene
	};

	explicit VRayExporter(OP_Node *rop);
	virtual ~VRayExporter();

	/// Create and initilize or reset the V-Ray renderer instance.
	/// This will stop currently running rendering (if any).
	/// @param hasUI[in] - true to enable the VFB, false if VFB is not needed
	///        for example when running Houdini in non-GUI mode
	/// @param reInit[in] - true to re-create the V-Ray renderer instance
	///        otherwise it only resets the existing instance
	/// @returns true on success
	int initRenderer(int hasUI, int reInit);

	/// Prepare for scene export and clear cached data from previous invocations
	/// @param hasUI[in] - if the VFB is enabled
	/// @param nframes[in] - how many frames are we going to render
	/// @param tstart[in] - start export time
	/// @param tend[in] - end export time
	void initExporter(int hasUI, int nframes, fpreal tstart, fpreal tend);

	void fillViewParamsResFromCameraNode(const OBJ_Node &camera, ViewParams &viewParams);

	/// Fills view settings from camera node.
	/// @param camera Camera node.
	/// @param viewParams ViewParams to fill.
	void fillViewParamsFromCameraNode(const OBJ_Node &camera, ViewParams &viewParams);

	/// Sets parameretrs for physical camera.
	/// @param camera Camera node.
	/// @param viewParams ViewParams to fill.
	void fillPhysicalViewParamsFromCameraNode(const OBJ_Node &camera, ViewParams &viewParams);

	/// Fills view settings from ROP node.
	/// @param ropNode V-Ray ROP node.
	/// @param viewParams ViewParams to fill.
	void fillViewParamsFromRopNode(const OP_Node &ropNode, ViewParams &viewParams);

	/// Fill in motion blur settings
	/// @param pluginDesc[out] - motion blur settings plugin description
	void fillMotionBlurParams(MotionBlurParams &mbParams);

	/// Fill in output settings
	/// @param pluginDesc[out] - output settings plugin description
	ReturnValue fillSettingsOutput(Attrs::PluginDesc &pluginDesc);

	/// Export camera related settings - camera, dof, motion blur, etc.
	/// This is called once for each frame we want to render.
	void exportView();

	/// Export view from the specified view parameters.
	/// Used in SOHO IPR.
	/// @param viewParams View parameters
	ReturnValue exportView(const ViewParams &viewParams);

	/// Returns current view parameters.
	const ViewParams &getViewParams() const { return m_viewParams; }

	/// Export the actual scene - geometry, materials, lights, environment,
	/// volumes and render channels. This is called once for each frame we
	/// want to render
	void exportScene();

	/// Export light linker plugin
	/// that cast shadows from the given light plugin
	void exportLightLinker();

	/// Export global renderer settings - color mapping, gi, irradiance cache, etc.
	/// This is called once when a render session is initililzed.
	ReturnValue exportSettings();

	/// Export active lights in the scene
	/// This is called once for each frame we want to render
	void exportLights();

	/// Export render channels
	/// This is called once for each frame we want to render
	/// @param op_node[in] - render channels collector VOP node
	void exportRenderChannels(OP_Node *op_node);

	/// Export environment
	/// This is called once for each frame we want to render
	/// @param op_node[in] - environment VOP node
	void exportEnvironment(OP_Node *op_node);

	/// Export volumetric effects
	/// This is called once for each frame we want to render
	/// @param op_node[in] - environment VOP node
	void exportEffects(OP_Node *op_net);

	void setTime(fpreal time);

	/// Export scene at a given time
	/// This is called once for each frame we want to render
	void exportFrame(fpreal time);

	/// Export cleanup callback. Called when the rendering has finished.
	void exportEnd();

	/// Export OBJ_geometry node
	/// @returns V-Ray plugin created for that node
	VRay::Plugin exportObject(OP_Node *opNode);

	/// Export VRayClipper node
	/// @returns V-Ray plugin created for that node
	VRay::Plugin exportVRayClipper(OBJ_Node &clipperNode);

	/// Tries to set displacement texture from path attribute.
	/// Otherwise texture connected to socket will be used if found.
	int exportDisplacementTexture(OP_Node &opNode, Attrs::PluginDesc &pluginDesc, const QString &parmNamePrefix);

	int exportDisplacementFromSubdivInfo(const SubdivInfo &subdivInfo, Attrs::PluginDesc &pluginDesc);

	/// Export the given geomtry with displacement/subdivision at render time
	/// @param obj_node[in] - the OBJ_Geometry node owner of displacement/subdivision
	///        render properties
	/// @param geomPlugin[in] - geometry to displace/subdivide
	/// @returns V-Ray displacement/subdivision plugin
	VRay::Plugin exportDisplacement(OBJ_Node &obj_node, const VRay::Plugin &geomPlugin, const SubdivInfo &subdivInfo);

	/// Export VOP node
	/// @param opNode VOP node instance.
	/// @return V-Ray plugin.
	VRay::Plugin exportVop(OP_Node *opNode, ExportContext *parentContext=nullptr);

	/// Export Make transform VOP node
	/// @param rotate Rotate the transformation matrix so that Y-axis is up.
	/// @returns V-Ray transform for that node
	VRay::Transform exportTransformVop(VOP_Node &vop_node, ExportContext *parentContext = nullptr, bool rotate = false);

	/// Export V-Ray material from SHOP network or VOP node.
	/// @param node SHOP or VOP node. May be nullptr (the default material will be returned in this case). 
	/// @returns V-Ray plugin.
	VRay::Plugin exportMaterial(OP_Node *node);

	/// Export the default light created when there are no lights in the scene
	/// NOTE: will use the m_viewParams.renderView.tm for tm of the headlight - it must be set (exportView) before callign this method
	/// @param update[in] - flags whether this is called from IPR callback
	/// @returns V-Ray plugin for default light
	VRay::Plugin exportDefaultHeadlight(bool update = false);

	/// Export defaull V-Ray material. This is used when no valid material is found
	/// for the object
	/// @returns V-Ray plugin for default material
	VRay::Plugin exportDefaultMaterial();

	/// Exports node from "op:" or file path.
	/// @param path String value: file or node path.
	VRay::Plugin exportNodeFromPath(const UT_String &path);

	///	Fill RawBitmapBuffer parameters from COP node.
	/// @param copNode COP2 node.
	/// @param pluginDesc Plugin description.
	/// @returns True on success.
	int fillCopNodeBitmapBuffer(COP2_Node &copNode, Attrs::PluginDesc &pluginDesc);

	///	Exports COP node as RawBitmapBuffer.
	/// @param copNode COP2 node.
	VRay::Plugin exportCopNodeBitmapBuffer(COP2_Node &copNode);

	/// Default mapping type.
	enum DefaultMappingType {
		defaultMappingChannel = 0,
		defaultMappingChannelName,
		defaultMappingSpherical,
		defaultMappingTriPlanar,
	};

	/// Fills defualt settings for default mapping.
	static void fillDefaultMappingDesc(DefaultMappingType mappingType, Attrs::PluginDesc &uvwgenDesc);

	/// Exports image texture from "op:" or file path.
	/// @param path String value: file or node path.
	/// @param mappingType Default mapping type.
	VRay::Plugin exportNodeFromPathWithDefaultMapping(const UT_String &path, DefaultMappingType mappingType, BitmapBufferColorSpace colorSpace);

	///	Exports COP node as TexBitmap.
	/// @param copNode COP2 node.
	/// @param mappingType Default mapping type.
	VRay::Plugin exportCopNodeWithDefaultMapping(COP2_Node &copNode, DefaultMappingType mappingType);

	///	Exports file path as RawBitmapBuffer.
	/// @param filePath File path.
	VRay::Plugin exportFileTextureBitmapBuffer(const UT_String &filePath, BitmapBufferColorSpace colorSpace);

	/// Exports file path as TexBitmap with default mapping.
	/// @param filePath File path.
	/// @param mappingType Mapping type.
	/// @param colorSpace Color space.
	VRay::Plugin exportFileTextureWithDefaultMapping(const UT_String &filePath,
	                                                 DefaultMappingType mappingType,
	                                                 BitmapBufferColorSpace colorSpace);

#ifdef CGR_HAS_VRAYSCENE
	VRay::Plugin exportVRayScene(OBJ_Node *obj_node, SOP_Node *geom_node);
#endif

	/// Create or update a plugin from a plugin description
	/// @param pluginDesc - plugin description with relevant properties set
	/// @returns invalid Plugin object if not successul
	virtual VRay::Plugin exportPlugin(const Attrs::PluginDesc &pluginDesc);

	/// Update a plugin properties from a plugin description
	/// @param plugin - the plugin to update
	/// @param pluginDesc - plugin description with relevant properties set
	void exportPluginProperties(VRay::Plugin &plugin, const Attrs::PluginDesc &pluginDesc);

	/// Exports the current scene contents to the specified file. AppSDK
	/// serializes all plugins in text format.
	/// @param filepath - The path to the file where the scene will be exported. The
	/// file path must contain the name and extension of the destination file.
	/// @param settings - Additional options such as compression and file splitting
	/// @returns 0 - no error
	int exportVrscene(const QString &filepath, VRay::VRayExportSettings &settings);

	/// Delete plugins created for the given OBJ node.
	void removePlugin(OBJ_Node *node);

	/// Delete plugin with the given name
	void removePlugin(const QString &pluginName);

	/// Delete plugin.
	/// @param plugin V-Ray plugin instance.
	void removePlugin(VRay::Plugin plugin);

	/// Start rendering at the current time. This will do different this depending on
	/// the work mode of the exporter- export vrscene, render or both
	/// @param locked[in] - when true this will force the current thread to block
	///        until rendering is done. By default this is a non-blocking call
	/// @returns 0 - no error
	int renderFrame(int locked=false);

	/// Start rendering an animation sequence.
	/// @param start[in] - animation start time
	/// @param end[in] - animation end time
	/// @param step[in] - animation time step
	/// @param locked[in] - when true this will force the current thread to block
	///        until rendering is done. By default this is a non-blocking call
	/// @returns 0 - no error
	int renderSequence(int start, int end, int step, int locked=false);

	void clearKeyFrames(double toTime);

	/// Set if we are exporting animation
	/// @note also used for motion blur
	void setAnimation(bool on);

	/// Sets sesstion type.
	/// @param value Session type.
	void setSessionType(VfhSessionType value);

	/// Returns sesstion type.
	VfhSessionType getSessionType() const { return sessionType; }

	/// Adjust DR options and hosts based on what is set in the ROP parameters
	void setDRSettings();

	/// Set the render mode: Production/RT CPU/RT GPU
	void setRenderMode(VRay::RendererOptions::RenderMode mode);

	/// Sets export mode: export vrscene/render/both
	void setExportMode(ExpWorkMode mode);

	/// Set current export context
	void setContext(const VRayOpContext &ctx);

	/// Abort rendering at first possible time
	void setAbort();

	/// Set image width and height
	void setRenderSize(int w, int h);

	/// Get current export context
	VRayOpContext &getContext() { return m_context; }

	/// Get current export context
	const VRayOpContext &getContext() const { return m_context; }

	/// Get vfh plugin renderer
	VRayPluginRenderer& getRenderer() { return m_renderer; }

	/// Set the V-Rap ROP bound to this exporter
	void setRopPtr(OP_Node *value) { m_rop = value; }

	/// Get pointer to the bound V-Ray ROP
	/// NOTE: CHECK THE RETURN VALUE OF THIS - NULL IS VALID!
	OP_Node* getRopPtr() { return m_rop; }

	/// Get pointer to the bound V-Ray ROP
	const OP_Node * getRopPtr() const { return m_rop; }

	/// Get ROP error code. This is called from the V-Ray ROP on every frame
	/// to check if rendering should be abo=rted
	ROP_RENDER_CODE getError() const { return m_error; }

	/// Test if we are using the GPU engine
	int isGPU() const { return m_isGPU; }

	/// Test if we are rendering in IPR mode
	bool isInteractive() const { return sessionType == VfhSessionType::ipr || sessionType == VfhSessionType::rt; }

	/// Test if we need to abort the rendering
	int isAborted() const { return m_isAborted; }

	/// Test if we are going to export data for more that single frame
	/// i.e. animation, motion blur or output velocity channel
	int isAnimation() const { return m_isAnimation; }

	/// Test if we are going to export data for more that single frame
	/// i.e. animation, motion blur or output velocity channel
	int needVelocity() const { return m_isMotionBlur || m_isVelocityOn; }

	/// Test if we are using stereo camera. This is setup on the V-Ray ROP
	int isStereoView() const;

	/// Test if we are using physical camera
	/// @param camera[in] - camera object to read parameters from
	PhysicalCameraMode usePhysicalCamera(const OBJ_Node &camera) const;

	/// Test if velocity channels is enabled
	/// @param rop[in] - the V-Ray ROP to test for velocity channel enabled
	int hasVelocityOn(OP_Node &rop) const;

	/// Test if motion blur is enabled
	/// @param rop[in] - the V-Ray ROP
	/// @param camera[in] - active render camera
	int hasMotionBlur(OP_Node &rop, OBJ_Node &camera) const;

	/// Show VFB if renderer is started and VFB is enabled
	void showVFB();

	/// Reset exporter / renderer.
	void reset();

	/// Clear exporter plugin caches.
	void clearCaches();

	/// Helper functions to retrieve the input node given an input connection name
	/// @param op_node[in] - VOP node
	/// @param inputName[in] -  the input connection name
	/// @returns the VOP input
	static OP_Input* getConnectedInput(OP_Node *op_node, const QString &inputName);
	static OP_Node* getConnectedNode(OP_Node *op_node, const QString &inputName);

	/// Helper function to retrieve the connection type given an input connection name
	/// @param op_node[in] - VOP node
	/// @param inputName[in] -  the input connection name
	/// @returns the connection type
	static const Parm::SocketDesc* getConnectedOutputType(OP_Node *op_node, const QString &inputName);

	/// Helper functions to generate a plugin name for a given node
	/// @param opNode Node instance.
	/// @param prefix Optional prefix.
	/// @param suffix Optional suffix.
	/// @returns Plugin name.
	static QString getPluginName(const OP_Node &opNode, const QString &prefix=SL(""), const QString &suffix=SL(""));

	static QString getPluginName(OBJ_Node &objNode);

	static VRay::VUtils::CharStringRefList getSceneName(const OP_Node &opNode, int primID=-1);
	static VRay::VUtils::CharStringRefList getSceneName(const tchar *name);

	/// Helper function to get the active camera from a given ROP node
	/// @param rop[in] - the ROP node
	/// @returns the active camera
	static OBJ_Node* getCamera(const OP_Node *rop);

	/// Helper function to get material for an OBJ_Geometry node
	/// @param obj[in] - the OBJ node
	/// @param t[in] - evaluation time for the paremeter
	/// @returns the SHOP node
	static OP_Node *getObjMaterial(OBJ_Node *objNode, fpreal t=0.0);

	/// Helper function to get OBJ node transform as VRay::Transform
	/// @param obj[in] - the OBJ node
	/// @param context[in] - evaluation time for the paremeters
	/// @param flip[in] - whether to flip y and z axis
	static VRay::Transform getObjTransform(OBJ_Node *obj_node, OP_Context &context, bool flip=false);

	/// Helper function to get OBJ node transform as 2d array of floats
	/// @param obj[in] - the OBJ node
	/// @param context[in] - evaluation time for the paremeters
	/// @param tm[out] - output trasform
	static void getObjTransform(OBJ_Node *obj_node, OP_Context &context, float tm[4][4]);

	/// Convert from VUtils transform to Houdini one
	static void TransformToMatrix4(const VUtils::TraceTransform &tm, UT_Matrix4 &m);

	/// Convert from Houdini transform to VRay::Transform
	/// @param m4[in] - Houdini trasform
	/// @param flip[in] - whether to flip y and z axis
	static VRay::Transform Matrix4ToTransform(const UT_Matrix4D &m4, bool flip=false);

	/// Obtain the first child node with given operator type
	/// @param op_node[in] - parent node
	/// @param op_type[in] - operator name
	static OP_Node* FindChildNodeByType(OP_Node *op_node, const QString &op_type);

	/// Helper function to fill in plugin description attributes from UT_options
	/// @param pluginDesc[out] - the plugin description
	/// @param options[in] - UT_Options map that holds attribute values
	/// @returns true on success
	bool setAttrsFromUTOptions(Attrs::PluginDesc &pluginDesc, const UT_Options &options) const;

	/// Helper function to fill in single plugin description attribute from a node parameter
	/// @param pluginDesc[out] - the plugin description
	/// @param parmDesc[in] - plugin attribute description
	/// @param opNode[in] - the node
	/// @param parmName[in] - parameter name
	void setAttrValueFromOpNodePrm(Attrs::PluginDesc &plugin, const Parm::AttrDesc &parmDesc, OP_Node &opNode, const QString &parmName) const;

	/// Helper function to fill in plugin description attributes from matching node parameters
	/// @param pluginDesc[out] - the plugin description
	/// @param opNode[in] - the node
	/// @param prefix[in] - common prefix for the name of related parameters
	/// @param remapInterp[in] - whether to remap ramp interpotaion type (used for ramp parameters)
	void setAttrsFromOpNodePrms(Attrs::PluginDesc &plugin, OP_Node *opNode, const QString &prefix="", bool remapInterp=false);

	/// Converts different socket types
	/// @param[out] conPluginInfo - generated connection plugin info
	/// @param[in] curSockInfo - the socket 
	/// @param[in] fromSocketInfo - type of the connection
	/// @param[in] pluginDesc - the plugin description
	void autoconvertSocket(ConnectedPluginInfo &conPluginInfo, const Parm::SocketDesc &curSockInfo, const Parm::SocketDesc &fromSocketInfo, Attrs::PluginDesc &pluginDesc);

	/// Converts the input plugin to plugin of socket type if needed
	/// @param[out] inputPlugin - the plugin connected to the socket
	/// @param[in] pluginDesc - description of the current exported plugin
	/// @param[in] node - current node
	/// @param[in] socketType - socket type
	/// @param[in] socketName - socket name
	void convertInputPlugin(VRay::Plugin& inputPlugin, Attrs::PluginDesc &pluginDesc, OP_Node* node, VOP_Type socketType, const QString &socketName);

	/// Helper function to fill in plugin description attributes from VOP node connected inputs
	/// @param pluginDesc[out] - the plugin description
	/// @param vopNode[in] - the VOP node
	/// @param parentContext - not used
	void setAttrsFromOpNodeConnectedInputs(Attrs::PluginDesc &pluginDesc, VOP_Node *vopNode, ExportContext *parentContext=nullptr);

	/// Export connected input for a VOP node
	/// @param pluginDesc[out] - the plugin description
	/// @param vopNode[in] - the VOP node
	/// @param inpidx[in] - input index
	/// @param inputName[in] - input connection name
	/// @param parentContext - not used
	/// @returns VRay::Plugin for the input VOP or invalid plugin on error
	VRay::Plugin exportConnectedVop(VOP_Node *vop_node, int inpidx, ExportContext *parentContext = nullptr);
	VRay::Plugin exportConnectedVop(VOP_Node *vop_node, const UT_String &inputName, ExportContext *parentContext = nullptr);

	VRay::Plugin exportPrincipledShader(OP_Node &opNode, ExportContext *parentContext=nullptr);

	void fillNodeTexSky(const OP_Node &opNode, Attrs::PluginDesc &pluginDesc);

	/// Export input parameter VOPs for a given VOP node as V-Ray user textures.
	/// Default values for the textures will be queried from the corresponding
	/// SHOP parameter. Exported user textures are added as attibutes to the plugin
	/// description.
	/// @param pluginDesc[out] - VOP plugin description
	/// @param vopNode[in] - the VOP node
	void setAttrsFromSHOPOverrides(Attrs::PluginDesc &pluginDesc, VOP_Node &vopNode);

	/// Returns object exporter.
	ObjectExporter& getObjectExporter() { return objectExporter; }

	/// Applies which take to use for scene export.
	/// @param take Take name. If not specified ROP's take will be used (if ROP is a VRayRendererNode instance).
	void applyTake(const char *take=nullptr);

	/// Restores "system" take.
	void restoreCurrentTake();

	/// Saves VFB state.
	void saveVfbState();

	/// Restores VFB state.
	void restoreVfbState();

	/// Executed when user presses "Render" button in the VFB.
	void renderLast() const;

	const GSTY_BundleMap &getBundleMap() const { return bundleMap.getBundleMap(); }

	/// Flag indicating that we are exporting scene.
	/// Some non-cont OP_Node methods could trigger event handlers.
	QAtomicInt inSceneExport = false;

	/// Get plugin cache.
	OpCacheMan &getCacheMan() { return cacheMan; }

private:
	/// Export V-Ray material from VOP node.
	/// @param node VOP node.
	/// @returns V-Ray plugin.
	VRay::Plugin exportMaterial(VOP_Node *node);

	/// The driver node bound to this exporter.
	OP_Node *m_rop;

	VRayPluginRenderer             m_renderer; ///< the plugin renderer
	VRayOpContext                  m_context; ///< current export context
	int                            m_renderMode; ///< rend
	QAtomicInt                     m_isAborted; ///< flag whether rendering should be aborted when possible
	ViewParams                     m_viewParams; ///< used to gather view data from the ROP and camera
	int                            m_frames; ///< how many frames are we going to export
	ROP_RENDER_CODE                m_error; ///< ROP error to singnal the ROP rendering should be aborted
	ExpWorkMode                    m_workMode; ///< what should the exporter do- export vrscene, render or both
	CbItems                        m_opRegCallbacks; ///< holds registered node callbacks for live IPR updates

	/// Rendering session type.
	VfhSessionType sessionType;

	int                            m_isGPU; ///< if we are using RT GPU rendering engine
	int                            m_isAnimation; ///< if we should export the scene at more than one time
	int                            m_isMotionBlur; ///< if motion blur is turned on
	int                            m_isVelocityOn; ///< if we have velocity channel enabled
	fpreal                         m_timeStart; ///< start time for the export
	fpreal                         m_timeEnd; ///< end time for the export
	FloatSet                       m_exportedFrames; ///< set of time points at which the scene has already been exported

	struct AnimInfo {
		/// Start frame.
		fpreal frameStart = 0;

		/// End frame.
		fpreal frameEnd = 0;

		/// Frame increment step.
		int frameStep = 1;
	};

	/// Animation settings.
	AnimInfo animInfo;

	/// Object exporter.
	ObjectExporter objectExporter;

	/// Frame buffer settings.
	VFBSettings vfbSettings;

	/// Scene take selected in UI. Used to restore selected take after export.
	TAKE_Take *currentTake{nullptr};

	/// Export each frame into a separate file.
	int exportFilePerFrame{0};

	struct VfhBundleMap {
		~VfhBundleMap();

		/// Initialize bundle OP names from OP_BundleList.
		void init();

		/// Frees allocated memory for names.
		void freeMem();

		/// Returns preprocessed GSTY_BundleMap.
		const GSTY_BundleMap &getBundleMap() const { return bundleMap; }

	private:
		struct MyBundle {
			/// Frees allocated memory for names.
			void freeMem();

			/// Bundle name.
			UT_StringHolder name;

			/// Number of OPs in a bundle.
			int opNamesCount = 0;

			/// OP names array.
			char* *opNames = nullptr;
		};

		/// Preprocessed bundles map.
		GSTY_BundleMap bundleMap;

		/// Bundles data.
		QList<MyBundle> bundles;
	} bundleMap;

	/// Plugins cache.
	OpCacheMan cacheMan;

public:
	/// Register event callback for a given node. This callback will be invoked when
	/// change on the node occurs. The event type will be passed as parameter to the callback
	/// @param op_node - the node to track
	/// @param cb - the event callback function
	void addOpCallback(OP_Node *op_node, OP_EventMethod cb);

	/// Unregister a callback for the given node. This callback will no longer be invoked when
	/// change on the node occurs.
	/// @param op_node - the node to track
	/// @param cb - the event callback function
	void delOpCallback(OP_Node *op_node, OP_EventMethod cb);

	/// Unregister all callbacks for the given node.
	/// @param op_node - the node to track
	void delOpCallbacks(OP_Node *op_node);

	/// Unregister all callbacks for all nodes.
	/// @note this will only remove callbacks that have been registered
	///       through the callback interface
	void resetOpCallbacks();

	/// Callbacks for tracking changes on different types of nodes
	static void RtCallbackLight(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void RtCallbackOBJGeometry(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void RtCallbackSOPChanged(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void RtCallbackVop(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void RtCallbackSurfaceShop(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void RtCallbackDisplacementShop(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void RtCallbackDisplacementVop(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void RtCallbackDisplacementObj(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void RtCallbackVRayClipper(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void RtCallbackOPDirector(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void rtCallbackObjNetwork(OP_Node *caller, void *callee, OP_EventType type, void *data);

	/// A lock for callbacks.
	static VUtils::FastCriticalSection csect;
};

const char *getVRayPluginIDName(VRayPluginID pluginID);

// XXX: Rename or remove.
int getFrameBufferType(OP_Node &rop);

/// Returns render mode/device for the production rendering from the ROP node.
VRay::RendererOptions::RenderMode getRendererMode(const OP_Node &rop);

/// Returns render mode/device for the interactive rendering from the ROP node.
VRay::RendererOptions::RenderMode getRendererIprMode(const OP_Node &rop);

/// Returns export mode from the ROP node.
VRayExporter::ExpWorkMode getExportMode(const OP_Node &rop);

int isBackground();

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORTER_H
