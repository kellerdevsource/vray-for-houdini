//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <QDir>
#include <QRegExp>

#include "vfh_defines.h"
#include "vfh_exporter.h"
#include "vfh_prm_globals.h"
#include "vfh_prm_templates.h"
#include "vfh_tex_utils.h"
#include "vfh_hou_utils.h"
#include "vfh_attr_utils.h"
#include "vfh_log.h"

#include "obj/obj_node_base.h"
#include "vop/vop_node_base.h"
#include "vop/material/vop_mtl_def.h"
#include "vop/material/vop_PhoenixSim.h"
#include "sop/sop_vrayproxy.h"
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

#include <OBJ/OBJ_Camera.h>
#include <OBJ/OBJ_Geometry.h>
#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_Light.h>
#include <OBJ/OBJ_SubNet.h>
#include <OP/OP_Director.h>

#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#include "vfh_export_geom.h"
#include "vfh_op_utils.h"

using namespace VRayForHoudini;

/// Directory hierarchy creator.
/// Using static variable, because QDir::mkpath is not static.
static QDir directoryCreator;

static boost::format FmtPluginNameWithPrefix("%s@%s");

/// Type converter name template: "TexColorToFloat@<CurrentPluginName>|<ParameterName>"
static boost::format fmtPluginTypeConverterName("%s@%s|%s");

/// Type converter name template: "<CurrentPluginName>|<ParameterName>"
static boost::format fmtPluginTypeConverterName2("%s|%s");

static StringSet RenderSettingsPlugins;

static UT_DMatrix4 yAxisUpRotationMatrix(1.0, 0.0, 0.0, 0.0,
                                         0.0, 0.0, 1.0, 0.0,
                                         0.0, -1.0, 0.0, 0.0,
                                         0.0, 0.0, 0.0, 0.0);

static const VRay::Transform envMatrix(VRay::Matrix(VRay::Vector(1.f, 0.f, 0.f),
                                                    VRay::Vector(0.f, 0.f, 1.f),
                                                    VRay::Vector(0.f, -1.f, 0.f)),
                                       VRay::Vector(0.f));

static QRegExp frameMatch("\\$[.\\d]*F");

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
	self.interactive = Parm::getParmInt(ropNode, "SettingsRTEngine_interactive");
	self.low_gpu_thread_priority = Parm::getParmInt(ropNode, "SettingsRTEngine_low_gpu_thread_priority");
	self.max_draw_interval = Parm::getParmInt(ropNode, "SettingsRTEngine_max_draw_interval");
	self.max_render_time = Parm::getParmInt(ropNode, "SettingsRTEngine_max_render_time");
	self.max_sample_level = Parm::getParmInt(ropNode, "SettingsRTEngine_max_sample_level");
	self.min_draw_interval = Parm::getParmInt(ropNode, "SettingsRTEngine_min_draw_interval");
	self.noise_threshold = Parm::getParmInt(ropNode, "SettingsRTEngine_noise_threshold");
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
static void setSettingsRTEnginetOptimizedGpuSettings(SettingsRTEngine &self, const OP_Node &ropNode, VRay::RendererOptions::RenderMode mode)
{
	if (!Parm::getParmInt(ropNode, "SettingsRTEngine_auto"))
		return;

	// CPU/GPU RT/IPR.
	if (mode >= VRay::RendererOptions::RENDER_MODE_RT_CPU &&
        mode <= VRay::RendererOptions::RENDER_MODE_RT_GPU)
    {
		self.cpu_samples_per_pixel = 1;
		self.cpu_bundle_size = 64;
		self.gpu_samples_per_pixel = 1;
		self.gpu_bundle_size = 128;
		self.undersampling = 0;
		self.progressive_samples_per_pixel = 0;
    }
	// GPU Production.
	else if (mode >= VRay::RendererOptions::RENDER_MODE_PRODUCTION_OPENCL &&
			 mode <= VRay::RendererOptions::RENDER_MODE_PRODUCTION_CUDA)
	{
		self.gpu_samples_per_pixel = 16;
		self.gpu_bundle_size = 256;
		self.undersampling = 0;
		self.progressive_samples_per_pixel = 0;
	}
}

void VRayExporter::reset()
{
	objectExporter.clearPrimPluginCache();
	objectExporter.clearOpDepPluginCache();
	objectExporter.clearOpPluginCache();

	resetOpCallbacks();
	restoreCurrentTake();

	m_renderer.reset();
}

std::string VRayExporter::getPluginName(const OP_Node &opNode, const char *prefix)
{
	std::string pluginName = boost::str(FmtPluginNameWithPrefix % prefix % opNode.getFullPath().buffer());

	// AppSDK doesn't like "/" for some reason.
	boost::replace_all(pluginName, "/", "|");
	if (boost::ends_with(pluginName, "|")) {
		pluginName.pop_back();
	}

	return pluginName;
}


std::string VRayExporter::getPluginName(OP_Node *op_node, const std::string &prefix, const std::string &suffix)
{
	static boost::format FmtPlugin("%s@%s|%s");

	const std::string &pluginName = boost::str(FmtPlugin
											   % prefix
											   % op_node->getFullPath().buffer()
											   % suffix);

	return pluginName;
}


std::string VRayExporter::getPluginName(OBJ_Node *obj_node)
{
	std::string pluginName;

	const OBJ_OBJECT_TYPE ob_type = obj_node->getObjectType();
	if (ob_type & OBJ_LIGHT) {
		static boost::format FmtLight("Light@%s");
		pluginName = boost::str(FmtLight
								% obj_node->getFullPath().buffer());
	}
	else if (ob_type & OBJ_CAMERA) {
		static boost::format FmtCamera("Camera@%s");
		pluginName = boost::str(FmtCamera
								% obj_node->getFullPath().buffer());
	}
	else if (ob_type == OBJ_GEOMETRY) {
		static boost::format FmtObject("Node@%s");
		pluginName = boost::str(FmtObject
								% obj_node->getFullPath().buffer());
	}

	return pluginName;
}


std::string VRayExporter::getPluginName(OBJ_Node &objNode)
{
	return VRayExporter::getPluginName(&objNode);
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
	OBJ_Node *camera = nullptr;

	UT_String camera_path;
	rop->evalString(camera_path, "render_camera", 0, 0.0);
	if (NOT(camera_path.equal(""))) {
		OP_Node *node = getOpNodeFromPath(camera_path);
		if (node) {
			camera = node->castToOBJNode();
		}
	}

	return camera;
}


OP_Node* VRayExporter::FindChildNodeByType(OP_Node *op_node, const std::string &op_type)
{
	OP_NodeList childNodes;
	op_node->getAllChildren(childNodes);

	for (auto childIt : childNodes) {
		const UT_String &opType = childIt->getOperator()->getName();
		if (opType.equal(op_type.c_str())) {
			return childIt;
		}
	}

	return nullptr;
}


