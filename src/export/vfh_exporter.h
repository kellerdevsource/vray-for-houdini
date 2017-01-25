//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORTER_H
#define VRAY_FOR_HOUDINI_EXPORTER_H

#include "vfh_defines.h"
#include "vfh_typedefs.h"
#include "vfh_vray.h"
#include "vfh_plugin_exporter.h"
#include "vfh_plugin_info.h"
#include "vfh_export_view.h"
#include "vfh_vfb.h"
#include "vfh_log.h"


#include "vfh_export_context.h"

#include <OP/OP_Node.h>
#include <OBJ/OBJ_Node.h>
#include <ROP/ROP_Node.h>

#include <unordered_map>


namespace VRayForHoudini {

class VRayRendererNode;

enum VRayLightType {
	VRayLightOmni      = 0,
	VRayLightRectangle = 2,
	VRayLightSphere    = 4,
	VRayLightDome      = 6,
#if UT_MAJOR_VERSION_INT < 15
	VRayLightSun       = 7,
#else
	VRayLightSun       = 8,
#endif
};


struct OpInterestItem {
	OpInterestItem():
		op_node(nullptr),
		cb(nullptr),
		cb_data(nullptr)
	{}

	OpInterestItem(OP_Node *op_node, OP_EventMethod cb, void *cb_data):
		op_node(op_node),
		cb(cb),
		cb_data(cb_data)
	{}

	OP_Node        *op_node;
	OP_EventMethod  cb;
	void           *cb_data;
};
typedef std::vector<OpInterestItem> CbItems;


struct MotionBlurParams {
	MotionBlurParams()
		: mb_geom_samples(1)
		, mb_duration(0.)
		, mb_interval_center(0.)
	{}

	void   calcParams(fpreal currFrame);

	int    mb_geom_samples;
	/// (fpreal) motion blur duration in frames
	fpreal mb_duration;
	/// (fpreal) motion blur interval center in frames
	fpreal mb_interval_center;

	fpreal mb_start;
	fpreal mb_end;
	fpreal mb_frame_inc;
};

/// Vfh main exporter. This is the main class responsible for translating
/// Houdini geometry and nodes to V-Ray plugins, intilizing and starting
/// the rendering process
class VRayExporter
{
public:
	/// Available work modes for the exporter
	// NOTE: Keep in sync with "render_export_mode"
	enum ExpWorkMode {
		ExpRender = 0, ///< only render
		ExpExportRender, ///< export a vrscene and render
		ExpExport, ///< export a vrscene
	};

	VRayExporter(VRayRendererNode *rop);
	~VRayExporter();

public:
	/// Create and initilize or reset the V-Ray renderer instance.
	/// This will stop currently running rendering (if any).
	/// @param hasUI[in] - true to enable the VFB, false if VFB is not needed
	///        for example when running Houdini in non-GUI mode
	/// @param reInit[in] - true to re-create the V-Ray renderer instance
	///        otherwise it only resets the existing instance
	/// @retval true on success
	int initRenderer(int hasUI, int reInit);

	/// Prepare for scene export and clear cached data from previous invocations
	/// @param hasUI[in] - if the VFB is enabled
	/// @param nframes[in] - how many frames are we going to render
	/// @param tstart[in] - start export time
	/// @param tend[in] - end export time
	void initExporter(int hasUI, int nframes, fpreal tstart, fpreal tend);

	/// Gather data for camera settings
	/// @param camera[in] - the active camera
	/// @param rop[in] - the rop node invoking the rendering
	/// @param viewParams[out] - collects camera settings
	void fillCameraData(const OBJ_Node &camera, const OP_Node &rop, ViewParams &viewParams);

	/// Gather data for motion blur
	/// @param viewParams[out] - collects motion blur settings
	void fillSettingsMotionBlur(ViewParams &viewParams);

	/// Fill in physical camera settings
	/// @param viewParams[in] - holds data for camera settings
	/// @param pluginDesc[out] - physical camera plugin description
	void fillPhysicalCamera(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc);

	/// Fill in depth of field settings
	/// @param viewParams[in] - holds data for camera settings
	/// @param pluginDesc[out] - depth of field plugin description
	void fillSettingsCameraDof(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc);

	/// Fill in default camera settings
	/// @param viewParams[in] - holds data for camera settings
	/// @param pluginDesc[out] - default camera plugin description
	void fillCameraDefault(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc);

	/// Fill in camera settings
	/// @param viewParams[in] - holds data for camera settings
	/// @param pluginDesc[out] - camera settings plugin description
	void fillSettingsCamera(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc);

	/// Fill in render view settings
	/// @param viewParams[in] - holds data for camera settings
	/// @param pluginDesc[out] - render view settings plugin description
	void fillRenderView(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc);

