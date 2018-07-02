//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <QDir>
#include <QRegExp>
#include <QRegularExpression>

#include "vfh_defines.h"
#include "vfh_exporter.h"
#include "vfh_prm_templates.h"
#include "vfh_tex_utils.h"
#include "vfh_hou_utils.h"
#include "vfh_attr_utils.h"
#include "vfh_log.h"

#include "obj/obj_node_base.h"
#include "vop/vop_node_base.h"
#include "vop/material/vop_PhoenixSim.h"
#include "sop/sop_vrayscene.h"
#include "rop/vfh_rop.h"

#include <OP/OP_Options.h>
#include <OP/OP_Node.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_Take.h>
#include <ROP/ROP_Node.h>
#include <SHOP/SHOP_Node.h>
#include <SOP/SOP_Node.h>
#include <VOP/VOP_Node.h>

#include <PRM/PRM_ParmOwner.h>

#include <OBJ/OBJ_Geometry.h>
#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_SubNet.h>
#include <OP/OP_Director.h>
#include <OP/OP_BundleList.h>

#include "vfh_export_geom.h"
#include "vfh_op_utils.h"
#include "vfh_vray_cloud.h"

using namespace VRayForHoudini;

static const float RAD_TO_DEG = M_PI / 180.f;

/// Directory hierarchy creator.
/// Using static variable, because QDir::mkpath is not static.
static QDir directoryCreator;

static StringSet RenderSettingsPlugins;

/// Matches frame number like "$F" and "$F4".
static QRegExp frameMatch("\\$F(\\d+)?");
static QRegularExpression frameMatchExpr(frameMatch.pattern());

/// Frame match group indexes.
enum FrameNumberMatchedItem {
	frameNumberMatchedItemVariable = 0,
	frameNumberMatchedItemFramePadding = 1,
};

static VRayDisplacementType getDisplacementType(OP_Node &opNode, const QString &parmTypeName)
{
	UT_String dispTypeMenu;
	opNode.evalString(dispTypeMenu, qPrintable(parmTypeName), 0, 0.0);

	vassert(dispTypeMenu.isInteger());

	return static_cast<VRayDisplacementType>(dispTypeMenu.toInt());
}

static void setGeomDisplacedMeshType(Attrs::PluginDesc &pluginDesc, VRayDisplacementType displaceType)
{
	if (displaceType == displ_type_2d) {
		pluginDesc.add(SL("displace_2d"), true);
		pluginDesc.setIngore(SL("vector_displacement"));
	}
	else {
		if (displaceType == displ_type_vector) {
			pluginDesc.add(SL("vector_displacement"), 1);
		}
		else if (displaceType == displ_type_vector_signed) {
			pluginDesc.add(SL("vector_displacement"), 2);
		}
		else if (displaceType == displ_type_vector_object) {
			pluginDesc.add(SL("vector_displacement"), 3);
		}
		pluginDesc.setIngore(SL("displace_2d"));
	}
}

/// Checks if we're exporting frames into separate *.vrscene files.
static int isExportFramesToSeparateFiles(OP_Node &rop)
{
	UT_String exportFilepath;
	rop.evalStringRaw(exportFilepath, "render_export_filepath", 0, 0.0);

	return frameMatch.indexIn(exportFilepath.buffer()) != -1;
}

/// Fills SettingsRTEngine settings from the ROP node.
/// @param self SettingsRTEngine instance.
/// @param ropNode ROP node.
/// @param isStereoView Stereo settings flag.
static void setSettingsRTEngineFromRopNode(SettingsRTEngine &self, const OP_Node &ropNode, int isStereoView = false)
{
	self.coherent_tracing = Parm::getParmInt(ropNode, "SettingsRTEngine_coherent_tracing");
	self.cpu_bundle_size = Parm::getParmInt(ropNode, "SettingsRTEngine_cpu_bundle_size");
	self.cpu_samples_per_pixel = Parm::getParmInt(ropNode, "SettingsRTEngine_cpu_samples_per_pixel");
	self.disable_render_elements = Parm::getParmInt(ropNode, "SettingsRTEngine_disable_render_elements");
	self.enable_cpu_interop = Parm::getParmInt(ropNode, "SettingsRTEngine_enable_cpu_interop");
	self.enable_mask = Parm::getParmInt(ropNode, "SettingsRTEngine_enable_mask");
	self.gi_depth = Parm::getParmInt(ropNode, "SettingsRTEngine_gi_depth");
	self.gpu_bundle_size = Parm::getParmInt(ropNode, "SettingsRTEngine_gpu_bundle_size");
	self.gpu_samples_per_pixel = Parm::getParmInt(ropNode, "SettingsRTEngine_gpu_samples_per_pixel");
	self.low_gpu_thread_priority = Parm::getParmInt(ropNode, "SettingsRTEngine_low_gpu_thread_priority");
	self.max_draw_interval = Parm::getParmInt(ropNode, "SettingsRTEngine_max_draw_interval");
	self.max_render_time = Parm::getParmFloat(ropNode, "SettingsRTEngine_max_render_time");
	self.max_sample_level = Parm::getParmInt(ropNode, "SettingsRTEngine_max_sample_level");
	self.min_draw_interval = Parm::getParmInt(ropNode, "SettingsRTEngine_min_draw_interval");
	self.noise_threshold = Parm::getParmFloat(ropNode, "SettingsRTEngine_noise_threshold");
	self.opencl_resizeTextures = Parm::getParmInt(ropNode, "SettingsRTEngine_opencl_resizeTextures");
	self.opencl_texsize = Parm::getParmInt(ropNode, "SettingsRTEngine_opencl_texsize");
	self.opencl_textureFormat = Parm::getParmInt(ropNode, "SettingsRTEngine_opencl_textureFormat");
	self.progressive_samples_per_pixel = Parm::getParmInt(ropNode, "SettingsRTEngine_progressive_samples_per_pixel");
	self.stereo_eye_distance = isStereoView ? Parm::getParmFloat(ropNode, "VRayStereoscopicSettings_eye_distance") : 0;
	self.stereo_focus = isStereoView ? Parm::getParmInt(ropNode, "VRayStereoscopicSettings_focus_method") : 0;
	self.stereo_mode = isStereoView ? Parm::getParmInt(ropNode, "VRayStereoscopicSettings_use") : 0;
	self.trace_depth = Parm::getParmInt(ropNode, "SettingsRTEngine_trace_depth");
	self.undersampling = Parm::getParmInt(ropNode, "SettingsRTEngine_undersampling");
}

/// Sets optimized settings for GPU.
/// @param self SettingsRTEngine instance.
/// @param ropNode ROP node.
/// @param mode Render mode.
static void setSettingsRTEnginetOptimizedGpuSettings(SettingsRTEngine &self, const OP_Node &ropNode, VRay::VRayRenderer::RenderMode mode)
{
	if (!Parm::getParmInt(ropNode, "SettingsRTEngine_auto"))
		return;

	// CPU/GPU RT/IPR.
	if (mode >= VRay::VRayRenderer::RENDER_MODE_INTERACTIVE &&
        mode <= VRay::VRayRenderer::RENDER_MODE_INTERACTIVE_CUDA)
    {
		self.cpu_samples_per_pixel = 1;
		self.cpu_bundle_size = 64;
		self.gpu_samples_per_pixel = 1;
		self.gpu_bundle_size = 128;
		self.undersampling = 0;
		self.progressive_samples_per_pixel = 0;
    }
	// GPU Production.
	else if (mode >= VRay::VRayRenderer::RENDER_MODE_PRODUCTION_OPENCL &&
			 mode <= VRay::VRayRenderer::RENDER_MODE_PRODUCTION_CUDA)
	{
		self.gpu_samples_per_pixel = 16;
		self.gpu_bundle_size = 256;
		self.undersampling = 0;
		self.progressive_samples_per_pixel = 0;
	}
}

/// Make sure path is relative (strip leading slashes) added by ChannelManager's expand path
static void makePathRelative(UT_String &path)
{
	if (!path.isstring() || !(path[0] == '\\' || path[0] == '/')) {
		return;
	}

	UT_String result(UT_String::ALWAYS_DEEP);
	const int lenBefore = path.length();
	const int newLen = path.substr(result, 1, INT_MAX);
	path = result;
	vassert(newLen == lenBefore - 1 && "makePathRelative failed to strip leading slash");
}

struct QAtomicIntRaii
{
	QAtomicIntRaii(QAtomicInt &value)
		: value(value)
	{
		value = true;
	}

	~QAtomicIntRaii()
	{
		value = false;
	}

private:
	QAtomicInt &value;
};

void VRayExporter::reset()
{
	clearCaches();

	resetOpCallbacks();
	restoreCurrentTake();

	m_renderer.reset();
}

void VRayExporter::clearCaches()
{
	shaderExporter.reset();
	objectExporter.reset();
}

QString VRayExporter::getPluginName(const OP_Node &opNode, const QString &prefix, const QString &suffix)
{
	QString pluginName = prefix % opNode.getFullPath().buffer() % suffix;

	// AppSDK doesn't like "/" for some reason.
	pluginName = pluginName.replace('/', '|');

	// Remove last | for more readable *.vrscene.
	if (pluginName.endsWith('|')) {
		pluginName.chop(1);
	}

	return pluginName;
}

QString VRayExporter::getPluginName(OBJ_Node &objNode)
{
	QString pluginName;

	const OBJ_OBJECT_TYPE ob_type = objNode.getObjectType();
	if (ob_type & OBJ_LIGHT) {
		pluginName = getPluginName(objNode, SL("Light@"));
	}
	else if (ob_type & OBJ_CAMERA) {
		pluginName = getPluginName(objNode, SL("Camera@"));
	}
	else if (ob_type == OBJ_GEOMETRY) {
		pluginName = getPluginName(objNode, SL("Node@"));
	}

	return pluginName;
}

VRay::VUtils::CharStringRefList VRayExporter::getSceneName(const OP_Node &opNode, int primID)
{
	const UT_String &nodeName = opNode.getName();

	// getFullPath() starts and ends with "/".
	QString nodePath = SL("scene") % opNode.getFullPath().buffer();
	if (primID >= 0) {
		nodePath.append(QString::number(primID));
	}

	VRay::VUtils::CharStringRefList sceneName(2);
	sceneName[0] = nodeName.buffer();
	sceneName[1] = qPrintable(nodePath);

	return sceneName;
}

VRay::VUtils::CharStringRefList VRayExporter::getSceneName(const tchar *name)
{
	VRay::VUtils::CharStringRefList sceneName(1);
	sceneName[0].set(name);
	return sceneName;
}

VRay::Transform VRayExporter::Matrix4ToTransform(const UT_Matrix4D &m, bool flip)
{
	VRay::Transform tm;
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			tm.matrix[i][j] = m[i][j];
		}
		tm.offset[i] = m[3][i];
	}

	if (flip) {
		VUtils::swap(tm.matrix[1], tm.matrix[2]);
	}

	return tm;
}

VRay::Transform VRayExporter::getObjTransform(OBJ_Node *obj_node, OP_Context &context, bool flip)
{
	UT_Matrix4D matrix;
	obj_node->getLocalToWorldTransform(context, matrix);

	return VRayExporter::Matrix4ToTransform(matrix, flip);
}