void VRayExporter::setAttrValueFromOpNodePrm(Attrs::PluginDesc &pluginDesc, const Parm::AttrDesc &attrDesc, OP_Node &opNode, const std::string &parmName) const
{
	if (Parm::isParmExist(opNode, parmName)) {
		const PRM_Parm *parm = Parm::getParm(opNode, parmName);
		if (parm->getParmOwner()->isPendingOverride()) {
			Log::getLog().debug("Pending override: %s %s",
								opNode.getName().buffer(), parmName.c_str());
		}

		const fpreal t = m_context.getTime();

		Attrs::PluginAttr attr;
		attr.paramName = attrDesc.attr.ptr();

		if (attrDesc.value.type == Parm::eBool ||
			attrDesc.value.type == Parm::eInt  ||
			attrDesc.value.type == Parm::eTextureInt)
		{
			attr.paramType = Attrs::PluginAttr::AttrTypeInt;
			attr.paramValue.valInt = opNode.evalInt(parmName.c_str(), 0, t);
		}
		else if (attrDesc.value.type == Parm::eEnum) {
			UT_String enumValue;
			opNode.evalString(enumValue, parmName.c_str(), 0, t);

			if (enumValue.isInteger()) {
				attr.paramType = Attrs::PluginAttr::AttrTypeInt;
				attr.paramValue.valInt = enumValue.toInt();
			}
			else if (pluginDesc.pluginID == "UVWGenEnvironment") {
				// UVWGenEnvironment is the only plugin with enum with the string keys.
				attr.paramType = Attrs::PluginAttr::AttrTypeString;
				attr.paramValue.valString = enumValue.buffer();
			}
			else {
				Log::getLog().error("Incorrect enum: %s.%s!",
									pluginDesc.pluginID.c_str(),
									attrDesc.attr.ptr());
			}
		}
		else if (attrDesc.value.type == Parm::eFloat ||
				 attrDesc.value.type == Parm::eTextureFloat) {
			attr.paramType = Attrs::PluginAttr::AttrTypeFloat;
			attr.paramValue.valFloat = (float)opNode.evalFloat(parmName.c_str(), 0, t);

			if (attrDesc.flags & Parm::attrFlagToRadians) {
				attr.paramValue.valFloat *= Attrs::RAD_TO_DEG;
			}
		}
		else if (attrDesc.value.type == Parm::eColor  ||
				 attrDesc.value.type == Parm::eAColor ||
				 attrDesc.value.type == Parm::eTextureColor)
		{
			const PRM_Parm *parm = Parm::getParm(opNode, parmName);
			if (parm && parm->getType().isFloatType()) {
				attr.paramType = Attrs::PluginAttr::AttrTypeColor;
				attr.paramValue.valVector[0] = (float)opNode.evalFloat(parmName.c_str(), 0, t);
				attr.paramValue.valVector[1] = (float)opNode.evalFloat(parmName.c_str(), 1, t);
				attr.paramValue.valVector[2] = (float)opNode.evalFloat(parmName.c_str(), 2, t);
				if (attrDesc.value.type != Parm::eColor) {
					attr.paramValue.valVector[3] = (float)opNode.evalFloat(parmName.c_str(), 3, t);
				}
			}
		}
		else if (attrDesc.value.type == Parm::eVector) {
			const PRM_Parm *parm = Parm::getParm(opNode, parmName);
			if (parm && parm->getType().isFloatType()) {
				attr.paramType = Attrs::PluginAttr::AttrTypeVector;
				attr.paramValue.valVector[0] = (float)opNode.evalFloat(parmName.c_str(), 0, t);
				attr.paramValue.valVector[1] = (float)opNode.evalFloat(parmName.c_str(), 1, t);
				attr.paramValue.valVector[2] = (float)opNode.evalFloat(parmName.c_str(), 2, t);
			}
		}
		else if (attrDesc.value.type == Parm::eString) {
			UT_String buf;
			opNode.evalString(buf, parmName.c_str(), 0, t);

			attr.paramType = Attrs::PluginAttr::AttrTypeString;
			attr.paramValue.valString = buf.buffer();
		}
		else if (attrDesc.value.type > Parm::eManualExportStart && attrDesc.value.type < Parm::eManualExportEnd) {
			// These are fake params and must be handled manually
		}
		else if (attrDesc.value.type < Parm::ePlugin) {
			Log::getLog().error("Unhandled param type: %s at %s [%i]",
								parmName.c_str(), opNode.getOperator()->getName().buffer(), attrDesc.value.type);
		}

		pluginDesc.addAttribute(attr);

	}
}


VRay::Transform VRayExporter::exportTransformVop(VOP_Node &vop_node, ExportContext *parentContext, bool rotate)
{
	const fpreal t = getContext().getTime();

	OP_Options options;
	for (int i = 0; i < vop_node.getParmList()->getEntries(); ++i) {
		const PRM_Parm &prm = vop_node.getParm(i);
		options.setOptionFromTemplate(&vop_node, prm, *prm.getTemplatePtr(), t);
	}

	UT_DMatrix4 m4;
	OP_Node::buildXform(options.getOptionI("trs"),
						options.getOptionI("xyz"),
						options.getOptionV3("trans").x(), options.getOptionV3("trans").y(), options.getOptionV3("trans").z(),
						options.getOptionV3("rot").x(), options.getOptionV3("rot").y(), options.getOptionV3("rot").z(),
						options.getOptionV3("scale").x(), options.getOptionV3("scale").y(), options.getOptionV3("scale").z(),
						options.getOptionV3("pivot").x(), options.getOptionV3("pivot").y(), options.getOptionV3("pivot").z(),
						m4);
	if (rotate) {
		m4 = m4 * yAxisUpRotationMatrix;
	}

	return Matrix4ToTransform(m4);
}

struct ConnectedPluginInfo {
	explicit ConnectedPluginInfo(VRay::Plugin plugin=VRay::Plugin(), const std::string &output="")
		: plugin(plugin)
		, output(output)
	{}

	/// Connected plugin.
	VRay::Plugin plugin;

	/// Connected output. May be empty.
	std::string output;
};

/// Sets attribute plugin value to a specific output based on ConnectedPluginInfo.
/// @param pluginDesc Plugin description to add parameter on.
/// @param attrName Attribute name.
/// @param conPluginInfo Connected plugin info.
static void setPluginValueFromConnectedPluginInfo(Attrs::PluginDesc &pluginDesc, const char *attrName, const ConnectedPluginInfo &conPluginInfo)
{
	if (!conPluginInfo.plugin)
		return;

	if (!conPluginInfo.output.empty()) {
		pluginDesc.addAttribute(Attrs::PluginAttr(attrName, conPluginInfo.plugin, conPluginInfo.output));
	}
	else {
		pluginDesc.addAttribute(Attrs::PluginAttr(attrName, conPluginInfo.plugin));
	}
}

static int isConnectedToTexTriplanar(VOP_Node &vopNode)
{
	OP_NodeList outputs;
	vopNode.getOutputNodes(outputs);

	for (OP_Node *opNode : outputs) {
		if (isOpType(*opNode, "VRayNodeTexTriPlanar")) {
			return true;
		}
	}

	return false;
}