	/// Fill in stereoscopic settings
	/// @param viewParams[in] - holds data for camera settings
	/// @param pluginDesc[out] - stereoscopic settings plugin description
	void fillStereoSettings(const ViewParams &viewParams, Attrs::PluginDesc &pluginDesc);

	/// Fill in motion blur settings
	/// @param pluginDesc[out] - motion blur settings plugin description
	void fillMotionBlurParams(MotionBlurParams &mbParams);

	/// Fill in output settings
	/// @param pluginDesc[out] - output settings plugin description
	void fillSettingsOutput(Attrs::PluginDesc &pluginDesc);

	/// Export camera related settings - camera, dof, motion blur, etc.
	/// This is called once for each frame we want to render
	/// @retval 0 on success
	int exportView();

	/// Export the actual scene - geometry, materials, lights, environment,
	/// volumes and render channels. This is called once for each frame we
	/// want to render
	void exportScene();

	/// Export global renderer settings - color mapping, gi, irradiance cache, etc.
	/// This is called once when a render session is initililzed.
	void exportSettings();

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

	/// Export scene at a given time
	/// This is called once for each frame we want to render
	void exportFrame(fpreal time);

	/// Export cleanup callback. Called when the rendering has finished.
	void exportEnd();

	/// Export OBJ_geometry node
	/// @retval V-Ray plugin created for that node
	VRay::Plugin exportObject(OBJ_Node *obj_node);

	/// Export VRayClipper node
	/// @retval V-Ray plugin created for that node
	VRay::Plugin exportVRayClipper(OBJ_Node &clipperNode);

	VRay::Plugin exportParticles(OBJ_Node *dop_network);

	/// Fill in displacement/subdivision render properties for the given node
	/// @param obj_node[in] - the OBJ_Geometry node
	/// @param pluginDesc[out] - diplacement/subdivision plugin description
	void exportDisplacementDesc(OBJ_Node *obj_node, Attrs::PluginDesc &pluginDesc);

	/// Export the given geomtry with displacement/subdivision at render time
	/// @param obj_node[in] - the OBJ_Geometry node owner of displacement/subdivision
	///        render properties
	/// @param geomPlugin[in] - geometry to displace/subdivide
	/// @retval V-Ray displacement/subdivision plugin
	VRay::Plugin exportDisplacement(OBJ_Node *obj_node, VRay::Plugin &geomPlugin);

	/// Export OBJ_Light node
	/// @retval V-Ray plugin created for that node
	VRay::Plugin exportLight(OBJ_Node *obj_node);

	/// Export VOP node
	/// @retval V-Ray plugin created for that node
	VRay::Plugin exportVop(OP_Node *op_node, ExportContext *parentContext = nullptr);

	/// Export Make transform VOP node
	/// @retval V-Ray transform for that node
	VRay::Transform exportTransformVop(VOP_Node &vop_node, ExportContext *parentContext = nullptr);

	/// Export V-Ray material SHOP network
	/// @retval V-Ray surface material plugin for that node
	VRay::Plugin exportMaterial(SHOP_Node &shop_node);

	/// Export the default light created when there are no lights in the scene
	/// @param update[in] - flags whether this is called from IPR callback
	/// @retval V-Ray plugin for default light
	VRay::Plugin exportDefaultHeadlight(bool update = false);

	/// Export defaull V-Ray material. This is used when no valid material is found
	/// for the object
	/// @retval V-Ray plugin for default material
	VRay::Plugin exportDefaultMaterial();


#ifdef CGR_HAS_VRAYSCENE
	VRay::Plugin                   exportVRayScene(OBJ_Node *obj_node, SOP_Node *geom_node);
#endif

	VRay::Plugin                   exportPlugin(const Attrs::PluginDesc &pluginDesc);
	void                           exportPluginProperties(VRay::Plugin &plugin, const Attrs::PluginDesc &pluginDesc);
	int                            exportVrscene(const std::string &filepath, VRay::VRayExportSettings &settings);

	void                           removePlugin(OBJ_Node *node);
	void                           removePlugin(const std::string &pluginName);
	void                           removePlugin(const Attrs::PluginDesc &pluginDesc);

	int                            renderFrame(int locked=false);
	int                            renderSequence(int start, int end, int step, int locked=false);

	void                           clearKeyFrames(float toTime);

	void                           setAnimation(bool on);
	void                           setCurrentTime(fpreal time);
	void                           setIPR(int isIPR);
	void                           setDRSettings();
	void                           setRendererMode(int mode);
	void                           setWorkMode(ExpWorkMode mode);
	void                           setContext(const OP_Context &ctx);
	void                           setAbort();
	void                           setRenderSize(int w, int h);
	void                           setSettingsRtEngine();

	OP_Context                    &getContext() { return m_context; }
	VRayPluginRenderer            &getRenderer() { return m_renderer; }
	VRayRendererNode              &getRop() { return *m_rop; }
	ROP_RENDER_CODE                getError() const { return m_error; }