void VRayExporter::getObjTransform(OBJ_Node *obj_node, OP_Context &context, float tm[4][4])
{
	UT_Matrix4D matrix;
	obj_node->getLocalToWorldTransform(context, matrix);

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			tm[i][j] = matrix[i][j];
		}
	}
}


void VRayExporter::TransformToMatrix4(const VUtils::TraceTransform &tm, UT_Matrix4 &m)
{
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			m(i,j) = tm.m[i][j];
		}
		m(3,i) = tm.offs[i];
	}
}


OBJ_Node *VRayExporter::getCamera(const OP_Node *rop)
{
	vassert(rop);

	OBJ_Node *camera = nullptr;

	OP_Node *node = getOpNodeFromAttr(*rop, "render_camera");
	if (node) {
		camera = node->castToOBJNode();
	}

	return camera;
}


OP_Node* VRayExporter::FindChildNodeByType(OP_Node *op_node, const QString &op_type)
{
	OP_NodeList childNodes;
	op_node->getAllChildren(childNodes);

	for (const auto &childIt : childNodes) {
		const UT_String &opType = childIt->getOperator()->getName();
		if (op_type == opType.buffer()) {
			return childIt;
		}
	}

	return nullptr;
}

template <typename ListType>
static void mergePluginListToValueList(Attrs::QValueList &dstList, const ListType &srcList)
{
	for (const VRay::Plugin &plugin : srcList) {
		if (plugin.isEmpty())
			continue;

		dstList.append(VRay::VUtils::Value(plugin));
	}
}

void VRayExporter::setAttrValueFromOpNodePrm(Attrs::PluginDesc &pluginDesc,
                                             const Parm::AttrDesc &attrDesc,
                                             const OP_Node &opNode,
                                             const QString &parmNameStr)
{
	if (!Parm::isParmExist(opNode, parmNameStr))
		return;

	const char *parmName = qPrintable(parmNameStr);

	const PRM_Parm &parm = *Parm::getParm(opNode, parmNameStr);

	if (parm.getParmOwner()->isPendingOverride()) {
		Log::getLog().debug("Pending override: %s %s",
		                    opNode.getName().buffer(), parmName);
	}

	const fpreal t = m_context.getTime();

	int addAttr = true;

	Attrs::PluginAttr attr(attrDesc.attr);
	attr.setAnimated(parm.isTimeDependent());

	if (attrDesc.value.type == Parm::eBool ||
	    attrDesc.value.type == Parm::eInt ||
	    attrDesc.value.type == Parm::eTextureInt) {
		attr.paramType = Attrs::AttrTypeInt;
		attr.paramValue.valInt = opNode.evalInt(parmName, 0, t);
	}
	else if (attrDesc.value.type == Parm::eEnum) {
		UT_String enumValue;
		opNode.evalString(enumValue, parmName, 0, t);

		if (enumValue.isInteger()) {
			attr.paramType = Attrs::AttrTypeInt;
			attr.paramValue.valInt = enumValue.toInt();
		}
		else if (pluginDesc.pluginID == SL("UVWGenEnvironment")) {
			// UVWGenEnvironment is the only plugin with enum with the string keys.
			attr.paramType = Attrs::AttrTypeString;
			attr.paramValue.valString = enumValue.buffer();
		}
		else {
			Log::getLog().error("Incorrect enum: %s.%s!",
			                    qPrintable(pluginDesc.pluginID),
			                    qPrintable(attrDesc.attr));
		}
	}
	else if (attrDesc.value.type == Parm::eFloat ||
	         attrDesc.value.type == Parm::eTextureFloat) {
		attr.paramType = Attrs::AttrTypeFloat;
		attr.paramValue.valVector[0] = opNode.evalFloat(parmName, 0, t);

		if (attrDesc.flags & Parm::attrFlagToRadians) {
			attr.paramValue.valVector[0] *= RAD_TO_DEG;
		}
	}
	else if (attrDesc.value.type == Parm::eColor ||
	         attrDesc.value.type == Parm::eAColor ||
	         attrDesc.value.type == Parm::eTextureColor)
	{
		if (parm.getType().isFloatType()) {
			attr.paramType = Attrs::AttrTypeColor;
			attr.paramValue.valVector[0] = opNode.evalFloat(parmName, 0, t);
			attr.paramValue.valVector[1] = opNode.evalFloat(parmName, 1, t);
			attr.paramValue.valVector[2] = opNode.evalFloat(parmName, 2, t);
			if (attrDesc.value.type != Parm::eColor) {
				attr.paramType = Attrs::AttrTypeAColor;
				attr.paramValue.valVector[3] = opNode.evalFloat(parmName, 3, t);
			}
		}
	}
	else if (attrDesc.value.type == Parm::eVector) {
		if (parm.getType().isFloatType()) {
			attr.paramType = Attrs::AttrTypeVector;
			attr.paramValue.valVector[0] = opNode.evalFloat(parmName, 0, t);
			attr.paramValue.valVector[1] = opNode.evalFloat(parmName, 1, t);
			attr.paramValue.valVector[2] = opNode.evalFloat(parmName, 2, t);
		}
	}
	else if (attrDesc.value.type == Parm::eString) {
		UT_String buf;
		opNode.evalString(buf, parmName, 0, t);

		attr.paramType = Attrs::AttrTypeString;
		attr.paramValue.valString = buf.buffer();
	}
	else if (attrDesc.value.type == Parm::eListNode) {
		DelayedExportItem item;
		item.type = DelayedExportItem::ItemType::typeExcludeList;
		item.opNode = &opNode;
		item.pluginName = pluginDesc.pluginName;
		item.pluginID = pluginDesc.pluginID;
		item.parmName = parmNameStr;
		item.attrDesc = attrDesc;

		delayedExport.append(item);
	}
	else if (attrDesc.value.type > Parm::eManualExportStart &&
	         attrDesc.value.type < Parm::eManualExportEnd)
	{
		// These are fake params and must be handled manually
		addAttr = false;
	}
	else if (attrDesc.value.type < Parm::eOutputPlugin) {
		Log::getLog().debug("Unhandled param type: %s at %s [%i]",
		                    parmName, opNode.getOperator()->getName().buffer(), attrDesc.value.type);
		addAttr = false;
	}

	if (addAttr) {
		pluginDesc.add(attr);
	}
}

void VRayExporter::setAttrsFromOpNodePrms(Attrs::PluginDesc &pluginDesc, const OP_Node *opNode, const QString &prefix,
                                          bool remapInterp)
{
	if (const VOP_Node *vopNode = CAST_VOPNODE(opNode)) {
		// This handles "parameter" VOP overrides.
		setAttrsFromNetworkParameters(pluginDesc, *vopNode);
	}

	const Parm::VRayPluginInfo *pluginInfo = Parm::getVRayPluginInfo(pluginDesc.pluginID);
	if (!pluginInfo) {
		Log::getLog().error("Node \"%s\": Plugin \"%s\" description is not found!",
		                    opNode->getName().buffer(), qPrintable(pluginDesc.pluginID));
	}
	else {
		FOR_CONST_IT (Parm::AttributeDescs, aIt, pluginInfo->attributes) {
			const Parm::AttrDesc &attrDesc = aIt.value();
			const QString &attrName = attrDesc.attr;

			if (pluginDesc.contains(attrName) || attrDesc.flags & Parm::attrFlagCustomHandling) {
				continue;
			}

			const QString &parmName = prefix % attrName;

			int isAnimated = false;

			const PRM_Parm *parm = Parm::getParm(*opNode, parmName);
			if (parm) {
				isAnimated = parm->isTimeDependent();

				if (attrDesc.flags & Parm::attrFlagEnabledOnly) {
					if (!parm->getEnableState() || !parm->getVisibleState()) {
						continue;
					}
				}
			}

			const bool isTextureAttr = attrDesc.value.type >= Parm::eTextureColor &&
			                           attrDesc.value.type <= Parm::eTextureTransform;

			if (isTextureAttr && parm && parm->getType().isStringType()) {
				const UT_String &opPath = getOpPathFromAttr(*opNode, parm->getToken());

				const VRay::Plugin opPlugin = exportNodeFromPath(opPath);
				if (opPlugin.isNotEmpty()) {
					pluginDesc.add(attrName, opPlugin);
				}
			}
			else if (!(attrDesc.flags & Parm::attrFlagLinkedOnly)) {
				if (attrDesc.value.type == Parm::eRamp) {
					Texture::exportRampAttribute(*this,
					                             pluginDesc,
					                             *opNode,
					                             /* Houdini ramp attr */ parmName,
					                             /* V-Ray attr: colors */ attrDesc.value.colorRampInfo.colors,
					                             /* V-Ray attr: pos    */ attrDesc.value.colorRampInfo.positions,
					                             /* V-Ray attr: interp */ attrDesc.value.colorRampInfo.interpolations,
					                             /* As color list not plugin */ attrDesc.value.colorRampInfo.colorAsTexture,
					                             /* Remap to vray interpolations*/ remapInterp);
				}
				else if (attrDesc.value.type == Parm::eCurve) {
					VRay::VUtils::IntRefList interpolations;
					VRay::VUtils::FloatRefList positions;
					VRay::VUtils::FloatRefList values;

					int isRampAnimated = false;

					Texture::getCurveData(*this,
					                      *opNode,
					                      /* Houdini curve attr */ parmName,
					                      /* V-Ray attr: interp */ interpolations,
					                      /* V-Ray attr: x      */ positions,
					                      /* V-Ray attr: y      */ values,
					                      /* Is ramp  animated  */ isRampAnimated);

					pluginDesc.add(attrDesc.value.curveRampInfo.interpolations, interpolations, isRampAnimated);
					pluginDesc.add(attrDesc.value.curveRampInfo.positions, positions, isRampAnimated);
					pluginDesc.add(attrDesc.value.curveRampInfo.values, values, isRampAnimated);
				}
				else {
					setAttrValueFromOpNodePrm(pluginDesc, attrDesc, *opNode, parmName);
				}
			}
		}
	}
}