void VRayExporter::setAttrsFromOpNodeConnectedInputs(Attrs::PluginDesc &pluginDesc, VOP_Node *vopNode, ExportContext *parentContext)
{
	const Parm::VRayPluginInfo *pluginInfo = Parm::getVRayPluginInfo(pluginDesc.pluginID.c_str());
	if (!pluginInfo) {
		Log::getLog().error("Node \"%s\": Plugin \"%s\" description is not found!",
							vopNode->getName().buffer(), pluginDesc.pluginID.c_str());
		return;
	}

	OP::VRayNode *vrayNode = dynamic_cast<OP::VRayNode*>(vopNode);
	vassert(vrayNode);

	for (int i = 0; i < pluginInfo->inputs.count(); ++i) {
		const Parm::SocketDesc &curSockInfo = pluginInfo->inputs[i];

		const VUtils::CharString &attrName = curSockInfo.attrName.ptr();

		if (!pluginInfo->hasAttribute(attrName.ptr()) ||
			pluginDesc.contains(attrName.ptr()))
		{
			continue;
		}

		const Parm::AttrDesc &attrDesc = pluginInfo->getAttribute(attrName.ptr());
		if (attrDesc.flags & Parm::attrFlagCustomHandling) {
			continue;
		}

		VRay::Plugin conPlugin = exportConnectedVop(vopNode, attrName.ptr(), parentContext);
		if (!conPlugin) {
			if (!(attrDesc.flags & Parm::attrFlagLinkedOnly) &&
				vrayNode->getVRayPluginType() == VRayPluginType::TEXTURE  &&
				attrName == "uvwgen")
			{
				if (!isConnectedToTexTriplanar(*vopNode)) {
					Attrs::PluginDesc defaultUVWGen(getPluginName(vopNode, "DefaultUVWGen"),
					                                "UVWGenProjection");
					defaultUVWGen.add(Attrs::PluginAttr("type", 6));
					defaultUVWGen.add(Attrs::PluginAttr("object_space", true));

					conPlugin = exportPlugin(defaultUVWGen);
				}
			}
			else {
				const unsigned inpidx = vopNode->getInputFromName(attrName.ptr());
				VOP_Node *inpvop = vopNode->findSimpleInput(inpidx);
				if (inpvop) {
					if (inpvop->getOperator()->getName() == "makexform") {
						switch (curSockInfo.attrType) {
							case Parm::eMatrix: {
								const bool shouldRotate = pluginDesc.pluginID == "UVWGenPlanar" ||
								                          pluginDesc.pluginID == "UVWGenProjection" ||
								                          pluginDesc.pluginID == "UVWGenObject" ||
								                          pluginDesc.pluginID == "UVWGenEnvironment";

								const VRay::Transform &transform = exportTransformVop(*inpvop, parentContext, shouldRotate);
								pluginDesc.addAttribute(Attrs::PluginAttr(attrName.ptr(), transform.matrix));
								break;
							}
							case Parm::eTransform: {
								pluginDesc.addAttribute(Attrs::PluginAttr(attrName.ptr(), exportTransformVop(*inpvop, parentContext)));
								break;
							}
							default:
								break;
						}
					}
				}
			}
		}

		if (conPlugin) {
			ConnectedPluginInfo conPluginInfo(conPlugin);

			const Parm::SocketDesc *fromSocketInfo = getConnectedOutputType(vopNode, attrName.ptr());
			if (fromSocketInfo) {
				// Check if we need to auto-convert color / float.
				std::string floatColorConverterType;

				// Check if some specific output was connected.
				conPluginInfo.output = fromSocketInfo->attrName.ptr();

				// If connected plugin type is BRDF, but we expect a Material, wrap it into "MtlSingleBRDF".
				if (fromSocketInfo->socketType == VOP_TYPE_BSDF &&
					curSockInfo.socketType     == VOP_SURFACE_SHADER)
				{
					const std::string &convName = str(fmtPluginTypeConverterName2
													  % pluginDesc.pluginName
													  % attrName.ptr());

					Attrs::PluginDesc brdfToMtl(convName, "MtlSingleBRDF");
					brdfToMtl.addAttribute(Attrs::PluginAttr("brdf", conPluginInfo.plugin));

					conPluginInfo.plugin = exportPlugin(brdfToMtl);
					conPluginInfo.output.clear();
				}
				else if (fromSocketInfo->socketType == VOP_TYPE_COLOR &&
						 curSockInfo.socketType     == VOP_TYPE_FLOAT)
				{
					floatColorConverterType = "TexColorToFloat";
				}
				else if (fromSocketInfo->socketType == VOP_TYPE_FLOAT &&
						 curSockInfo.socketType     == VOP_TYPE_COLOR)
				{
					floatColorConverterType = "TexFloatToColor";
				}

				if (!floatColorConverterType.empty()) {
					const std::string &convName = str(fmtPluginTypeConverterName
														% pluginDesc.pluginName
														% floatColorConverterType
														% attrName.ptr());

					Attrs::PluginDesc convDesc(convName, floatColorConverterType);
					setPluginValueFromConnectedPluginInfo(convDesc, "input", conPluginInfo);

					conPluginInfo.plugin = exportPlugin(convDesc);

					// We've stored the original connected output in the "input" of the converter.
					conPluginInfo.output.clear();
				}
			}

			// Set "scene_name" for Cryptomatte.
			if (vutils_strcmp(conPlugin.getType(), "MtlSingleBRDF") == 0) {
				VRay::ValueList sceneName(1);
				sceneName[0] = VRay::Value(vopNode->getName().buffer());
				conPlugin.setValue("scene_name", sceneName);
			}

			setPluginValueFromConnectedPluginInfo(pluginDesc, attrName.ptr(), conPluginInfo);
		}
	}
}


void VRayExporter::setAttrsFromOpNodePrms(Attrs::PluginDesc &pluginDesc, OP_Node *opNode, const std::string &prefix, bool remapInterp)
{
	const Parm::VRayPluginInfo *pluginInfo = Parm::getVRayPluginInfo(pluginDesc.pluginID.c_str());
	if (NOT(pluginInfo)) {
		Log::getLog().error("Node \"%s\": Plugin \"%s\" description is not found!",
							opNode->getName().buffer(), pluginDesc.pluginID.c_str());
	}
	else {
		FOR_CONST_IT (Parm::AttributeDescs, aIt, pluginInfo->attributes) {
			const std::string    &attrName = aIt.key();
			const Parm::AttrDesc &attrDesc = aIt.data();

			if (!(pluginDesc.contains(attrName) || attrDesc.flags & Parm::attrFlagCustomHandling)) {
				const std::string &parmName = prefix.empty()
											  ? attrDesc.attr.ptr()
											  : boost::str(Parm::FmtPrefixManual % prefix % attrDesc.attr.ptr());

				const PRM_Parm *parm = Parm::getParm(*opNode, parmName);

				// check for properties that are marked for custom handling on hou side
				if (parm) {
					auto spareData = parm->getSparePtr();
					if (spareData && spareData->getValue("vray_custom_handling")) {
						continue;
					}
				}

				const bool isTextureAttr = attrDesc.value.type >= Parm::eTextureColor &&
				                           attrDesc.value.type <= Parm::eTextureTransform;

				if ( isTextureAttr
					&& parm
					&& parm->getType().isStringType())
				{
					UT_String opPath;
					opNode->evalString(opPath, parm->getToken(), 0, 0.0f);

					const VRay::Plugin opPlugin = exportNodeFromPath(opPath);
					if (opPlugin) {
						pluginDesc.addAttribute(Attrs::PluginAttr(attrName, opPlugin));
					}
				}
				else if (!(attrDesc.flags & Parm::attrFlagLinkedOnly)) {
					if (attrDesc.value.type == Parm::eRamp) {
						static StringSet rampColorAsPluginList;
						if (rampColorAsPluginList.empty()) {
							rampColorAsPluginList.insert("PhxShaderSim");
						}

						// TODO: Move to attribute description
						const bool asColorList = rampColorAsPluginList.count(pluginDesc.pluginID);

						Texture::exportRampAttribute(*this, pluginDesc, opNode,
													 /* Houdini ramp attr */ parmName,
													 /* V-Ray attr: colors */ attrDesc.value.colorRampInfo.colors,
													 /* V-Ray attr: pos    */ attrDesc.value.colorRampInfo.positions,
													 /* V-Ray attr: interp */ attrDesc.value.colorRampInfo.interpolations,
													 /* As color list not plugin */ asColorList,
													 /* Remap to vray interpolations*/ remapInterp);

						pluginDesc.addAttribute(Attrs::PluginAttr(attrName, Attrs::PluginAttr::AttrTypeIgnore));
					}
					else if (attrDesc.value.type == Parm::eCurve) {

						VRay::IntList    interpolations;
						VRay::FloatList  positions;
						VRay::FloatList  values;
						VRay::FloatList *valuesPtr = attrDesc.value.curveRampInfo.values.empty()
													 ? nullptr
													 : &values;

						Texture::getCurveData(*this, opNode,
											  /* Houdini curve attr */ parmName,
											  /* V-Ray attr: interp */ interpolations,
											  /* V-Ray attr: x      */ positions,
											  /* V-Ray attr: y      */ valuesPtr,
											  /* Don't need handles */ false,
											  /* Remap to vray interpolations*/ remapInterp);

						pluginDesc.addAttribute(Attrs::PluginAttr(attrDesc.value.curveRampInfo.interpolations, interpolations));
						pluginDesc.addAttribute(Attrs::PluginAttr(attrDesc.value.curveRampInfo.positions,      positions));
						if (valuesPtr) {
							pluginDesc.addAttribute(Attrs::PluginAttr(attrDesc.value.curveRampInfo.values,     values));
						}
					}
					else {
						setAttrValueFromOpNodePrm(pluginDesc, attrDesc, *opNode, parmName);
					}
				}
			}
		}
	}
}