	int                            isGPU() const { return m_isGPU; }
	int                            isIPR() const { return m_isIPR; }
	int                            isAborted() const { return m_isAborted; }
	int                            isAnimation() const { return m_isAnimation; }
	int                            isStereoView() const;
	int                            isPhysicalView(const OBJ_Node &camera) const;
	int                            isNodeAnimated(OP_Node *op_node);
	int                            isLightEnabled(OP_Node *op_node);

	int                            hasVelocityOn(OP_Node &rop) const;
	int                            hasMotionBlur(OP_Node &rop, OBJ_Node &camera) const;

	static OP_Input               *getConnectedInput(OP_Node *op_node, const std::string &inputName);
	static OP_Node                *getConnectedNode(OP_Node *op_node, const std::string &inputName);
	static const Parm::SocketDesc *getConnectedOutputType(OP_Node *op_node, const std::string &inputName);

	static std::string             getPluginName(OP_Node *op_node, const std::string &prefix="", const std::string &suffix="");
	static std::string             getPluginName(OBJ_Node *obj_node);

	static OBJ_Node               *getCamera(const OP_Node *rop);
	SHOP_Node                     *getObjMaterial(OBJ_Node *obj, fpreal t=0.0);
	static VRay::Transform         getObjTransform(OBJ_Node *obj_node, OP_Context &context, bool flip=false);
	static void                    getObjTransform(OBJ_Node *obj_node, OP_Context &context, float tm[4][4]);

	static void                    TransformToMatrix4(const VUtils::TraceTransform &tm, UT_Matrix4 &m);
	static VRay::Transform         Matrix4ToTransform(const UT_Matrix4D &m4, bool flip=false);
	static OP_Node                *FindChildNodeByType(OP_Node *op_node, const std::string &op_type);

	bool                           setAttrsFromUTOptions(Attrs::PluginDesc &pluginDesc, const UT_Options &options) const;
	void                           setAttrValueFromOpNodePrm(Attrs::PluginDesc &plugin, const Parm::AttrDesc &parmDesc, OP_Node &opNode, const std::string &parmName) const;
	void                           setAttrsFromOpNodePrms(Attrs::PluginDesc &plugin, OP_Node *opNode, const std::string &prefix="", bool remapInterp=false);
	void                           setAttrsFromOpNodeConnectedInputs(Attrs::PluginDesc &pluginDesc, VOP_Node *vopNode, ExportContext *parentContext=nullptr);

	VRay::Plugin                   exportConnectedVop(VOP_Node *vop_node, int inpidx, ExportContext *parentContext = nullptr);
	VRay::Plugin                   exportConnectedVop(VOP_Node *vop_node, const UT_String &inputName, ExportContext *parentContext = nullptr);
	void                           phxAddSimumation(VRay::Plugin sim);

	void                           setAttrsFromSHOPOverrides(Attrs::PluginDesc &pluginDesc, VOP_Node &vopNode);

private:
	VRayRendererNode              *m_rop;
	UI::VFB                        m_vfb;
	VRayPluginRenderer             m_renderer;
	OP_Context                     m_context;
	int                            m_renderMode;
	int                            m_isAborted;
	ViewParams                     m_viewParams;
	int                            m_frames;
	ROP_RENDER_CODE                m_error;
	ExpWorkMode                    m_workMode;
	CbItems                        m_opRegCallbacks;
	VRay::ValueList                m_phxSimulations;
	int                            m_isIPR;
	int                            m_isGPU;
	int                            m_isAnimation;
	int                            m_isMotionBlur;
	int                            m_isVelocityOn;
	fpreal                         m_timeStart;
	fpreal                         m_timeEnd;
	FloatSet                       m_exportedFrames;

public:

	void                           addOpCallback(OP_Node *op_node, OP_EventMethod cb);
	void                           delOpCallback(OP_Node *op_node, OP_EventMethod cb);
	void                           delOpCallbacks(OP_Node *op_node);
	void                           resetOpCallbacks();

	void                           onDumpMessage(VRay::VRayRenderer &renderer, const char *msg, int level);
	void                           onProgress(VRay::VRayRenderer &renderer, const char *msg, int elementNumber, int elementsCount);
	void                           onAbort(VRay::VRayRenderer &renderer);

	static void                    RtCallbackObjManager(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void                    RtCallbackLight(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void                    RtCallbackOBJGeometry(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void                    RtCallbackSOPChanged(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void                    RtCallbackView(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void                    RtCallbackVop(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void                    RtCallbackSurfaceShop(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void                    RtCallbackDisplacementShop(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void                    RtCallbackDisplacementVop(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void                    RtCallbackDisplacementObj(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static void                    RtCallbackVRayClipper(OP_Node *caller, void *callee, OP_EventType type, void *data);

};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORTER_H