bool VRayExporter::setAttrsFromUTOptions(Attrs::PluginDesc &pluginDesc, const UT_Options &options) const
{
	const Parm::VRayPluginInfo *pluginInfo = Parm::getVRayPluginInfo(pluginDesc.pluginID);
	if (!pluginInfo) 
		return false;

	FOR_CONST_IT (Parm::AttributeDescs, aIt, pluginInfo->attributes) {
		const Parm::AttrDesc &attrDesc = aIt.value();

		const QString &attrName = attrDesc.attr;
		const char *attrNameChar = qPrintable(attrName);

		if (!options.hasOption(attrNameChar) || pluginDesc.contains(attrName)) {
			continue;
		}

		Attrs::PluginAttr attr(attrName);

		if (attrDesc.value.type == Parm::eBool ||
		    attrDesc.value.type == Parm::eInt ||
		    attrDesc.value.type == Parm::eTextureInt)
		{
			attr.paramType = Attrs::AttrTypeInt;
			attr.paramValue.valInt = options.getOptionI(attrNameChar);
		}
		else if (attrDesc.value.type == Parm::eEnum) {
			attr.paramType = Attrs::AttrTypeInt;
			attr.paramValue.valInt = options.getOptionI(attrNameChar);
		}
		else if (attrDesc.value.type == Parm::eFloat ||
		         attrDesc.value.type == Parm::eTextureFloat)
		{
			attr.paramType = Attrs::AttrTypeFloat;
			attr.paramValue.valVector[0] = options.getOptionF(attrNameChar);

			if (attrDesc.flags & Parm::attrFlagToRadians) {
				attr.paramValue.valVector[0] *= RAD_TO_DEG;
			}
		}
		else if (attrDesc.value.type == Parm::eColor) {
			attr.paramType = Attrs::AttrTypeColor;

			const UT_Vector3D &valVector = options.getOptionV3(attrNameChar);

			attr.paramValue.valVector[0] = valVector(0);
			attr.paramValue.valVector[1] = valVector(1);
			attr.paramValue.valVector[2] = valVector(2);
		}
		else if (attrDesc.value.type == Parm::eAColor ||
		         attrDesc.value.type == Parm::eTextureColor)
		{
			attr.paramType = Attrs::AttrTypeAColor;

			const UT_Vector4D &valVector = options.getOptionV4(attrNameChar);

			attr.paramValue.valVector[0] = valVector(0);
			attr.paramValue.valVector[1] = valVector(1);
			attr.paramValue.valVector[2] = valVector(2);
			attr.paramValue.valVector[3] = valVector(3);
		}
		else if (attrDesc.value.type == Parm::eString) {
			attr.paramType = Attrs::AttrTypeString;
			attr.paramValue.valString = options.getOptionS(attrNameChar);
		}

		if (attr.paramType != Attrs::AttrTypeUnknown) {
			pluginDesc.add(attr);
		}
	}

	return true;
}


VRayExporter::VRayExporter(OP_Node *rop)
	: m_rop(rop)
	, m_renderMode(0)
	, m_isAborted(0)
	, m_frames(0)
	, m_error(ROP_CONTINUE_RENDER)
	, m_workMode(ExpRender)
	, sessionType(VfhSessionType::production)
	, m_isGPU(0)
	, m_isAnimation(false)
	, m_isMotionBlur(0)
	, m_isVelocityOn(0)
	, m_timeStart(0)
	, m_timeEnd(0)
	, objectExporter(*this)
	, shaderExporter(*this)
{
	Log::getLog().debug("VRayExporter()");
}

VRayExporter::~VRayExporter()
{
	Log::getLog().debug("~VRayExporter()");

	resetOpCallbacks();
}

enum ImageFormat {
	imageFormatPNG = 0,
	imageFormatJPEG,
	imageFormatTIFF,
	imageFormatTGA,
	imageFormatSGI,
	imageFormatOpenEXR,
	imageFormatVRayImage,
	imageFormatLast
};

static const char* const imgFormatExt[imageFormatLast] = {
	".png",
	".jpg",
	".tiff",
	".tga",
	".sgi",
	".exr",
	".vrimg"
};

static ImageFormat getImgFormat(const UT_String& filePath)
{
	for (int imgFormat = 0; imgFormat < static_cast<int>(imageFormatLast); ++imgFormat) {
		if (filePath.endsWith(imgFormatExt[imgFormat], false)) {
			return static_cast<ImageFormat>(imgFormat);
		}
	}
	
	return imageFormatLast;
}

static void fillSettingsRegionsGenerator(OP_Node &rop, Attrs::PluginDesc &pluginDesc)
{
	const int bucketW = rop.evalInt("SettingsRegionsGenerator_xc", 0, 0.0);
	int bucketH = bucketW;

	const int lockBucketSize = rop.evalInt("SettingsRegionsGenerator_lock_size", 0, 0.0);
	if (!lockBucketSize) {
		bucketH = rop.evalInt("SettingsRegionsGenerator_yc", 0, 0.0);
	}

	pluginDesc.add(Attrs::PluginAttr(SL("xc"), bucketW));
	pluginDesc.add(Attrs::PluginAttr(SL("yc"), bucketH));
}

static void fillSettingsImageSampler(OP_Node &rop, Attrs::PluginDesc &pluginDesc)
{
	UT_String _renderMaskObjectIDS;
	rop.evalString(_renderMaskObjectIDS, "SettingsImageSampler_render_mask_object_ids", 0, 0.0);

	const QString renderMaskObjectIDS(_renderMaskObjectIDS.buffer());
	if (!renderMaskObjectIDS.isEmpty()) {
		const QStringList renderMaskObjectIDSList = renderMaskObjectIDS.split(' ');

		VRay::VUtils::IntRefList renderMaskObjectIDs(renderMaskObjectIDSList.size());
		for (int i = 0; i < renderMaskObjectIDSList.size(); ++i) {
			const QString &objectID = renderMaskObjectIDSList[i];
			renderMaskObjectIDs[i] = objectID.toInt();
		}
		if (renderMaskObjectIDs.count()) {
			pluginDesc.add(Attrs::PluginAttr("render_mask_object_ids", renderMaskObjectIDs));
		}
	}

	UT_String _renderMaskObjects;
	rop.evalString(_renderMaskObjects, "SettingsImageSampler_render_mask_objects", 0, 0.0);

	const QString renderMaskObjectNames(_renderMaskObjects.buffer());
	if (!renderMaskObjectNames.isEmpty()) {
		const QStringList renderMaskObjectNamesList = renderMaskObjectNames.split(' ');

		VRay::VUtils::ValueRefList renderMaskObjects(renderMaskObjectNamesList.size());
		for (const QString &opName : renderMaskObjectNamesList) {
			// TODO:
			//   [ ] Add object plugins to list
			//   [ ] Add bundles
		}

		if (renderMaskObjects.count()) {
			pluginDesc.add(Attrs::PluginAttr("render_mask_objects", renderMaskObjects));
		}
	}

	const int minSubdivs = rop.evalInt("SettingsImageSampler_dmc_minSubdivs", 0, 0.0);
	int maxSubdivs = minSubdivs;

	const int lockSubdivs = rop.evalInt("SettingsImageSampler_dmc_lockSubdivs", 0, 0.0);
	if (!lockSubdivs) {
		maxSubdivs = rop.evalInt("SettingsImageSampler_dmc_maxSubdivs", 0, 0.0);
	}

	pluginDesc.add(Attrs::PluginAttr("dmc_minSubdivs", minSubdivs));
	pluginDesc.add(Attrs::PluginAttr("dmc_maxSubdivs", maxSubdivs));
}

ReturnValue VRayExporter::fillSettingsOutput(Attrs::PluginDesc &pluginDesc)
{
	const fpreal t = getContext().getTime();
	OBJ_Node *camera = VRayExporter::getCamera(m_rop);

	if (!camera) {
		Log::getLog().error("Camera does not exist! In VrayExporter::fillSettingsOutput");
		return ReturnValue::Error;
	}

	fpreal pixelAspect = camera->evalFloat("aspect", 0, t);

	UT_String resfraction;
	m_rop->evalString(resfraction, "res_fraction", 0, t);
	if (m_rop->evalInt("override_camerares", 0, t) &&
		!resfraction.isFloat())
	{
		pixelAspect = m_rop->evalFloat("aspect_override", 0, t);
	}

	pluginDesc.add(Attrs::PluginAttr("img_pixelAspect", pixelAspect));

	if (sessionType == VfhSessionType::rt ||
		sessionType == VfhSessionType::ipr ||
		!m_rop->evalInt("SettingsOutput_img_save", 0, 0.0))
	{
		pluginDesc.setIngore(SL("img_dir"));
		pluginDesc.setIngore(SL("img_file"));
	}
	else {
		UT_String _filePathRaw;
		m_rop->evalStringRaw(_filePathRaw, "SettingsOutput_img_file_path", 0, 0.0);

		// Replace frame number with V-Ray compatible frame pattern.
		QString filePathRaw(_filePathRaw.buffer());
		QRegularExpressionMatch frameMatchRes = frameMatchExpr.match(filePathRaw);
		if (frameMatchRes.hasMatch()) {
			int numPaddedDigits = 1;

			const int hasPadding = frameMatchRes.lastCapturedIndex() == frameNumberMatchedItemFramePadding;
			if (hasPadding) {
				numPaddedDigits = frameMatchRes.captured(frameNumberMatchedItemFramePadding).toInt();
			}

			filePathRaw = filePathRaw.replace(frameMatch, "#");

			Log::getLog().debug("Output path: %s", qPrintable(filePathRaw));

			pluginDesc.add(Attrs::PluginAttr("img_file_needFrameNumber", 1));
			pluginDesc.add(Attrs::PluginAttr("anim_frame_padding", numPaddedDigits));
		}

		UT_String reconstructedPath(qPrintable(filePathRaw));
		UT_String dirPathRaw;
		UT_String fileNameRaw;
		reconstructedPath.splitPath(dirPathRaw, fileNameRaw);

		// Format dirPathRaw.
		dirPathRaw.append('/');

		UT_String dirPath;
		UT_String fileName;

		// Expand all the other variables.
		CH_Manager *chanMan = OPgetDirector()->getChannelManager();
		chanMan->expandString(fileNameRaw.buffer(), fileName, t);
		chanMan->expandString(dirPathRaw.buffer(), dirPath, t);

		// fileName must be relative path inside dirPath, but if it starts with a variable which when expanded contains
		// leading slash, then fileName will have leading slash which will make it not relative
		// for exmaple ${HIPNAME}/result.exr could expand to /HBATCH/scene_name/result.exr
		makePathRelative(fileName);

		if (sessionType != VfhSessionType::cloud) {
			// Create output directory.
			if (!directoryCreator.mkpath(dirPath.buffer())) {
				Log::getLog().error("Failed to create output directory \"%s\"!", dirPath.buffer());
				return ReturnValue::Error;
			}
		}

		// Append default file type if not set.
		ImageFormat imgFormat = getImgFormat(fileName.buffer());
		if (imgFormat == imageFormatLast) {
			imgFormat = imageFormatOpenEXR;
			fileName.append(imgFormatExt[imageFormatOpenEXR]);

			Log::getLog().warning("Output image file format not supported/recognized! Setting output image format to Open EXR.");
		}

		if (imgFormat == imageFormatOpenEXR ||
			imgFormat == imageFormatVRayImage)
		{
			const int relementsSeparateFiles = m_rop->evalInt("SettingsOutput_relements_separateFiles", 0, t);
			if (!relementsSeparateFiles) {
				pluginDesc.add(Attrs::PluginAttr("img_rawFile", 1));
			}
		}

		pluginDesc.add(Attrs::PluginAttr(SL("img_dir"), dirPath.buffer()));
		pluginDesc.add(Attrs::PluginAttr(SL("img_file"), fileName.buffer()));
	}

	VRay::VUtils::ValueRefList frames(1);

	if (exportFilePerFrame) {
		frames[0].setDouble(getContext().getFloatFrame());
	}
	else {
		animInfo.frameStart = CAST_ROPNODE(m_rop)->FSTART();
		animInfo.frameEnd = CAST_ROPNODE(m_rop)->FEND();
		animInfo.frameStep = CAST_ROPNODE(m_rop)->FINC();

		frames[0].setDouble(animInfo.frameStart);

		if (m_frames > 1) {
			if (animInfo.frameStep > 1) {
				frames = VRay::VUtils::ValueRefList(m_frames);
				for (int i = 0; i < m_frames; ++i) {
					frames[i].setDouble(animInfo.frameStart + i * animInfo.frameStep);
				}
			}
			else {
				VRay::VUtils::ValueRefList frameRange(2);
				frameRange[0].setDouble(animInfo.frameStart);
				frameRange[1].setDouble(animInfo.frameEnd);
				frames[0].setList(frameRange);
			}
		}
	}

	pluginDesc.add(Attrs::PluginAttr(SL("frames"), frames));

	return ReturnValue::Success;
}