bool VRayExporter::setAttrsFromUTOptions(Attrs::PluginDesc &pluginDesc, const UT_Options &options) const
{
	bool res = false;

	const Parm::VRayPluginInfo *pluginInfo = Parm::getVRayPluginInfo(pluginDesc.pluginID.c_str());
	if (NOT(pluginInfo)) {
		return res;
	}

	FOR_CONST_IT (Parm::AttributeDescs, aIt, pluginInfo->attributes) {
		const std::string    &attrName = aIt.key();
		const Parm::AttrDesc &attrDesc = aIt.data();

		if (!options.hasOption(attrName) ||
			pluginDesc.contains(attrName))
		{
			continue;
		}

		Attrs::PluginAttr attr;
		attr.paramName = attrDesc.attr.ptr();

		if (   attrDesc.value.type == Parm::eBool
			|| attrDesc.value.type == Parm::eInt
			|| attrDesc.value.type == Parm::eTextureInt)
		{
			attr.paramType = Attrs::PluginAttr::AttrTypeInt;
			attr.paramValue.valInt = options.getOptionI(attrName);
		}
		else if (attrDesc.value.type == Parm::eEnum) {
			attr.paramType = Attrs::PluginAttr::AttrTypeInt;
			attr.paramValue.valInt = options.getOptionI(attrName);
		}
		else if (   attrDesc.value.type == Parm::eFloat
				 || attrDesc.value.type == Parm::eTextureFloat)
		{
			attr.paramType = Attrs::PluginAttr::AttrTypeFloat;
			attr.paramValue.valFloat = options.getOptionF(attrName);

			if (attrDesc.flags & Parm::attrFlagToRadians) {
				attr.paramValue.valFloat *= Attrs::RAD_TO_DEG;
			}
		}
		else if (attrDesc.value.type == Parm::eColor)
		{
			attr.paramType = Attrs::PluginAttr::AttrTypeColor;
			attr.paramValue.valVector[0] = options.getOptionV3(attrName)(0);
			attr.paramValue.valVector[1] = options.getOptionV3(attrName)(1);
			attr.paramValue.valVector[2] = options.getOptionV3(attrName)(2);
		}
		else if (   attrDesc.value.type == Parm::eAColor
				 || attrDesc.value.type == Parm::eTextureColor)
		{
			attr.paramType = Attrs::PluginAttr::AttrTypeColor;
			attr.paramValue.valVector[0] = options.getOptionV4(attrName)(0);
			attr.paramValue.valVector[1] = options.getOptionV4(attrName)(1);
			attr.paramValue.valVector[2] = options.getOptionV4(attrName)(2);
			attr.paramValue.valVector[3] = options.getOptionV4(attrName)(3);
		}
		else if (attrDesc.value.type == Parm::eString)
		{
			attr.paramType = Attrs::PluginAttr::AttrTypeString;
			attr.paramValue.valString = options.getOptionS(attrName);
		}

		if (attr.paramType != Attrs::PluginAttr::AttrTypeUnknown) {
			pluginDesc.addAttribute(attr);
			res = true;
		}
	}

	return res;
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
	imageFormatCount,
	imageFormatError
};

static const char* const imgFormatExt[imageFormatCount] = {
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
	for (int imgFormat = 0; imgFormat < static_cast<int>(imageFormatCount); ++imgFormat) {
		if (filePath.endsWith(imgFormatExt[imgFormat])) {
			return static_cast<ImageFormat>(imgFormat);
		}
	}
	
	return imageFormatError;
}

ReturnValue VRayExporter::fillSettingsOutput(Attrs::PluginDesc &pluginDesc)
{
	if (sessionType != VfhSessionType::production)
		return ReturnValue::Success;

	const fpreal t = getContext().getTime();
	OBJ_Node *camera = VRayExporter::getCamera(m_rop);

	if (!camera) {
		Log::getLog().error("Camera does not exist! In VrayExporter::fillSettingsOutput");
		return ReturnValue::Error;
	}

	fpreal pixelAspect = camera->evalFloat("aspect", 0, t);

	UT_String resfraction;
	m_rop->evalString(resfraction, "res_fraction", 0, t);
	if (   m_rop->evalInt("override_camerares", 0, t)
		&& NOT(resfraction.isFloat()) )
	{
		pixelAspect = m_rop->evalFloat("aspect_override", 0, t);
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("img_pixelAspect", pixelAspect));

	if (!m_rop->evalInt("SettingsOutput_img_save", 0, 0.0)) {
		pluginDesc.addAttribute(Attrs::PluginAttr("img_dir", Attrs::PluginAttr::AttrTypeIgnore));
		pluginDesc.addAttribute(Attrs::PluginAttr("img_file", Attrs::PluginAttr::AttrTypeIgnore));
	}
	else {
		UT_String filePath;
		m_rop->evalString(filePath, "SettingsOutput_img_file_path", 0, t);

		UT_String dirpath, filename;
		filePath.splitPath(dirpath, filename);

		// format dirpath
		dirpath.append('/');

		// Create output directory.
		directoryCreator.mkpath(dirpath.buffer());

		if (m_rop->evalInt("SettingsOutput_img_file_needFrameNumber", 0, 0.0)) {
			// NOTE: Remove after AppSDK update.
			int i = filename.length() - 1;
			while (i >= 0 && filename[i] != '.') {
				--i;
			}
			filename.insert(i, ".");
		}

		// append default file type if not set
		ImageFormat imgFormat = getImgFormat(filePath.buffer());
		if (imgFormat == imageFormatError) {
			imgFormat = imageFormatOpenEXR;
			filePath.append(imgFormatExt[imageFormatOpenEXR]);
		}
		if (imgFormat == imageFormatOpenEXR ||
			imgFormat == imageFormatVRayImage)
		{
			const int relementsSeparateFiles = m_rop->evalInt("SettingsOutput_relements_separateFiles", 0, t);
			if (relementsSeparateFiles == 0) {
				pluginDesc.addAttribute(Attrs::PluginAttr("img_rawFile", 1));
			}
		}


		pluginDesc.addAttribute(Attrs::PluginAttr("img_dir", dirpath.toStdString()));
		pluginDesc.addAttribute(Attrs::PluginAttr("img_file", filename.toStdString()));
	}

	VRay::VUtils::ValueRefList frames(1);

	if (exportFilePerFrame) {
		frames[0].setDouble(getContext().getFloatFrame());
	}
	else {
		const fpreal frameStart = CAST_ROPNODE(m_rop)->FSTART();
		frames[0].setDouble(frameStart);

		if (m_frames > 1) {
			const fpreal frameEnd = CAST_ROPNODE(m_rop)->FEND();

			if (CAST_ROPNODE(m_rop)->FINC() > 1) {
				frames = VRay::VUtils::ValueRefList(m_frames);
				for (int i = 0; i < m_frames; ++i) {
					frames[i].setDouble(frameStart + i * CAST_ROPNODE(m_rop)->FINC());
				}
			}
			else {
				VRay::VUtils::ValueRefList frameRange(2);
				frameRange[0].setDouble(frameStart);
				frameRange[1].setDouble(frameEnd);
				frames[0].setList(frameRange);
			}
		}
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("frames", frames));

	return ReturnValue::Success;
}


ReturnValue VRayExporter::exportSettings()
{
	if (RenderSettingsPlugins.empty()) {
		RenderSettingsPlugins.insert("SettingsOptions");
		RenderSettingsPlugins.insert("SettingsColorMapping");
		RenderSettingsPlugins.insert("SettingsDMCSampler");
		RenderSettingsPlugins.insert("SettingsImageSampler");
		RenderSettingsPlugins.insert("SettingsGI");
		RenderSettingsPlugins.insert("SettingsIrradianceMap");
		RenderSettingsPlugins.insert("SettingsLightCache");
		RenderSettingsPlugins.insert("SettingsDMCGI");
		RenderSettingsPlugins.insert("SettingsRaycaster");
		RenderSettingsPlugins.insert("SettingsRegionsGenerator");
		RenderSettingsPlugins.insert("SettingsOutput");
		RenderSettingsPlugins.insert("SettingsCaustics");
		RenderSettingsPlugins.insert("SettingsDefaultDisplacement");
	}

	for (const auto &sp : RenderSettingsPlugins) {
		const Parm::VRayPluginInfo *pluginInfo = Parm::getVRayPluginInfo(sp.c_str());
		if (!pluginInfo) {
			Log::getLog().error("Plugin \"%s\" description is not found!",
								sp.c_str());
		}
		else {
			if (sessionType != VfhSessionType::production) {
				if (sp == "SettingsOutput")
					continue;
			}

			Attrs::PluginDesc pluginDesc(sp, sp);
			if (sp == "SettingsOutput") {
				if (fillSettingsOutput(pluginDesc) == ReturnValue::Error) {
					return ReturnValue::Error;
				}
			}

			setAttrsFromOpNodePrms(pluginDesc, m_rop, boost::str(Parm::FmtPrefix % sp));
			exportPlugin(pluginDesc);
		}
	}

	Attrs::PluginDesc pluginDesc("settingsUnitsInfo", "SettingsUnitsInfo");
	pluginDesc.addAttribute(Attrs::PluginAttr("scene_upDir", VRay::Vector(0.0f, 1.0f, 0.0f)));
	pluginDesc.addAttribute(Attrs::PluginAttr("meters_scale",
											  OPgetDirector()->getChannelManager()->getUnitLength()));

	exportPlugin(pluginDesc);

	return ReturnValue::Success;
}


void VRayExporter::exportEnvironment(OP_Node *op_node)
{
	exportVop(CAST_VOPNODE(op_node));
}


void VRayExporter::exportEffects(OP_Node *op_net)
{
	// Test simulation export
	// Add simulations from ROP
	OP_Node *sim_node = VRayExporter::FindChildNodeByType(op_net, "VRayNodePhxShaderSimVol");
	if (sim_node) {
		exportVop(CAST_VOPNODE(sim_node));
	}
}


void VRayExporter::phxAddSimumation(VRay::Plugin sim)
{
	m_phxSimulations.insert(sim);
}


void VRayExporter::exportRenderChannels(OP_Node *op_node)
{
	exportVop(CAST_VOPNODE(op_node));
}


OP_Input* VRayExporter::getConnectedInput(OP_Node *op_node, const std::string &inputName)
{
	const unsigned input_idx = op_node->getInputFromName(inputName.c_str());
	return op_node->getInputReferenceConst(input_idx);
}


OP_Node* VRayExporter::getConnectedNode(OP_Node *op_node, const std::string &inputName)
{
	OP_Input *input = getConnectedInput(op_node, inputName);
	if (input) {
		return input->getNode();
	}
	return nullptr;
}


const Parm::SocketDesc* VRayExporter::getConnectedOutputType(OP_Node *op_node, const std::string &inputName)
{
	OP_Node *connNode = getConnectedNode(op_node, inputName);
	if (connNode) {
		const UT_String &opType = connNode->getOperator()->getName();
		if (opType.startsWith("VRayNode")) {
			OP_Input *fromOutput = getConnectedInput(op_node, inputName);
			if (fromOutput) {
				VOP::NodeBase *vrayNode = static_cast<VOP::NodeBase*>(connNode);

				const Parm::VRayPluginInfo *pluginInfo = vrayNode->getVRayPluginInfo();

				const int &fromOutputIdx = fromOutput->getNodeOutputIndex();

				if (fromOutputIdx < pluginInfo->outputs.count()) {
					return &pluginInfo->outputs[fromOutputIdx];
				}
			}
		}
	}

	return nullptr;
}


VRay::Plugin VRayExporter::exportConnectedVop(VOP_Node *vop_node, int inpidx, ExportContext *parentContext)
{
	if (NOT(vop_node)) {
		return VRay::Plugin();
	}

	VOP_Node *inpvop = vop_node->findSimpleInput(inpidx);
	if (NOT(inpvop)) {
		return VRay::Plugin();
	}

	return exportVop(inpvop, parentContext);
}


VRay::Plugin VRayExporter::exportConnectedVop(VOP_Node *vop_node, const UT_String &inputName, ExportContext *parentContext)
{
	if (NOT(vop_node)) {
		return VRay::Plugin();
	}

	const unsigned inpidx = vop_node->getInputFromName(inputName);
	return exportConnectedVop(vop_node, inpidx, parentContext);
}


void VRayExporter::RtCallbackVop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	if (!csect.tryEnter())
		return;

	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

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
			exporter.exportVop(CAST_VOPNODE(caller), nullptr);
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


VRay::Plugin VRayExporter::exportVop(OP_Node *opNode, ExportContext *parentContext)
{
	VOP_Node *vop_node = CAST_VOPNODE(opNode);
	if (!vop_node) {
		return VRay::Plugin();
	}

	const UT_String &opType = vop_node->getOperator()->getName();

	Log::getLog().debug("Exporting node \"%s\" [%s]...",
					   vop_node->getName().buffer(),
					   opType.buffer());

	if (opType == "switch") {
		const fpreal t = m_context.getTime();
		const int switcher = vop_node->evalInt("switcher", 0, t);
		return exportConnectedVop(vop_node, switcher+1, parentContext);
	}

	if (opType == "null") {
		return exportConnectedVop(vop_node, 0, parentContext);
	}

	if (opType.startsWith("principledshader")) {
		return exportPrincipledShader(*opNode, parentContext);
	}

	if (opType == "parameter") {
		return exportConnectedVop(vop_node, 0, parentContext);
	}

	if (opType.equal(vfhNodeMaterialOutput)) {
		return exportVop(getVRayNodeFromOp(*opNode, vfhSocketMaterialOutputMaterial), parentContext);
	}

	if (opType.startsWith("VRayNode")) {
		VOP::NodeBase *vrayNode = static_cast<VOP::NodeBase*>(vop_node);

		addOpCallback(vop_node, VRayExporter::RtCallbackVop);

		Attrs::PluginDesc pluginDesc;
		//TODO: need consistent naming for surface/displacement/other vops and their overrides
		pluginDesc.pluginName = VRayExporter::getPluginName(vop_node);
		pluginDesc.pluginID   = vrayNode->getVRayPluginID();

		//TODO: need consistent naming for surface/displacement/other vops and their overrides
		OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(pluginDesc, *this, parentContext);
		if (res == OP::VRayNode::PluginResultError) {
			Log::getLog().error("Error creating plugin descripion for node: \"%s\" [%s]",
								vop_node->getName().buffer(),
								opType.buffer());
		}
		else if (res == OP::VRayNode::PluginResultNA ||
				 res == OP::VRayNode::PluginResultContinue)
		{
			// NOTE: first handle all connected inputs on a VOP
			//       then consider any material overrides
			//       lastly take attr values from VOP params

			setAttrsFromOpNodeConnectedInputs(pluginDesc, vop_node, parentContext);

			// handle VOP overrides if any
			setAttrsFromSHOPOverrides(pluginDesc, *vop_node);

			setAttrsFromOpNodePrms(pluginDesc, vop_node);

			if (vrayNode->getVRayPluginType() == VRayPluginType::RENDERCHANNEL) {
				Attrs::PluginAttr *attr_chan_name = pluginDesc.get("name");
				if (NOT(attr_chan_name) || attr_chan_name->paramValue.valString.empty()) {
					const std::string channelName = vop_node->getName().buffer();
					if (NOT(attr_chan_name)) {
						pluginDesc.addAttribute(Attrs::PluginAttr("name", channelName));
					}
					else {
						attr_chan_name->paramValue.valString = channelName;
					}
				}
			}

			// TODO: this is not needed?
			if (pluginDesc.pluginID == "PhxShaderSimVol") {
				// "phoenix_sim" attribute is a List()
				//
				Attrs::PluginAttr *attr_phoenix_sim = pluginDesc.get("phoenix_sim");
				if (attr_phoenix_sim) {
					attr_phoenix_sim->paramType = Attrs::PluginAttr::AttrTypeListValue;
					attr_phoenix_sim->paramValue.valListValue.push_back(VRay::Value(attr_phoenix_sim->paramValue.valPlugin));
				}
			}

			if (   pluginDesc.pluginID == "UVWGenEnvironment"
				&& NOT(pluginDesc.contains("uvw_matrix")))
			{
				pluginDesc.addAttribute(Attrs::PluginAttr("uvw_matrix", envMatrix));
			}

			return exportPlugin(pluginDesc);
		}
	}

	Log::getLog().error("Unsupported VOP node: %s", opType.buffer());

	return VRay::Plugin();
}