ReturnValue VRayExporter::exportSettings()
{
	if (RenderSettingsPlugins.empty()) {
		RenderSettingsPlugins.insert(SL("SettingsOptions"));
		RenderSettingsPlugins.insert(SL("SettingsColorMapping"));
		RenderSettingsPlugins.insert(SL("SettingsDMCSampler"));
		RenderSettingsPlugins.insert(SL("SettingsImageSampler"));
		RenderSettingsPlugins.insert(SL("SettingsGI"));
		RenderSettingsPlugins.insert(SL("SettingsIrradianceMap"));
		RenderSettingsPlugins.insert(SL("SettingsLightCache"));
		RenderSettingsPlugins.insert(SL("SettingsDMCGI"));
		RenderSettingsPlugins.insert(SL("SettingsRaycaster"));
		RenderSettingsPlugins.insert(SL("SettingsRegionsGenerator"));
		RenderSettingsPlugins.insert(SL("SettingsOutput"));
		RenderSettingsPlugins.insert(SL("SettingsCaustics"));
		RenderSettingsPlugins.insert(SL("SettingsDefaultDisplacement"));
	}

	for (const QString &sp : RenderSettingsPlugins) {
		const Parm::VRayPluginInfo *pluginInfo = Parm::getVRayPluginInfo(sp);
		if (!pluginInfo) {
			Log::getLog().error("Plugin \"%s\" description is not found!", qPrintable(sp));
		}
		else {
			Attrs::PluginDesc pluginDesc(sp, sp);
			if (sp == SL("SettingsOutput")) {
				if (fillSettingsOutput(pluginDesc) == ReturnValue::Error) {
					return ReturnValue::Error;
				}
			}
			else if (sp == SL("SettingsRegionsGenerator")) {
				fillSettingsRegionsGenerator(*m_rop, pluginDesc);
			}
			else if (sp == SL("SettingsImageSampler")) {
				fillSettingsImageSampler(*m_rop, pluginDesc);
			}

			setAttrsFromOpNodePrms(pluginDesc, m_rop, sp % SL("_"));
			exportPlugin(pluginDesc);
		}
	}

	CH_Manager &chanMan = *OPgetDirector()->getChannelManager();

	Attrs::PluginDesc pluginDesc(SL("settingsUnitsInfo"),
	                             SL("SettingsUnitsInfo"));

	pluginDesc.add(Attrs::PluginAttr(SL("scene_upDir"), VRay::Vector(0.0f, 1.0f, 0.0f)));
	pluginDesc.add(Attrs::PluginAttr(SL("meters_scale"), chanMan.getUnitLength()));
	pluginDesc.add(Attrs::PluginAttr(SL("seconds_scale"), chanMan.getSecsPerSample()));
	pluginDesc.add(Attrs::PluginAttr(SL("frames_scale"), chanMan.getSamplesPerSec()));

	exportPlugin(pluginDesc);

	return ReturnValue::Success;
}


void VRayExporter::exportEnvironment(OP_Node *op_node)
{
	exportShaderNode(CAST_VOPNODE(op_node));
}


void VRayExporter::exportEffects(OP_Node *op_net)
{
	// Test simulation export
	// Add simulations from ROP
	OP_Node *sim_node = FindChildNodeByType(op_net, SL("VRayNodePhxShaderSimVol"));
	if (sim_node) {
		exportShaderNode(CAST_VOPNODE(sim_node));
	}
}


void VRayExporter::exportRenderChannels(OP_Node *op_node)
{
	exportShaderNode(CAST_VOPNODE(op_node));
}


void VRayExporter::RtCallbackVop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);
	if (exporter.inSceneExport)
		return;

	if (!csect.tryEnter())
		return;

	Log::getLog().debug("RtCallbackVop: %s from \"%s\"",
					   OPeventToString(type), caller->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED: {
			if (Parm::isParmSwitcher(*caller, reinterpret_cast<intptr_t>(data))) {
				break;
			}
		}
		case OP_INPUT_CHANGED:
		case OP_INPUT_REWIRED: {
			ShaderExporter &shaderExporter = exporter.getShaderExporter();
			shaderExporter.reset();
			shaderExporter.exportShaderNode(*caller);
			break;
		}
		case OP_NODE_PREDELETE: {
			exporter.delOpCallback(caller, VRayExporter::RtCallbackVop);
			break;
		}
		default:
			break;
	}

	csect.leave();
}


VRay::Plugin VRayExporter::exportShaderNode(OP_Node *opNode)
{
	if (!opNode)
		return VRay::Plugin();
	return VRay::Plugin(shaderExporter.exportShaderNode(*opNode));
}


void VRayExporter::RtCallbackDisplacementObj(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);
	if (exporter.inSceneExport)
		return;

	if (!csect.tryEnter())
		return;

	Log::getLog().debug("RtCallbackDisplacementObj: %s from \"%s\"",
					   OPeventToString(type), caller->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED: {
			if (Parm::isParmSwitcher(*caller, reinterpret_cast<intptr_t>(data))) {
				break;
			}

			const PRM_Parm *parm = Parm::getParm(*caller, reinterpret_cast<intptr_t>(data));
			if (parm) {
				OBJ_Node *obj_node = caller->castToOBJNode();
				if (obj_node) {
					exporter.exportObject(obj_node);
				}
			}
			break;
		}
		case OP_NODE_PREDELETE: {
			exporter.delOpCallback(caller, VRayExporter::RtCallbackDisplacementObj);
			break;
		}
		default:
			break;
	}

	csect.leave();
}


void VRayExporter::RtCallbackDisplacementShop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);
	if (exporter.inSceneExport)
		return;

	if (!csect.tryEnter())
		return;

	Log::getLog().debug("RtCallbackDisplacementShop: %s from \"%s\"",
					   OPeventToString(type), caller->getName().buffer());

	if (type == OP_INPUT_REWIRED) {
		UT_String inputName;
		const int idx = reinterpret_cast<intptr_t>(data);
		caller->getInputName(inputName, idx);

		if (caller->error() < UT_ERROR_ABORT && inputName.equal("Geometry")) {
			SHOP_Node *shop_node = caller->getParent()->castToSHOPNode();
			if (shop_node) {
				UT_String shopPath;
				shop_node->getFullPath(shopPath);

				OP_NodeList refs;
				shop_node->getExistingOpDependents(refs, true);
				for (OP_Node *node : refs) {
					UT_String nodePath;
					node->getFullPath(nodePath);

					OBJ_Node *obj_node = node->castToOBJNode();
					if (obj_node) {
						exporter.exportObject(obj_node);
					}
				}
			}
		}
	}
	else if (type == OP_NODE_PREDELETE) {
		exporter.delOpCallback(caller, VRayExporter::RtCallbackDisplacementShop);
	}

	csect.leave();
}


void VRayExporter::RtCallbackDisplacementVop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);
	if (exporter.inSceneExport)
		return;

	if (!csect.tryEnter())
		return;

	Log::getLog().debug("RtCallbackDisplacementVop: %s from \"%s\"",
					   OPeventToString(type), caller->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED: {
			if (Parm::isParmSwitcher(*caller, reinterpret_cast<intptr_t>(data))) {
				break;
			}
		}
		case OP_INPUT_REWIRED: {
			const int idx = reinterpret_cast<intptr_t>(data);

			OP_Node *matNode = caller->getParent();
			if (idx >= 0 && matNode) {
				OP_NodeList refs;
				matNode->getExistingOpDependents(refs, true);

				for (OP_Node *node : refs) {
					OBJ_Node *objNode = node->castToOBJNode();
					if (objNode) {
						exporter.exportObject(objNode);
					}
				}
			}
			break;
		}
		case OP_NODE_PREDELETE: {
			exporter.delOpCallback(caller, VRayExporter::RtCallbackDisplacementVop);
			break;
		}
		default:
			break;
	}

	csect.leave();
}

int VRayExporter::exportDisplacementTexture(OP_Node &opNode, Attrs::PluginDesc &pluginDesc, const QString &parmNamePrefix, VRayDisplacementType displaceType)
{
	const fpreal t = getContext().getTime();
	const QString &attrName = parmNamePrefix % SL("displacement_texture");

	VRay::PluginRef texture;

	const PRM_Parm *parm = Parm::getParm(opNode, qPrintable(attrName));
	if (parm) {
		const UT_String &texPath = getOpPathFromAttr(opNode, attrName, t);
		if (texPath.isstring()) {
			texture = exportNodeFromPathWithDefaultMapping(texPath,
			                                               defaultMappingChannelName,
			                                               bitmapBufferColorSpaceLinear);
		}
	}

	if (texture.isEmpty()) {
		if (VOP_Node *vopNode = CAST_VOPNODE(&opNode)) {
			texture = shaderExporter.exportConnectedSocket(*vopNode, SL("displacement_tex"));
		}
	}

	if (texture.isEmpty())
		return false;

	if (displaceType == displ_type_3d ||
		displaceType == displ_type_2d)
	{
		// If plugin doesn't have "out_intensity" output wrap it into TexOutput.
		if (!ShaderExporter::hasPluginOutput(texture, SL("out_intensity"))) {
			Attrs::PluginDesc texOutputDesc(SL("Out@") % texture.getName(),
											SL("TexOutput"));
			texOutputDesc.add(SL("texmap"), texture);

			texture = exportPlugin(texOutputDesc);
		}

		pluginDesc.add(SL("displacement_tex_float"), texture, SL("out_intensity"));
	}
	else {
		pluginDesc.add(SL("displacement_tex_color"), texture);
	}

	return true;
}

int VRayExporter::exportDisplacementFromSubdivInfo(const SubdivInfo &subdivInfo, struct Attrs::PluginDesc &pluginDesc)
{
	const QString &parmNamePrefix = subdivInfo.needParmNamePrefix() ? pluginDesc.pluginID % SL("_") : SL("");

	const QString displacementTypeAttr = parmNamePrefix + SL("type");

	VRayDisplacementType displacementType = displ_type_subdivision;

	if (subdivInfo.type == SubdivisionType::displacement) {
		displacementType = getDisplacementType(*subdivInfo.parmHolder, displacementTypeAttr);
		setGeomDisplacedMeshType(pluginDesc, displacementType);
	}

	exportDisplacementTexture(*subdivInfo.parmHolder, pluginDesc, parmNamePrefix, displacementType);

	setAttrsFromOpNodePrms(pluginDesc, subdivInfo.parmHolder, parmNamePrefix);

	return true;
}

static QString subdivisionPluginFromType(SubdivisionType subdivType)
{
	switch (subdivType) {
		case SubdivisionType::displacement: return SL("GeomDisplacedMesh");
		case SubdivisionType::subdivision:  return SL("GeomStaticSmoothedMesh");
		default: {
			vassert(false);
			return SL("");
		}
	}
}

VRay::Plugin VRayExporter::exportDisplacement(OBJ_Node &objNode, const VRay::Plugin &geomPlugin, const SubdivInfo &subdivInfo)
{
	if (!subdivInfo.hasSubdiv())
		return geomPlugin; 

	Attrs::PluginDesc pluginDesc;
	pluginDesc.pluginName = SL("Subdiv@") % geomPlugin.getName();
	pluginDesc.pluginID = subdivisionPluginFromType(subdivInfo.type);

	pluginDesc.add(SL("mesh"), geomPlugin);

	if (!exportDisplacementFromSubdivInfo(subdivInfo, pluginDesc))
		return geomPlugin; 

	addOpCallback(&objNode, RtCallbackDisplacementObj);

	return exportPlugin(pluginDesc);
}

static QString ObjectTypeToString(const OBJ_OBJECT_TYPE &ob_type)
{
	QString object_type;

	if (ob_type & OBJ_WORLD) {
		object_type += " | OBJ_WORLD";
	}
	if (ob_type & OBJ_GEOMETRY) {
		object_type += " | OBJ_GEOMETRY";
	}
	if (ob_type & OBJ_CAMERA) {
		object_type += " | OBJ_CAMERA";
	}
	if (ob_type & OBJ_LIGHT) {
		object_type += " | OBJ_LIGHT";
	}
	if (ob_type & OBJ_RENDERER) {
		object_type += " | OBJ_RENDERER";
	}
	if (ob_type & OBJ_FOG) {
		object_type += " | OBJ_FOG";
	}
	if (ob_type & OBJ_BONE) {
		object_type += " | OBJ_BONE";
	}
	if (ob_type & OBJ_HANDLE) {
		object_type += " | OBJ_HANDLE";
	}
	if (ob_type & OBJ_BLEND) {
		object_type += " | OBJ_BLEND";
	}
	if (ob_type & OBJ_FORCE) {
		object_type += " | OBJ_FORCE";
	}
	if (ob_type & OBJ_CAMSWITCH) {
		object_type += " | OBJ_CAMSWITCH";
	}
	if (ob_type & OBJ_SOUND) {
		object_type += " | OBJ_SOUND";
	}
	if (ob_type & OBJ_MICROPHONE) {
		object_type += " | OBJ_MICROPHONE";
	}
	if (ob_type & OBJ_SUBNET) {
		object_type += " | OBJ_SUBNET";
	}
	if (ob_type & OBJ_FETCH) {
		object_type += " | OBJ_FETCH";
	}
	if (ob_type & OBJ_NULL) {
		object_type += " | OBJ_NULL";
	}
	if (ob_type & OBJ_STICKY) {
		object_type += " | OBJ_STICKY";
	}
	if (ob_type & OBJ_DOPNET) {
		object_type += " | OBJ_DOPNET";
	}
	if (ob_type & OBJ_RIVET) {
		object_type += " | OBJ_RIVET";
	}
	if (ob_type & OBJ_MUSCLE) {
		object_type += " | OBJ_MUSCLE";
	}

	return object_type;
}


void VRayExporter::resetOpCallbacks()
{
	for (auto const &item : m_opRegCallbacks) {
		if (item.op_node->hasOpInterest(item.cb_data, item.cb)) {
			item.op_node->removeOpInterest(item.cb_data, item.cb);
		}
	}

	m_opRegCallbacks.clear();
}

void VRayExporter::addOpCallback(OP_Node *op_node, OP_EventMethod cb)
{
	if (sessionType == VfhSessionType::production)
		return;
	if (op_node->hasOpInterest(this, cb))
		return;

	Log::getLog().debug("addOpInterest(%s)",
						op_node->getName().buffer());

	op_node->addOpInterest(this, cb);

	// Store registered callback for faster removal
	m_opRegCallbacks.push_back(OpInterestItem(op_node, cb, this));
}


void VRayExporter::delOpCallback(OP_Node *op_node, OP_EventMethod cb)
{
	if (sessionType == VfhSessionType::production)
		return;
	if (!op_node->hasOpInterest(this, cb))
		return;

	Log::getLog().debug("removeOpInterest(%s)",
						op_node->getName().buffer());

	op_node->removeOpInterest(this, cb);
}


void VRayExporter::delOpCallbacks(OP_Node *op_node)
{
	m_opRegCallbacks.erase(std::remove_if(m_opRegCallbacks.begin(), m_opRegCallbacks.end(),
										  [op_node](OpInterestItem &item) { return item.op_node == op_node; }), m_opRegCallbacks.end());
}

/// Callback function for the event when V-Ray logs a text message.
static void onLogMessage(VRay::VRayRenderer& /*renderer*/, const char *msg, VRay::MessageLevel level, double /*instant*/, void*)
{
	const QString message(QString(msg).simplified());

	if (level <= VRay::MessageError) {
		Log::getLog().error("V-Ray: %s", qPrintable(message));
	}
	else if (level > VRay::MessageError && level <= VRay::MessageWarning) {
		Log::getLog().warning("V-Ray: %s", qPrintable(message));
	}
	else if (level > VRay::MessageWarning && level <= VRay::MessageInfo) {
		Log::getLog().info("V-Ray: %s", qPrintable(message));
	}
}

/// Callback function for the event when V-Ray updates its current computation task and the number of workunits done.
static void onProgress(VRay::VRayRenderer& /*renderer*/, const char *msg, int elementNumber, int elementsCount, double /*instant*/, void*)
{
	const QString message(QString(msg).simplified());

	const float percentage = 100.0f * elementNumber / elementsCount;

	Log::getLog().progress("V-Ray: %s %.1f%% %s",
						   qPrintable(message),
						   percentage,
						   (elementNumber >= elementsCount) ? "\n" : "\r");
}

/// Callback function for the event when rendering has finished, successfully or not.
static void onStateChanged(VRay::VRayRenderer &renderer, VRay::RendererState oldState, VRay::RendererState newState, double instant, void *data)
{
	Log::getLog().debug("onStateChanged");

	VRayExporter &self = *reinterpret_cast<VRayExporter*>(data);

	// Check abort from "Stop" button or Esc key.
	if (newState == VRay::IDLE_STOPPED ||
		renderer.isAborted())
	{
		self.setAbort();
	}

	switch (newState) {
		case VRay::IDLE_STOPPED:
		case VRay::IDLE_ERROR:
		case VRay::IDLE_DONE: {
			switch (self.getSessionType()) {
				case VfhSessionType::production: {
					// ROP_Node will call "VRayExporter::exportEnd()" and we'll free stuff there.
					break;
				}
				case VfhSessionType::rt: {
					self.exportEnd();
					break;
				}
				case VfhSessionType::ipr: {
					// Handled separately for the IPR session.
					break;
				}
				default:
					break;
			}
		}
		default:
			break;
	}
}

/// Callback function for the "Render Last" button in the VFB.
static void onRenderLast(VRay::VRayRenderer& /*renderer*/, bool /*isRendering*/, double /*instant*/, void *data)
{
	Log::getLog().debug("onRenderLast");

	VRayExporter &self = *reinterpret_cast<VRayExporter*>(data);
	self.renderLast();
}

/// Callback function when VFB closes.
static void onVfbClosed(VRay::VRayRenderer& /*renderer*/, double, void *data)
{
	Log::getLog().debug("onVfbClosed");

	VRayExporter &self = *reinterpret_cast<VRayExporter*>(data);

	switch (self.getSessionType()) {
		case VfhSessionType::production: {
			self.saveVfbState();
			break;
		}
		case VfhSessionType::rt: {
			self.saveVfbState();

			// Could be closed with out stopping the renderer.
			if (self.getRenderer().isRendering()) {
				self.exportEnd();
			}
			break;
		}
		case VfhSessionType::ipr: {
			// No VFB for the IPR session.
			break;
		}
		default:
			break;
	}
}

static void onRendererClosed(VRay::VRayRenderer& /*renderer*/, double, void *data)
{
	Log::getLog().debug("onRendererClosed");
}

void VRayExporter::exportDelayed()
{
	const fpreal t = getContext().getTime();

	for (const DelayedExportItem &item : delayedExport) {
		vassert(item.opNode);

		Attrs::PluginDesc pluginDesc(item.pluginName, item.pluginID);

		if (item.type == DelayedExportItem::ItemType::typeExcludeList) {
			if (item.attrDesc.value.type == Parm::eListNode) {
				UT_String listAttrValue;
				item.opNode->evalString(listAttrValue, qPrintable(item.parmName), 0, t);

				const int isListExcludeAll = listAttrValue.equal("*");
				const int isListExcludeNone = listAttrValue.equal("");
				if (isListExcludeAll || isListExcludeNone) {
					int isListInclusive = false;
					if (!item.attrDesc.value.nodeList.inclusiveFlag.isEmpty()) {
						isListInclusive = item.opNode->evalInt(qPrintable(item.attrDesc.value.nodeList.inclusiveFlag), 0, 0.0);
					}

					pluginDesc.add(item.attrDesc.value.nodeList.inclusiveFlag, !isListInclusive ^ isListExcludeNone);
					pluginDesc.add(item.parmName, Attrs::QValueList());
				}
				else {
					Attrs::QValueList excludeList;

					OP_Bundle *opBundle =
						getBundleFromOpNodePrm(const_cast<OP_Node&>(*item.opNode), qPrintable(item.parmName), t);
					if (opBundle) {
						OP_NodeList opList;
						opBundle->getMembers(opList);

						for (OP_Node *listNode : opList) {
							if (OBJ_Node *objNode = listNode->castToOBJNode()) {
								const ObjCacheEntry &objEntry = cacheMan.getObjEntry(*objNode);

								mergePluginListToValueList(excludeList, objEntry.nodes);
							}
						}
					}

					pluginDesc.add(item.parmName, excludeList);
				}
			}
		}
		else if (item.type == DelayedExportItem::ItemType::typeLightPlugin) {
			if (const OBJ_Node *objNode = CAST_OBJNODE(item.opNode)) {
				if (const OBJ_Light *objLight = const_cast<OBJ_Node*>(objNode)->castToOBJLight()) {
					const ObjLightCacheEntry &lightEntry = cacheMan.getLightEntry(*objLight);
					if (!lightEntry.lights.empty()) {
						pluginDesc.add(item.parmName, lightEntry.lights[0]);
					}
				}
				else if (const_cast<OBJ_Node*>(objNode)->castToOBJGeometry()) {
					const ObjCacheEntry &objEntry = cacheMan.getObjEntry(*objNode);
					if (!objEntry.nodes.empty()) {
						pluginDesc.add(item.parmName, objEntry.nodes[0]);
					}
				}
			}
		}

		// This will append new data to existing plugin.
		exportPlugin(pluginDesc);
	}

	delayedExport.clear();
}