void VRayExporter::RtCallbackDisplacementObj(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	if (!csect.tryEnter())
		return;

	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

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
	if (!csect.tryEnter())
		return;

	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

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
	if (!csect.tryEnter())
		return;

	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

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

int VRayExporter::exportDisplacementTexture(OP_Node &opNode, Attrs::PluginDesc &pluginDesc, const std::string &parmNamePrefix)
{
	const fpreal t = getContext().getTime();

	const PRM_Parm *parm = Parm::getParm(opNode, str(Parm::FmtPrefixManual % parmNamePrefix % "displacement_texture"));
	if (parm) {
		UT_String texPath;
		opNode.evalString(texPath, parm, 0, t);
		if (texPath.isstring()) {
			VRay::Plugin texture = exportNodeFromPathWithDefaultMapping(texPath, defaultMappingChannelName);
			if (!texture) {
				return false;
			}

			// Check if plugin has "out_intensity" output
			bool hasOutIntensity = false;

			const Parm::VRayPluginInfo *texPluginInfo = Parm::getVRayPluginInfo(texture.getType());
			if (!texPluginInfo) {
				Log::getLog().error("Node \"%s\": Plugin \"%s\" description is not found!",
									opNode.getName().buffer(), texture.getType());
				return false;
			}

			for (int i = 0; i < texPluginInfo->outputs.count(); ++i) {
				const Parm::SocketDesc &sock = texPluginInfo->outputs[i];
				if (VUtils::isEqual(sock.attrName, "out_intensity")) {
					hasOutIntensity = true;
					break;
				}
			}

			// Wrap texture with TexOutput
			if (!hasOutIntensity) {
				Attrs::PluginDesc texOutputDesc(str(FmtPluginNameWithPrefix % "Out" % texture.getName()),
												"TexOutput");
				texOutputDesc.add(Attrs::PluginAttr("texmap", texture));

				texture = exportPlugin(texOutputDesc);
			}

			pluginDesc.add(Attrs::PluginAttr("displacement_tex_color", texture));
			pluginDesc.add(Attrs::PluginAttr("displacement_tex_float", texture, "out_intensity"));

			return true;
		}
	}

	if (CAST_VOPNODE(&opNode)) {
		const int idxTexCol = opNode.getInputFromName("displacement_tex_color");
		OP_Node *texCol = opNode.getInput(idxTexCol);
		const int idxTexFloat = opNode.getInputFromName("displacement_tex_float");
		OP_Node *texFloat = opNode.getInput(idxTexFloat);
		if (texCol) {
			VRay::Plugin texture = exportVop(texCol);
			if (texture) {
				const Parm::SocketDesc *fromSocketInfo = getConnectedOutputType(&opNode, "displacement_tex_color");
				if (fromSocketInfo
				    && fromSocketInfo->attrType >= Parm::ParmType::eOutputColor
				    && fromSocketInfo->attrType < Parm::ParmType::eUnknown) {
					pluginDesc.addAttribute(Attrs::PluginAttr("displacement_tex_color", texture, fromSocketInfo->attrName.ptr()));
				}
				else {
					pluginDesc.addAttribute(Attrs::PluginAttr("displacement_tex_color", texture));
				}
				if (NOT(texFloat)) {
					// Check if plugin has "out_intensity" output
					bool hasOutIntensity = false;
					const Parm::VRayPluginInfo *texPluginInfo = Parm::getVRayPluginInfo(texture.getType());
					if (NOT(texPluginInfo)) {
						Log::getLog().error("Node \"%s\": Plugin \"%s\" description is not found!",
						                    opNode.getName().buffer(), texture.getType());
						return OP::VRayNode::PluginResultError;
					}

					for (int i = 0; i < texPluginInfo->outputs.count(); ++i) {
						const Parm::SocketDesc &sock = texPluginInfo->outputs[i];
						if (VUtils::isEqual(sock.attrName, "out_intensity")) {
							hasOutIntensity = true;
							break;
						}
					}

					// Wrap texture with TexOutput
					if (NOT(hasOutIntensity)) {
						Attrs::PluginDesc texOutputDesc(VRayExporter::getPluginName(texCol, "Out@"), "TexOutput");
						texOutputDesc.add(Attrs::PluginAttr("texmap", texture));
						texture = exportPlugin(texOutputDesc);
						pluginDesc.add(Attrs::PluginAttr("displacement_tex_float", texture, "out_intensity"));
					}
				}
			}
		}
		if (texFloat) {
			VRay::Plugin texture = exportVop(texFloat);
			if (texture) {
				const Parm::SocketDesc *fromSocketInfo = getConnectedOutputType(&opNode, "displacement_tex_float");
				if (fromSocketInfo
				    && fromSocketInfo->attrType >= Parm::ParmType::eOutputColor
				    && fromSocketInfo->attrType < Parm::ParmType::eUnknown) {
					pluginDesc.addAttribute(Attrs::PluginAttr("displacement_tex_float", texture, fromSocketInfo->attrName.ptr()));
				}
				else {
					pluginDesc.addAttribute(Attrs::PluginAttr("displacement_tex_float", texture));
				}
				pluginDesc.add(Attrs::PluginAttr("displacement_tex_color", texture));
			}
		}
	}

	return true;
}

static void setGeomDisplacedMeshType(OP_Node &opNode, const std::string &parmTypeName, Attrs::PluginDesc &pluginDesc)
{
	UT_String dispTypeMenu;
	opNode.evalString(dispTypeMenu, parmTypeName.c_str(), 0, 0.0);

	vassert(dispTypeMenu.isInteger());

	enum VRayDisplacementType {
		displ_type_2d = 0,
		displ_type_3d = 1,
		displ_type_vector = 2,
		displ_type_vector_signed = 3,
		displ_type_vector_object = 4,
	};

	const VRayDisplacementType displaceType =
		static_cast<VRayDisplacementType>(dispTypeMenu.toInt());

	if (displaceType == displ_type_2d) {
		pluginDesc.add(Attrs::PluginAttr("displace_2d", true));
		pluginDesc.add(Attrs::PluginAttr("vector_displacement", 0));
	}
	else if (displaceType == displ_type_vector) {
		pluginDesc.add(Attrs::PluginAttr("displace_2d", false));
		pluginDesc.add(Attrs::PluginAttr("vector_displacement", 1));
	}
	else if (displaceType == displ_type_vector_signed) {
		pluginDesc.add(Attrs::PluginAttr("displace_2d", false));
		pluginDesc.add(Attrs::PluginAttr("vector_displacement", 2));
	}
	else if (displaceType == displ_type_vector_object) {
		pluginDesc.add(Attrs::PluginAttr("displace_2d", false));
		pluginDesc.add(Attrs::PluginAttr("vector_displacement", 3));
	}
}

int VRayExporter::exportDisplacementFromSubdivInfo(const SubdivInfo &subdivInfo, struct Attrs::PluginDesc &pluginDesc)
{
	const std::string parmNamePrefix = subdivInfo.needParmNamePrefix() ? str(Parm::FmtPrefix % pluginDesc.pluginID) : "";

	exportDisplacementTexture(*subdivInfo.parmHolder, pluginDesc, parmNamePrefix);

	if (subdivInfo.type == SubdivisionType::displacement) {
		setGeomDisplacedMeshType(*subdivInfo.parmHolder, parmNamePrefix + "type", pluginDesc);
	}

	setAttrsFromOpNodePrms(pluginDesc, subdivInfo.parmHolder, parmNamePrefix);

	return true;
}

static const char *subdivisionPluginFromType(SubdivisionType subdivType)
{
	switch (subdivType) {
		case SubdivisionType::displacement: return "GeomDisplacedMesh";
		case SubdivisionType::subdivision:  return "GeomStaticSmoothedMesh";
		default:
			vassert(false);
	}

	return nullptr;
}

VRay::Plugin VRayExporter::exportDisplacement(OBJ_Node &objNode, const VRay::Plugin &geomPlugin, const SubdivInfo &subdivInfo)
{
	if (!subdivInfo.hasSubdiv())
		return geomPlugin; 

	Attrs::PluginDesc pluginDesc;
	pluginDesc.pluginName = str(FmtPluginNameWithPrefix % "Subdiv" % geomPlugin.getName());
	pluginDesc.pluginID = subdivisionPluginFromType(subdivInfo.type);

	pluginDesc.addAttribute(Attrs::PluginAttr("mesh", geomPlugin));

	if (!exportDisplacementFromSubdivInfo(subdivInfo, pluginDesc))
		return geomPlugin; 

	addOpCallback(&objNode, RtCallbackDisplacementObj);

	return exportPlugin(pluginDesc);
}


#ifdef CGR_HAS_VRAYSCENE

VRay::Plugin VRayExporter::exportVRayScene(OBJ_Node *obj_node, SOP_Node *geom_node)
{
	SOP::VRayScene *vraySceneNode = static_cast<SOP::VRayScene*>(geom_node);

	ExportContext ctx(CT_OBJ, *this, *static_cast<OP_Node*>(obj_node));

	Attrs::PluginDesc pluginDesc;
	OP::VRayNode::PluginResult res = vraySceneNode->asPluginDesc(pluginDesc, *this, &ctx);
	if (res == OP::VRayNode::PluginResultSuccess) {
		return exportPlugin(pluginDesc);
	}

	return VRay::Plugin();
}

#endif // CGR_HAS_VRAYSCENE


static std::string ObjectTypeToString(const OBJ_OBJECT_TYPE &ob_type)
{
	std::string object_type;

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
		if (item.op_node->hasOpInterest(this, item.cb)) {
			item.op_node->removeOpInterest(this, item.cb);
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
static void onDumpMessage(VRay::VRayRenderer& /*renderer*/, const char *msg, int level, void *data)
{
	const QString message(QString(msg).simplified());

	if (level <= VRay::MessageError) {
		Log::getLog().error("V-Ray: %s", message.toLocal8Bit().constData());
	}
	else if (level > VRay::MessageError && level <= VRay::MessageWarning) {
		Log::getLog().warning("V-Ray: %s", message.toLocal8Bit().constData());
	}
	else if (level > VRay::MessageWarning && level <= VRay::MessageInfo) {
		Log::getLog().info("V-Ray: %s", message.toLocal8Bit().constData());
	}
}

/// Callback function for the event when V-Ray updates its current computation task and the number of workunits done.
static void onProgress(VRay::VRayRenderer& /*renderer*/, const char *msg, int elementNumber, int elementsCount, void *data)
{
	const QString message(QString(msg).simplified());

	const float percentage = 100.0f * elementNumber / elementsCount;

	Log::getLog().progress("V-Ray: %s %.1f%% %s",
						   message.toLocal8Bit().constData(),
						   percentage,
						   (elementNumber >= elementsCount) ? "\n" : "\r");
}

/// Callback function for the event when rendering has finished, successfully or not.
static void onImageReady(VRay::VRayRenderer &renderer, void *data)
{
	Log::getLog().debug("onImageReady");

	VRayExporter &self = *reinterpret_cast<VRayExporter*>(data);

	// Check abort from "Stop" button or Esc key.
	if (renderer.isAborted()) {
		self.setAbort();
	}

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
	}
}
/// Callback function for the "Render Last" button in the VFB.
static void onRenderLast(VRay::VRayRenderer& /*renderer*/, bool /*isRendering*/, void *data)
{
	Log::getLog().debug("onRenderLast");

	VRayExporter &self = *reinterpret_cast<VRayExporter*>(data);
	self.renderLast();
}

/// Callback function when VFB closes.
static void onVfbClosed(VRay::VRayRenderer& /*renderer*/, void *data)
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
	}
}