void VRayExporter::exportScene()
{
	QAtomicIntRaii inSceneLock(inSceneExport);

	Log::getLog().debug("VRayExporter::exportScene()");

	if (sessionType != VfhSessionType::ipr) {
		exportView();
	}

	bundleMap.init();

	clearCaches();

	OP_Bundle *activeGeo = getActiveGeometryBundle(*m_rop, m_context.getTime());
	if (activeGeo) {
		for (int i = 0; i < activeGeo->entries(); ++i) {
			OP_Node *node = activeGeo->getNode(i);
			if (node) {
				exportObject(node);
			}
		}
	}

	OP_Bundle *activeLights = getActiveLightsBundle(*m_rop, m_context.getTime());
	if (activeLights) {
		for (int i = 0; i < activeLights->entries(); ++i) {
			OBJ_Node *objNode = CAST_OBJNODE(activeLights->getNode(i));
			if (objNode) {
				exportObject(objNode);
			}
		}
	}

	if (!cacheMan.numLights()) {
		exportDefaultHeadlight();
	}

	exportLightLinker();

	OP_Node *env_network = getOpNodeFromAttr(*m_rop, "render_network_environment");
	if (env_network) {
		OP_Node *env_node = FindChildNodeByType(env_network, SL("VRayNodeSettingsEnvironment"));
		if (!env_node) {
			Log::getLog().error("Node of type \"VRay SettingsEnvironment\" is not found!");
		}
		else {
			exportEnvironment(env_node);
			exportEffects(env_network);
		}
	}

	exportDelayed();

	OP_Node *channels_network = getOpNodeFromAttr(*m_rop, "render_network_render_channels");
	if (channels_network) { 
		OP_Node *chan_node = FindChildNodeByType(channels_network, SL("VRayNodeRenderChannelsContainer"));
		if (!chan_node) {
			Log::getLog().error("Node of type \"VRay RenderChannelsContainer\" is not found!");
		}
		else {
			exportRenderChannels(chan_node);
		}
	}

	if (sessionType == VfhSessionType::ipr) {
		Attrs::PluginDesc texOpId("userAttrOpId", "TexUserColor");
		texOpId.add(Attrs::PluginAttr("user_attribute", "Op_Id"));
		texOpId.add(Attrs::PluginAttr("attribute_priority", 1));

		Attrs::PluginDesc rcOpId("rcUserAttrOpId", "RenderChannelExtraTex");
		rcOpId.add(Attrs::PluginAttr("name", "Op_Id"));
		rcOpId.add(Attrs::PluginAttr("consider_for_aa", false));
		rcOpId.add(Attrs::PluginAttr("filtering", false));
		rcOpId.add(Attrs::PluginAttr("affect_matte_objects", false));
		rcOpId.add(Attrs::PluginAttr("enableDeepOutput", false));
		rcOpId.add(Attrs::PluginAttr("texmap", exportPlugin(texOpId)));
		exportPlugin(rcOpId);
	}

	bundleMap.freeMem();

	// Add callback to OP Director so new nodes can be exported during RT sessions.
	if (sessionType == VfhSessionType::rt) {
		OP_Node *objNetwork = OPgetDirector()->findNode("/obj");

		addOpCallback(objNetwork, rtCallbackObjNetwork);
		addOpCallback(OPgetDirector(), RtCallbackOPDirector);
	}
}


void VRayExporter::fillMotionBlurParams(MotionBlurParams &mbParams)
{
	OBJ_Node *camera = getCamera(m_rop);

	if (camera && usePhysicalCamera(*camera) != PhysicalCameraMode::modeNone) {
		const PhysicalCameraType cameraType = static_cast<PhysicalCameraType>(Parm::getParmInt(*camera, "CameraPhysical_type"));
		const fpreal frameDuration = OPgetDirector()->getChannelManager()->getSecsPerSample();

		switch (cameraType) {
			case PhysicalCameraType::still: {
				mbParams.mb_duration        = 1.0f / (Parm::getParmFloat(*camera, "CameraPhysical_shutter_speed") * frameDuration);
				mbParams.mb_interval_center = mbParams.mb_duration * 0.5f;
				break;
			}
			case PhysicalCameraType::cinematic: {
				mbParams.mb_duration        = Parm::getParmFloat(*camera, "CameraPhysical_shutter_angle") / 360.0f;
				mbParams.mb_interval_center = Parm::getParmFloat(*camera, "CameraPhysical_shutter_offset") / 360.0f + mbParams.mb_duration * 0.5f;
				break;
			}
			case PhysicalCameraType::video: {
				mbParams.mb_duration        = 1.0f + Parm::getParmFloat(*camera, "CameraPhysical_latency") / frameDuration;
				mbParams.mb_interval_center = -mbParams.mb_duration * 0.5f;
				break;
			}
			default: {
				vassert(false);
				break;
			}
		}
	}
	else {
		mbParams.mb_duration        = m_rop->evalFloat("SettingsMotionBlur_duration", 0, 0.0);
		mbParams.mb_interval_center = m_rop->evalFloat("SettingsMotionBlur_interval_center", 0, 0.0);
		mbParams.mb_geom_samples    = m_rop->evalInt("SettingsMotionBlur_geom_samples", 0, 0.0);
	}
}


VRay::Plugin VRayExporter::exportPlugin(const Attrs::PluginDesc &pluginDesc)
{
	return m_renderer.exportPlugin(pluginDesc);
}


void VRayExporter::exportPluginProperties(VRay::Plugin &plugin, const Attrs::PluginDesc &pluginDesc)
{
	return m_renderer.exportPluginProperties(plugin, pluginDesc);
}


void VRayExporter::removePlugin(OBJ_Node *node)
{
	if (!node)
		return;

	m_renderer.removePlugin(getPluginName(*node));
}


void VRayExporter::removePlugin(const QString &pluginName)
{
	m_renderer.removePlugin(pluginName);
}


void VRayExporter::removePlugin(VRay::Plugin plugin)
{
	m_renderer.removePlugin(plugin);
}


void VRayExporter::setSessionType(VfhSessionType value)
{
	sessionType = value;
}


void VRayExporter::setDRSettings()
{
	VRay::VRayRenderer &vray = m_renderer.getVRay();
	// clean up all previously set hosts
	vray.removeHosts(vray.getAllHosts());

	const int nDRHosts = Parm::getParmInt(*m_rop, "drhost_cnt");
	const bool drEnabled = Parm::getParmInt(*m_rop, "dr_enabled") && (nDRHosts > 0);

	vray.setDREnabled(drEnabled);

	if (drEnabled) {
		UT_String defaultHostPort;
		m_rop->evalString(defaultHostPort, "drhost_port", 0, 0.0f);

		UT_String drhosts;
		for (int i = 1; i <= nDRHosts; ++i) {
			const int hostEnabled = m_rop->evalIntInst("drhost#_enabled", &i, 0, 0.0f);
			if (NOT(hostEnabled)) {
				continue;
			}

			UT_String hostAddress;
			m_rop->evalStringInst("drhost#_address", &i, hostAddress, 0, 0.0f);
			// if address not set use default
			if (NOT(hostAddress.isstring())) {
				hostAddress = "localhost";
			}

			UT_String hostPort;
			const int useDefaultPort = m_rop->evalIntInst("drhost#_usedefaultport", &i, 0, 0.0f);
			if (NOT(useDefaultPort)) {
				m_rop->evalStringInst("drhost#_port", &i, hostPort, 0, 0.0f);
			}

			// if port not set use default
			if (NOT(hostPort.isstring())) {
				hostPort = defaultHostPort;
			}

			// skip empty parameter port
			if (NOT(hostPort.isstring())) {
				continue;
			}

			drhosts.append(hostAddress.buffer());
			drhosts.append(':');
			drhosts.append(hostPort.buffer());
			drhosts.append(';');
		}

		vray.addHosts(drhosts);
	}
}


void VRayExporter::setRenderMode(VRay::VRayRenderer::RenderMode mode)
{
	SettingsRTEngine settingsRTEngine;
	setSettingsRTEngineFromRopNode(settingsRTEngine, *m_rop, mode);
	setSettingsRTEnginetOptimizedGpuSettings(settingsRTEngine, *m_rop, mode);

	m_renderer.setRendererMode(settingsRTEngine, mode);

	m_isGPU = mode >= VRay::VRayRenderer::RENDER_MODE_INTERACTIVE_OPENCL;
}


void VRayExporter::setExportMode(VRayExporter::ExpWorkMode mode)
{
	m_workMode = mode;
}


void VRayExporter::setContext(const VRayOpContext &ctx)
{
	m_context = ctx;
}


void VRayExporter::setAbort()
{
	m_isAborted = true;
}


void VRayExporter::setRenderSize(int w, int h)
{
	Log::getLog().info("VRayExporter::setRenderSize(%i, %i)",
					   w, h);
	m_renderer.setImageSize(w, h);
}


int VRayExporter::isStereoView() const
{
	return Parm::getParmInt(*m_rop, "VRayStereoscopicSettings_use");
}

void VRayExporter::exportToVrscene(int currentFrameOnly)
{
	const fpreal t = getContext().getTime();

	UT_String exportFilepath;
	m_rop->evalString(exportFilepath, "render_export_filepath", 0, t);

	if (!exportFilepath.isstring()) {
		Log::getLog().error("Export mode is selected, but no filepath specified!");
	}
	else {
		VRay::VRayExportSettings expSettings;
		expSettings.hexArrays = m_rop->evalInt("exp_hexdata", 0, t);
		expSettings.hexTransforms = m_rop->evalInt("exp_hextm", 0, t);
		expSettings.compressed = m_rop->evalInt("exp_compressed", 0, t);

		// TODO: Set motion blur interval.
		expSettings.currentFrameOnly = currentFrameOnly;

		exportVrscene(exportFilepath.buffer(), expSettings);
	}
}

int VRayExporter::renderFrame(int locked)
{
	Log::getLog().debug("VRayExporter::renderFrame(%.3f)", m_context.getFloatFrame());

	if (sessionType == VfhSessionType::production &&
		(exportFilePerFrame || !isAnimation()) &&
	    (m_workMode == ExpExport ||
	     m_workMode == ExpExportRender))
	{
		// Pre-frame animation export.
		exportToVrscene(true);

		// TODO: Clean-up parameter values.
	}

	if (m_workMode == ExpRender || m_workMode == ExpExportRender) {
		m_renderer.startRender(locked);
	}

	return 0;
}

int VRayExporter::exportVrscene(const QString &filepath, VRay::VRayExportSettings &settings)
{
	// Create export directory.
	QFileInfo filePathInfo(filepath);
	if (!directoryCreator.mkpath(filePathInfo.absoluteDir().path())) {
		Log::getLog().error("Failed to create export directory: %s!",
							qPrintable(filePathInfo.absoluteDir().path()));
		return false;
	}

	return m_renderer.exportScene(filepath, settings);
}

void VRayExporter::clearKeyFrames(double toTime)
{
	const int clearAll = SYSalmostEqual(toTime, SYS_FP64_MAX);

	// XXX: A bit hacky.
	// Clear key-frames only if we are exporting to separate files or just rendering without export.
	if (!clearAll && !exportFilePerFrame)
		return;

	if (clearAll) {
		Log::getLog().debug("VRayExporter::clearKeyFrames(ALL)");
	}
	else {
		Log::getLog().debug("VRayExporter::clearKeyFrames(toTime = %.3f)", toTime);
	}

	m_renderer.clearFrames(toTime);
}


void VRayExporter::setAnimation(bool on)
{
	Log::getLog().debug("VRayExporter::setAnimation(%i)", on);
	m_renderer.setAnimation(on);
}

int VRayExporter::initRenderer(int hasUI, int reInit)
{
	return m_renderer.initRenderer(hasUI, reInit);
}

static int sessionSupportsAnimation(const VfhSessionType sessionType)
{
	return sessionType == VfhSessionType::production || sessionType == VfhSessionType::cloud;
}

static int sessionNeedsUI(const VfhSessionType sessionType)
{
	return sessionType == VfhSessionType::production || sessionType == VfhSessionType::rt;
}

void VRayExporter::initExporter(int hasUI, int nframes, fpreal tstart, fpreal tend)
{
	const int logLevel = m_rop->evalInt("exporter_log_level", 0, 0.0);
	Log::getLog().setLogLevel(logLevel == 0 ? Log::LogLevelError : Log::LogLevelDebug);

	OBJ_Node *camera = getCamera(m_rop);
	if (!camera) {
		Log::getLog().error("Camera is not set!");
		m_error = ROP_ABORT_RENDER;
		return;
	}

	resetOpCallbacks();

	m_viewParams = ViewParams();
	m_viewParams.firstExport = true;

	m_exportedFrames.clear();
	m_frames    = nframes;
	m_timeStart = tstart;
	m_timeEnd   = tend;
	m_isAborted = false;
	m_isAnimation = nframes > 1;
	m_isMotionBlur = hasMotionBlur(*m_rop, *camera);
	m_isVelocityOn = hasVelocityOn(*m_rop);

	// Reset time before exporting settings.
	setTime(0.0);
	setAnimation(sessionSupportsAnimation(sessionType) &&
		(m_isAnimation || m_isMotionBlur || m_isVelocityOn));

	if (hasUI) {
		if (!sessionNeedsUI(sessionType)) {
			getRenderer().showVFB(false);
		}
		else {
			if (!getRenderer().getVRay().vfb.isShown()) {
				// Set initial position.
				restoreVfbState(false);
			}

			getRenderer().getVfbSettings(vfbSettings);
			getRenderer().showVFB(m_workMode != ExpExport, m_rop->getFullPath());

			// Restore history state.
			restoreVfbState(true);
		}
	}

	m_renderer.getVRay().setOnProgress(onProgress, this);
	m_renderer.getVRay().setOnLogMessage(onLogMessage, VRay::MessageInfo, this);
	m_renderer.getVRay().setOnStateChanged(onStateChanged, this);
	m_renderer.getVRay().setOnRendererClose(onRendererClosed, this);

	if (hasUI) {
		m_renderer.getVRay().setOnVFBClosed(onVfbClosed, this);
		m_renderer.getVRay().setOnVFBRenderLast(onRenderLast, this);
	}

	const int isExportMode =
		m_workMode == ExpExport ||
		m_workMode == ExpExportRender;

	if (sessionType == VfhSessionType::cloud) {
		exportFilePerFrame = false;
	}
	else {
		exportFilePerFrame =
			isExportMode &&
			isAnimation() &&
			isExportFramesToSeparateFiles(*m_rop);
	}

	m_error = ROP_CONTINUE_RENDER;
}


int VRayExporter::hasVelocityOn(OP_Node &rop) const
{
	const fpreal t = m_context.getTime();

	OP_Node *rcNode = getOpNodeFromAttr(rop, "render_network_render_channels", t);
	if (!rcNode) {
		return false;
	}

	OP_Network *rcNetwork = UTverify_cast<OP_Network*>(rcNode);
	if (!rcNetwork) {
		return false;
	}

	OP_NodeList rcOutputList;
	if (!rcNetwork->getOpsByName("VRayNodeRenderChannelsContainer", rcOutputList)) {
		return false;
	}

	OP_NodeList velVOPList;
	if (!rcNetwork->getOpsByName("VRayNodeRenderChannelVelocity", velVOPList)) {
		return false;
	}

	OP_Node *rcOutput = rcOutputList(0);
	for (OP_Node *velVOP : velVOPList) {
		if (rcOutput->isInputAncestor(velVOP)) {
			return true;
		}
	}

	return false;
}


int VRayExporter::hasMotionBlur(OP_Node &rop, OBJ_Node &camera) const
{
	int hasMoBlur;

	if (usePhysicalCamera(camera) == PhysicalCameraMode::modeUser) {
		hasMoBlur = camera.evalInt("CameraPhysical_use_moblur", 0, 0.0);
	}
	else {
		hasMoBlur = rop.evalInt("SettingsMotionBlur_on", 0, 0.0);
	}

	return hasMoBlur;
}


void VRayExporter::showVFB()
{
	if (getRenderer().isVRayInit()) {
		getRenderer().showVFB();
	}
	else {
		Log::getLog().warning("Can't show VFB - no render or no UI.");
	}
}


void MotionBlurParams::calcParams(fpreal currFrame)
{
	mb_start = currFrame - (mb_duration * (0.5 - mb_interval_center));
	mb_end   = mb_start + mb_duration;
	mb_frame_inc = mb_duration / VUtils::Max(mb_geom_samples - 1, 1);

	Log::getLog().info("  MB time: %.3f", currFrame);
	Log::getLog().info("  MB duration: %.3f", mb_duration);
	Log::getLog().info("  MB interval center: %.3f", mb_interval_center);
	Log::getLog().info("  MB geom samples: %i", mb_geom_samples);
	Log::getLog().info("  MB start: %.3f", mb_start);
	Log::getLog().info("  MB end:   %.3f", mb_end);
	Log::getLog().info("  MB inc:   %.3f", mb_frame_inc);
}

void VRayExporter::setTime(fpreal time)
{
	m_context.setTime(time);
	getRenderer().getVRay().setCurrentFrame(m_context.getFloatFrame());

	Log::getLog().debug("Time:  %g", m_context.getTime());
	Log::getLog().debug("Frame: %g", m_context.getFloatFrame());
	Log::getLog().debug("V-Ray time:  %g", getRenderer().getVRay().getCurrentTime());
	Log::getLog().debug("V-Ray frame: %i", getRenderer().getVRay().getCurrentFrame());
}

void VRayExporter::applyTake(const char *takeName)
{
	// Houdini handles this automatically for production rendering,
	// since we've inherited from ROP_Node.
	if (sessionType == VfhSessionType::production)
		return;

	UT_String toTakeName(takeName);

	if (m_rop && CAST_ROPNODE(m_rop)) {
		VRayRendererNode &vrayROP = *static_cast<VRayRendererNode*>(m_rop);
		if (!toTakeName.isstring())
			vrayROP.evalString(toTakeName, "take", 0, 0.0f);
	}

	if (!toTakeName.isstring())
		return;

	OP_Take *takeMan = OPgetDirector()->getTakeManager();
	if (takeMan) {
		if (!currentTake) {
			currentTake = takeMan->getCurrentTake();
		}
		TAKE_Take *toTake = takeMan->findTake(toTakeName.buffer());
		if (toTake) {
			takeMan->switchToTake(toTake);
		}
	}
}

void VRayExporter::restoreCurrentTake()
{
	// Houdini handles this automatically for production rendering,
	// since we've inherited from ROP_Node.
	if (sessionType == VfhSessionType::production)
		return;
	if (!currentTake)
		return;

	OP_Take *takeMan = OPgetDirector()->getTakeManager();
	if (takeMan) {
		takeMan->takeRestoreCurrent(currentTake);
	}

	currentTake = nullptr;
}

void VRayExporter::exportFrame(fpreal time)
{
	QAtomicIntRaii inSceneLock(inSceneExport);

	Log::getLog().debug("VRayExporter::exportFrame(time = %.3f)", time);

	if (isAborted()) {
		Log::getLog().info("Operation is aborted by the user!");
		m_error = ROP_ABORT_RENDER;
		return;
	}

	applyTake();

	// Must go before setTime() for correct SettingsOutput parameters.
	exportSettings();

	setTime(time);

	m_context.hasMotionBlur = m_isMotionBlur || m_isVelocityOn;

	if (!m_context.hasMotionBlur) {
		clearKeyFrames(getContext().getFloatFrame());
		exportScene();
	}
	else {
		MotionBlurParams &mbParams = m_context.mbParams;
		fillMotionBlurParams(mbParams);
		mbParams.calcParams(m_context.getFloatFrame());

		// We don't need this data anymore
		clearKeyFrames(mbParams.mb_start);

		for (FloatSet::iterator tIt = m_exportedFrames.begin(); tIt != m_exportedFrames.end();) {
			if (*tIt < mbParams.mb_start) {
				m_exportedFrames.erase(tIt++);
			}
			else {
				++tIt;
			}
		}

		// Export motion blur data
		fpreal subframe = mbParams.mb_start;
		while (!isAborted() && subframe <= mbParams.mb_end) {
			const fpreal mbFrame = subframe >= 0.0 ? subframe : 0.0;

			if (!m_exportedFrames.contains(mbFrame)) {
				m_exportedFrames.insert(mbFrame);

				OP_Context mbTime;
				mbTime.setFrame(mbFrame);

				setTime(mbTime.getTime());

				exportScene();
			}

			subframe += mbParams.mb_frame_inc;
		}

		// Set time back to original time for rendering
		setTime(time);
	}

	if (isAborted()) {
		Log::getLog().info("Operation is aborted by the user!");
		m_error = ROP_ABORT_RENDER;
	}
	else if (sessionType != VfhSessionType::cloud) {
		renderFrame(!isInteractive());
	}
}

static void fillJobSettingsFromROP(OP_Node &rop, Cloud::Job &job)
{
	UT_String projectName;
	rop.evalString(projectName, "cloud_project_name", 0, 0.0);

	UT_String jobName;
	rop.evalString(jobName, "cloud_job_name", 0, 0.0);

	job.setProject(projectName.buffer());
	job.setName(jobName.buffer());
}

void VRayExporter::exportEnd()
{
	Log::getLog().debug("VRayExporter::exportEnd()");

	if (sessionType == VfhSessionType::cloud) {
		if (m_error != ROP_ABORT_RENDER) {
			const QString &jobSceneFilePath = JobFilePath::createFilePath();
			if (!jobSceneFilePath.isEmpty()) {
				VRay::VRayExportSettings expSettings;
				expSettings.hexArrays = true;
				expSettings.hexTransforms = true;
				expSettings.compressed = true;

				if (exportVrscene(jobSceneFilePath, expSettings)) {
					JobFilePath::removeFilePath(jobSceneFilePath);
				}
				else {
					Cloud::Job job(jobSceneFilePath);

					fillJobSettingsFromROP(*m_rop, job);

					job.width = m_viewParams.renderSize.w;
					job.height = m_viewParams.renderSize.h;
					job.animation = m_isAnimation;
					job.frameRange = Cloud::Job::FrameRange(animInfo.frameStart, animInfo.frameEnd);
					job.frameStep = animInfo.frameStep;

					Cloud::submitJob(job);
				}
			}
		}
	}
	else if (sessionType == VfhSessionType::production &&
	         (!exportFilePerFrame && isAnimation()) &&
	         (m_workMode == ExpExport ||
	          m_workMode == ExpExportRender))
	{
		// Full animation export.
		exportToVrscene(false);
	}

	clearKeyFrames(SYS_FP64_MAX);
	reset();

	m_error = ROP_CONTINUE_RENDER;
}