static void onRendererClosed(VRay::VRayRenderer& /*renderer*/, void *data)
{
	Log::getLog().debug("onRendererClosed");
}

void VRayExporter::exportScene()
{
	Log::getLog().debug("VRayExporter::exportScene()");

	if (sessionType != VfhSessionType::ipr) {
		exportView();
	}

	// Clear plugin caches.
	objectExporter.clearOpPluginCache();
	objectExporter.clearOpDepPluginCache();
	objectExporter.clearPrimPluginCache();

	// export geometry nodes
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
	if (!activeLights || activeLights->entries() <= 0) {
		exportDefaultHeadlight();
	}
	else if (activeLights) {
		for (int i = 0; i < activeLights->entries(); ++i) {
			OBJ_Node *objNode = CAST_OBJNODE(activeLights->getNode(i));
			if (objNode) {
				exportObject(objNode);
			}
		}
	}

	UT_String env_network_path;
	m_rop->evalString(env_network_path, "render_network_environment", 0, 0.0f);
	if (NOT(env_network_path.equal(""))) {
		OP_Node *env_network = getOpNodeFromPath(env_network_path);
		if (env_network) {
			OP_Node *env_node = VRayExporter::FindChildNodeByType(env_network, "VRayNodeSettingsEnvironment");
			if (NOT(env_node)) {
				Log::getLog().error("Node of type \"VRay SettingsEnvironment\" is not found!");
			}
			else {
				exportEnvironment(env_node);
				exportEffects(env_network);
			}
		}
	}

	UT_String channels_network_path;
	m_rop->evalString(channels_network_path, "render_network_render_channels", 0, 0.0f);
	if (NOT(channels_network_path.equal(""))) {
		OP_Node *channels_network = getOpNodeFromPath(channels_network_path);
		if (channels_network) {
			OP_Node *chan_node = VRayExporter::FindChildNodeByType(channels_network, "VRayNodeRenderChannelsContainer");
			if (NOT(chan_node)) {
				Log::getLog().error("Node of type \"VRay RenderChannelsContainer\" is not found!");
			}
			else {
				exportRenderChannels(chan_node);
			}
		}
	}

	if (sessionType == VfhSessionType::ipr) {
		Attrs::PluginDesc texOpId("userAttrOpId", "TexUserColor");
		texOpId.add(Attrs::PluginAttr("user_attribute", "Op_Id"));
		texOpId.add(Attrs::PluginAttr("attribute_priority", 1));

		Attrs::PluginDesc rcOpId("rcUserAttrOpId", "RenderChannelExtraTex");
		rcOpId.add(Attrs::PluginAttr("name", "OpID"));
		rcOpId.add(Attrs::PluginAttr("consider_for_aa", false));
		rcOpId.add(Attrs::PluginAttr("filtering", false));
		rcOpId.add(Attrs::PluginAttr("affect_matte_objects", false));
		rcOpId.add(Attrs::PluginAttr("enableDeepOutput", false));
		rcOpId.add(Attrs::PluginAttr("texmap", exportPlugin(texOpId)));
		exportPlugin(rcOpId);
	}

	// Add simulations from OBJ
	if (!m_phxSimulations.empty()) {
		Attrs::PluginDesc phxSims("VRayNodePhxShaderSimVol", "PhxShaderSimVol");
		VRay::ValueList sims(m_phxSimulations.size());
		std::transform(m_phxSimulations.begin(), m_phxSimulations.end(), sims.begin(), [](const VRay::Plugin &plugin) {
			return VRay::Value(plugin);
		});
		phxSims.addAttribute(Attrs::PluginAttr("phoenix_sim", sims));

		exportPlugin(phxSims);
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


void VRayExporter::removePlugin(OBJ_Node *node, int checkExisting)
{
	removePlugin(Attrs::PluginDesc(VRayExporter::getPluginName(node), ""), checkExisting);
}


void VRayExporter::removePlugin(const std::string &pluginName, int checkExisting)
{
	removePlugin(Attrs::PluginDesc(pluginName, ""), checkExisting);
}


void VRayExporter::removePlugin(const Attrs::PluginDesc &pluginDesc, int checkExisting)
{
	m_renderer.removePlugin(pluginDesc, checkExisting);
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

	VRay::RendererOptions options = vray.getOptions();
	options.noDR = NOT(drEnabled);
	vray.setOptions(options);

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


void VRayExporter::setRenderMode(VRay::RendererOptions::RenderMode mode)
{
	SettingsRTEngine settingsRTEngine;
	setSettingsRTEngineFromRopNode(settingsRTEngine, *m_rop, mode);
	setSettingsRTEnginetOptimizedGpuSettings(settingsRTEngine, *m_rop, mode);

	m_renderer.setRendererMode(settingsRTEngine, mode);

	m_isGPU = mode >= VRay::RendererOptions::RENDER_MODE_RT_GPU_OPENCL;
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


int VRayExporter::renderFrame(int locked)
{
	Log::getLog().debug("VRayExporter::renderFrame(%.3f)", m_context.getFloatFrame());

	if (m_workMode == ExpExport || m_workMode == ExpExportRender) {
		const fpreal t = getContext().getTime();

		UT_String exportFilepath;
		m_rop->evalString(exportFilepath, "render_export_filepath", 0, t);

		if (!exportFilepath.isstring()) {
			Log::getLog().error("Export mode is selected, but no filepath specified!");
		}
		else {
			VRay::VRayExportSettings expSettings;
			expSettings.useHexFormat = m_rop->evalInt("exp_hexdata", 0, t);
			expSettings.compressed = m_rop->evalInt("exp_compressed", 0, t);

			exportVrscene(exportFilepath.toStdString(), expSettings);
		}
	}

	if (m_workMode == ExpRender || m_workMode == ExpExportRender) {
		m_renderer.startRender(locked);
	}

	return 0;
}


int VRayExporter::renderSequence(int start, int end, int step, int locked)
{
	return m_renderer.startSequence(start, end, step, locked);
}


int VRayExporter::exportVrscene(const std::string &filepath, VRay::VRayExportSettings &settings)
{
	// Create export directory.
	QFileInfo filePathInfo(filepath.c_str());
	directoryCreator.mkpath(filePathInfo.absoluteDir().path());

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

void VRayExporter::initExporter(int hasUI, int nframes, fpreal tstart, fpreal tend)
{
	const int logLevel = m_rop->evalInt("exporter_log_level", 0, 0.0);
	Log::getLog().setLogLevel(logLevel == 0 ? Log::LogLevelError : Log::LogLevelDebug);

	OBJ_Node *camera = VRayExporter::getCamera(m_rop);
	if (!camera) {
		Log::getLog().error("Camera is not set!");
		m_error = ROP_ABORT_RENDER;
		return;
	}

	resetOpCallbacks();

	m_viewParams = ViewParams();
	m_exportedFrames.clear();
	m_phxSimulations.clear();
	m_frames    = nframes;
	m_timeStart = tstart;
	m_timeEnd   = tend;
	m_isAborted = false;
	m_isAnimation = nframes > 1;
	m_isMotionBlur = hasMotionBlur(*m_rop, *camera);
	m_isVelocityOn = hasVelocityOn(*m_rop);

	// Reset time before exporting settings.
	setTime(0.0);
	setAnimation(sessionType == VfhSessionType::production &&
		(m_isAnimation || m_isMotionBlur || m_isVelocityOn));

	if (hasUI) {
		if (!getRenderer().getVRay().vfb.isShown()) {
			restoreVfbState();
		}
		getRenderer().getVfbSettings(vfbSettings);
		getRenderer().showVFB(m_workMode != ExpExport, m_rop->getFullPath());
	}

	m_renderer.getVRay().setOnProgress(onProgress, this);
	m_renderer.getVRay().setOnDumpMessage(onDumpMessage, this);
	m_renderer.getVRay().setOnImageReady(onImageReady, this);
	m_renderer.getVRay().setOnVFBClosed(onVfbClosed, this);
	m_renderer.getVRay().setOnRendererClose(onRendererClosed, this);
	m_renderer.getVRay().setOnRenderLast(onRenderLast, this);

	const int isExportMode =
		m_workMode == ExpExport ||
		m_workMode == ExpExportRender;

	exportFilePerFrame =
		isExportMode &&
		isAnimation() &&
		isExportFramesToSeparateFiles(*m_rop);

	m_error = ROP_CONTINUE_RENDER;
}


int VRayExporter::hasVelocityOn(OP_Node &rop) const
{
	const fpreal t = m_context.getTime();

	UT_String rcNetworkPath;
	rop.evalString(rcNetworkPath, "render_network_render_channels", 0, t);
	OP_Node *rcNode = getOpNodeFromPath(rcNetworkPath, t);
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
	getRenderer().getVRay().setCurrentTime(m_context.getFloatFrame());

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
		while (!isAborted() && (subframe <= mbParams.mb_end)) {
			const fpreal mbFrame = subframe >= 0.0 ? subframe : 0.0;

			if (!m_exportedFrames.count(mbFrame)) {
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
	else {
		renderFrame(!isInteractive());
	}
}


void VRayExporter::exportEnd()
{
	Log::getLog().debug("VRayExporter::exportEnd()");

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

VRay::RendererOptions::RenderMode VRayForHoudini::getRendererMode(const OP_Node &rop)
{
	const VfhRenderModeMenu renderMode =
		static_cast<VfhRenderModeMenu>(rop.evalInt("render_render_mode", 0, 0.0));

	switch (renderMode) {
		case VfhRenderModeMenu::cpu:    return VRay::RendererOptions::RENDER_MODE_PRODUCTION;
		case VfhRenderModeMenu::cuda:   return VRay::RendererOptions::RENDER_MODE_PRODUCTION_CUDA;
		case VfhRenderModeMenu::opencl: return VRay::RendererOptions::RENDER_MODE_PRODUCTION_OPENCL;
	}

	vassert(false && "VRayForHoudini::getRendererMode(): Incorrect \"render_render_mode\" value!");

	return VRay::RendererOptions::RENDER_MODE_PRODUCTION;
}

VRay::RendererOptions::RenderMode VRayForHoudini::getRendererIprMode(const OP_Node &rop)
{
	const VfhRenderModeMenu renderMode =
		static_cast<VfhRenderModeMenu>(rop.evalInt("render_rt_mode", 0, 0.0));

	switch (renderMode) {
		case VfhRenderModeMenu::cpu:    return VRay::RendererOptions::RENDER_MODE_RT_CPU;
		case VfhRenderModeMenu::cuda:   return VRay::RendererOptions::RENDER_MODE_RT_GPU_CUDA;
		case VfhRenderModeMenu::opencl: return VRay::RendererOptions::RENDER_MODE_RT_GPU_OPENCL;
	}

	vassert(false && "VRayForHoudini::getRendererIprMode(): Incorrect \"render_rt_mode\" value!");

	return VRay::RendererOptions::RENDER_MODE_PRODUCTION;
}

VRayExporter::ExpWorkMode VRayForHoudini::getExportMode(const OP_Node &rop)
{
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
		vfbSettingsParm->setValue(0.0, buf.toLocal8Bit().constData(), CH_STRING_LITERAL);
	}
}

void VRayExporter::restoreVfbState()
{
	if (!m_rop)
		return;

	PRM_Parm *vfbSettingsParm = m_rop->getParmPtr("_vfb_settings");
	if (!vfbSettingsParm)
		return;

	UT_String vfbState;
	m_rop->evalString(vfbState, "_vfb_settings", 0, 0.0);

	if (vfbState.isstring()) {
		getRenderer().restoreVfbState(vfbState.buffer());
	}
}

void VRayExporter::renderLast()
{
	if (!m_rop)
		return;

	initExporter(true, m_frames, m_timeStart, m_timeEnd);
	exportFrame(m_context.getTime());
}