const char* VRayForHoudini::getVRayPluginIDName(VRayPluginID pluginID)
{
	static const char* pluginIDNames[static_cast<std::underlying_type<VRayPluginID>::type>(VRayPluginID::MAX_PLUGINID)] = {
		"SunLight",
		"LightDirect",
		"LightAmbient",
		"LightOmni",
		"LightSphere",
		"LightSpot",
		"LightRectangle",
		"LightMesh",
		"LightIES",
		"LightDome",
		"VRayClipper"
	};

	return (pluginID < VRayPluginID::MAX_PLUGINID) ? pluginIDNames[static_cast<std::underlying_type<VRayPluginID>::type>(pluginID)] : nullptr;
}

enum class VfhRenderModeMenu {
	cpu = 0,
	cuda,
	opencl,
};

VRay::VRayRenderer::RenderMode VRayForHoudini::getRendererMode(const OP_Node &rop)
{
	const VfhRenderModeMenu renderMode =
		static_cast<VfhRenderModeMenu>(rop.evalInt("render_render_mode", 0, 0.0));

	switch (renderMode) {
		case VfhRenderModeMenu::cpu:    return VRay::VRayRenderer::RENDER_MODE_PRODUCTION;
		case VfhRenderModeMenu::cuda:   return VRay::VRayRenderer::RENDER_MODE_PRODUCTION_CUDA;
		case VfhRenderModeMenu::opencl: return VRay::VRayRenderer::RENDER_MODE_PRODUCTION_OPENCL;
	}

	vassert(false && "VRayForHoudini::getRendererMode(): Incorrect \"render_render_mode\" value!");

	return VRay::VRayRenderer::RENDER_MODE_PRODUCTION;
}

VRay::VRayRenderer::RenderMode VRayForHoudini::getRendererIprMode(const OP_Node &rop)
{
	const VfhRenderModeMenu renderMode =
		static_cast<VfhRenderModeMenu>(rop.evalInt("render_rt_mode", 0, 0.0));

	switch (renderMode) {
		case VfhRenderModeMenu::cpu:    return VRay::VRayRenderer::RENDER_MODE_INTERACTIVE;
		case VfhRenderModeMenu::cuda:   return VRay::VRayRenderer::RENDER_MODE_INTERACTIVE_CUDA;
		case VfhRenderModeMenu::opencl: return VRay::VRayRenderer::RENDER_MODE_INTERACTIVE_OPENCL;
	}

	vassert(false && "VRayForHoudini::getRendererIprMode(): Incorrect \"render_rt_mode\" value!");

	return VRay::VRayRenderer::RENDER_MODE_PRODUCTION;
}

VRayExporter::ExpWorkMode VRayForHoudini::getExportMode(const OP_Node &rop)
{
	if (!Parm::isParmExist(rop, "render_export_mode"))
		return VRayExporter::ExpRender;
	return static_cast<VRayExporter::ExpWorkMode>(rop.evalInt("render_export_mode", 0, 0.0));
}

int VRayForHoudini::isBackground()
{
	return !HOU::isUIAvailable();
}

int VRayForHoudini::getFrameBufferType(OP_Node &rop)
{
	return isBackground() ? 0 : 1;
}

void VRayExporter::saveVfbState()
{
	if (!m_rop)
		return;

	QString buf;
	getRenderer().saveVfbState(buf);

	PRM_Parm *vfbSettingsParm = m_rop->getParmPtr("_vfb_settings");
	if (vfbSettingsParm) {
		vfbSettingsParm->setValue(0.0, qPrintable(buf), CH_STRING_LITERAL);
	}
}

void VRayExporter::restoreVfbState(int setHistory)
{
	if (!m_rop)
		return;

	PRM_Parm *vfbSettingsParm = m_rop->getParmPtr("_vfb_settings");
	if (!vfbSettingsParm)
		return;

	UT_String vfbState;
	m_rop->evalString(vfbState, "_vfb_settings", 0, 0.0);

	if (vfbState.isstring()) {
		getRenderer().restoreVfbState(vfbState.buffer(), setHistory);
	}
}

void VRayExporter::renderLast() const
{
	// We should not use this for IPR.
	vassert(sessionType != VfhSessionType::ipr);

	if (!m_rop)
		return;

	VRayRendererNode &vrayRop = static_cast<VRayRendererNode&>(*m_rop);
	vrayRop.renderLast();
}

void VRayExporter::VfhBundleMap::MyBundle::freeMem()
{
	for (int i = 0; i < opNamesCount; ++i) {
		FreePtrArr(opNames[i]);
	}
	FreePtrArr(opNames);
}

VRayExporter::VfhBundleMap::~VfhBundleMap()
{
	freeMem();
}

void VRayExporter::VfhBundleMap::init()
{
	freeMem();

	OP_BundleList *opBundles = OPgetDirector()->getBundles();
	if (!opBundles)
		return;

	for (int bundleIdx = 0; bundleIdx < opBundles->entries(); ++bundleIdx) {
		OP_Bundle &opBundle = *opBundles->getBundle(bundleIdx);

		UT_ValArray<OP_Node*> opList;
		opBundle.getMembers(opList);

		const int numBundleOps = opList.entries();
		if (numBundleOps) {
			MyBundle bundle;
			bundle.name = opBundle.getName();
			bundle.opNamesCount = numBundleOps;
			bundle.opNames = new char * [numBundleOps];

			for (int opIdx = 0; opIdx < numBundleOps; ++opIdx) {
				const OP_Node &bundleNode = *opList(opIdx);

				const int pathLen = bundleNode.getFullPath().length()+1;
				bundle.opNames[opIdx] = new char[pathLen];

				vutils_strcpy_n(bundle.opNames[opIdx], bundleNode.getFullPath().buffer(), pathLen);
			}

			bundles.append(bundle);
		}
	}

	for (const MyBundle &bundle : bundles) {
		bundleMap.add(bundle.name, bundle.opNames, bundle.opNamesCount);
	}
}

void VRayExporter::VfhBundleMap::freeMem()
{
	bundleMap = GSTY_BundleMap();

	for (MyBundle &bundle : bundles) {
		bundle.freeMem();
	}

	bundles.clear();
}

struct ExportLightLinker
{
	enum LightListType {
		/// The list is an exclude list (default).
		lightListTypeExclude = 0,

		/// The list is an include list.
		lightListTypeInclude = 1,

		/// List is ignored and the light illuminates
		/// (or casts shadows from) all objects in the
		/// scene, useful for animating the light linking.
		lightListTypeIgnore = 2,
	};

	struct LightLinkerEntry {
		/// Ligth plugin.
		VRay::Plugin light;

		/// Include / exclude list of Node plugins.
		PluginList nodes;
	};

	typedef QList<LightLinkerEntry> LightLinkerEntries;

	ExportLightLinker(VRayExporter &self, const OpCacheMan &cacheMan)
		: self(self)
		, cacheMan(cacheMan)
	{}

	static void lightLinkerEntriesToValueLists(const LightLinkerEntries &entries,
	                                           VRay::VUtils::ValueRefList &lightEntries,
	                                           VRay::VUtils::IntRefList &lightEntriesFlags)
	{
		const int numEntries = entries.size();
		if (!numEntries)
			return;

		lightEntries = VRay::VUtils::ValueRefList(numEntries);
		lightEntriesFlags = VRay::VUtils::IntRefList(numEntries);

		FOR_CONST_IT (LightLinkerEntries, it, entries) {
			const LightLinkerEntry &entry = *it;

			// +1 because first element is light itself.
			VRay::VUtils::ValueRefList entryItem(entry.nodes.size()+1);
			entryItem[0].setPlugin(entry.light);

			FOR_CONST_IT (PluginList, pIt, entry.nodes) {
				entryItem[pItIdx+1].setPlugin(*pIt);
			}

			lightEntries[itIdx].setList(entryItem);
			lightEntriesFlags[itIdx] = lightListTypeInclude;
		}
	}

	void fillShadowPluginList(const OBJ_Light &objLight, PluginList &shadowList) const
	{
		UT_String shadowmask;
		objLight.evalString(shadowmask, "shadowmask", 0, 0.0);
		if (shadowmask.equal("*"))
			return;

		OP_Bundle *shadowMaskBundle = const_cast<OBJ_Light&>(objLight).getShadowMaskBundle(0.0);
		if (!shadowMaskBundle)
			return;

		OP_NodeList castShadowList;
		shadowMaskBundle->getMembers(castShadowList);

		for (OP_Node *node : castShadowList) {
			OBJ_Node *objNodeShadow = node->castToOBJNode();
			if (objNodeShadow) {
				const ObjCacheEntry &objEntry = cacheMan.getObjEntry(*objNodeShadow);

				mergePluginList(shadowList, objEntry.nodes);
				mergePluginList(shadowList, objEntry.volumes);
			}
		}
	}

	void exportPlugin() const
	{
		const ObjLightPluginsCache &objLightToLightPlugins = cacheMan.getLightPlugins();
		if (objLightToLightPlugins.isEmpty())
			return;

		LightLinkerEntries ignoredLights;
		LightLinkerEntries ignoredShadowLights;

		FOR_CONST_IT (ObjLightPluginsCache, it, objLightToLightPlugins) {
			const OBJ_Light &objLight = *it.key();
			const ObjLightCacheEntry &lightEntry = it.value();

			// Generate shadow include list.
			PluginList shadowList;
			fillShadowPluginList(objLight, shadowList);

			// Create an entry for every light plugin.
			if (!lightEntry.includeNodes.empty() ||
			    !shadowList.empty())
			{
				for (const VRay::Plugin &light : lightEntry.lights) {
					if (!lightEntry.includeNodes.empty()) {
						ignoredLights.append({light, lightEntry.includeNodes});
					}
					if (!shadowList.empty()) {
						ignoredShadowLights.append({light, shadowList});
					}
				}
			}
		}

		VRay::VUtils::ValueRefList lightList;
		VRay::VUtils::IntRefList lightInclusivityList;
		lightLinkerEntriesToValueLists(ignoredLights, lightList, lightInclusivityList);

		VRay::VUtils::ValueRefList shadowList;
		VRay::VUtils::IntRefList shadowInclusivityList;
		lightLinkerEntriesToValueLists(ignoredShadowLights, shadowList, shadowInclusivityList);

		Attrs::PluginDesc lightLinker(SL("settingsLightLinker"),
		                              SL("SettingsLightLinker"));
		lightLinker.add(Attrs::PluginAttr(SL("ignored_lights"), lightList));
		lightLinker.add(Attrs::PluginAttr(SL("ignored_shadow_lights"), shadowList));
		lightLinker.add(Attrs::PluginAttr(SL("include_exclude_light_flags"), lightInclusivityList));
		lightLinker.add(Attrs::PluginAttr(SL("include_exclude_shadow_flags"), shadowInclusivityList));

		self.exportPlugin(lightLinker);
	}

private:
	VRayExporter &self;
	const OpCacheMan &cacheMan;
};

void VRayExporter::exportLightLinker()
{
	ExportLightLinker exportLightLinker(*this, cacheMan);
	exportLightLinker.exportPlugin();
}
