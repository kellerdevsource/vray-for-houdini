//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_exporter.h"
#include "vfh_prm_globals.h"
#include "vfh_prm_templates.h"
#include "vfh_tex_utils.h"
#include "vfh_hou_utils.h"

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


using namespace VRayForHoudini;


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


VRay::Transform VRayExporter::Matrix4ToTransform(const UT_Matrix4D &m4, bool flip)
{
	VRay::Transform tm;
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			tm.matrix[i][j] = m4[i][j];
		}
		tm.offset[i] = m4[3][i];
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
		OP_Node *node = OPgetDirector()->findNode(camera_path.buffer());
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
			Log::getLog().msg("Pending override: %s %s",
							  opNode.getName().buffer(), parmName.c_str());
		}

		const fpreal &t = m_context.getTime();
#if 0
		Log::getLog().info("Setting: [%s] %s_%s (from %s_%s)",
						   pluginDesc.pluginID.c_str(),
						   pluginDesc.pluginName.c_str(), attrDesc.attr.c_str(),
						   opNode.getName().buffer(), parmName.c_str());
#endif
		Attrs::PluginAttr attr;
		attr.paramName = attrDesc.attr;

		if (attrDesc.value.type == Parm::eBool ||
			attrDesc.value.type == Parm::eInt  ||
			attrDesc.value.type == Parm::eTextureInt)
		{
			attr.paramType = Attrs::PluginAttr::AttrTypeInt;
			attr.paramValue.valInt = opNode.evalInt(parmName.c_str(), 0, t);
		}
		else if (attrDesc.value.type == Parm::eEnum) {
			const int menuIndex = opNode.evalInt(parmName.c_str(), 0, t);

			const Parm::EnumItem &enumItem = attrDesc.value.defEnumItems[menuIndex];
			if (enumItem.valueType == Parm::EnumItem::EnumValueInt) {
				attr.paramType = Attrs::PluginAttr::AttrTypeInt;
				attr.paramValue.valInt = enumItem.value;
			}
			else {
				attr.paramType = Attrs::PluginAttr::AttrTypeString;
				attr.paramValue.valString = enumItem.valueString;
			}
		}
		else if (attrDesc.value.type == Parm::eFloat ||
				 attrDesc.value.type == Parm::eTextureFloat) {
			attr.paramType = Attrs::PluginAttr::AttrTypeFloat;
			attr.paramValue.valFloat = (float)opNode.evalFloat(parmName.c_str(), 0, t);

			if (attrDesc.convert_to_radians) {
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


VRay::Transform VRayExporter::exportTransformVop(VOP_Node &vop_node, ExportContext *parentContext)
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

	return Matrix4ToTransform(m4);
}


void VRayExporter::setAttrsFromOpNodeConnectedInputs(Attrs::PluginDesc &pluginDesc, VOP_Node *vopNode, ExportContext *parentContext)
{
	const Parm::VRayPluginInfo *pluginInfo = Parm::GetVRayPluginInfo( pluginDesc.pluginID );
	if (NOT(pluginInfo)) {
		Log::getLog().error("Node \"%s\": Plugin \"%s\" description is not found!",
							vopNode->getName().buffer(), pluginDesc.pluginID.c_str());
		return;
	}

	// handle connected VOP inputs
	for (const auto &inSockInfo : pluginInfo->inputs) {
		const std::string attrName = inSockInfo.name.getToken();
		if (   NOT(pluginInfo->attributes.count(attrName))
			|| pluginDesc.contains(attrName) )
		{
			continue;
		}

		const Parm::AttrDesc &attrDesc = pluginInfo->attributes.at(attrName);
		if ( attrDesc.custom_handling ) {
			continue;
		}

		VRay::Plugin plugin_value = exportConnectedVop(vopNode, attrName.c_str(), parentContext);
		if (NOT(plugin_value)) {

			if (  NOT(attrDesc.linked_only)
				&& pluginInfo->pluginType == Parm::PluginTypeTexture
				&& attrName == "uvwgen" )
			{
				Attrs::PluginDesc uvwGen(VRayExporter::getPluginName(vopNode, "Uvw"), "UVWGenObject");
				plugin_value = exportPlugin(uvwGen);
			}
			else {
				const unsigned inpidx = vopNode->getInputFromName(attrName.c_str());
				VOP_Node *inpvop = vopNode->findSimpleInput(inpidx);
				if (inpvop) {
					if (inpvop->getOperator()->getName() == "makexform") {
						switch (inSockInfo.type) {
							case Parm::eMatrix:
							{
								pluginDesc.addAttribute(Attrs::PluginAttr(attrName, exportTransformVop(*inpvop, parentContext).matrix));
								break;
							}
							case Parm::eTransform:
							{
								pluginDesc.addAttribute(Attrs::PluginAttr(attrName, exportTransformVop(*inpvop, parentContext)));
								break;
							}
						}
					}
				}
			}
		}

		if (plugin_value) {
			Log::getLog().info("  Setting plugin value: %s = %s",
							   attrName.c_str(), plugin_value.getName());

			const Parm::SocketDesc *fromSocketInfo = getConnectedOutputType(vopNode, attrName.c_str());

			if (fromSocketInfo &&
				fromSocketInfo->type >= Parm::ParmType::eOutputColor &&
				fromSocketInfo->type  < Parm::ParmType::eUnknown)
			{
				Log::getLog().info("    Using output: %s (\"%s\")",
								   fromSocketInfo->name.getToken(), fromSocketInfo->name.getLabel());
				pluginDesc.addAttribute(Attrs::PluginAttr(attrName, plugin_value, fromSocketInfo->name.getToken()));
			}
			else {
				pluginDesc.addAttribute(Attrs::PluginAttr(attrName, plugin_value));
			}
		}
	}
}


void VRayExporter::setAttrsFromOpNodePrms(Attrs::PluginDesc &pluginDesc, OP_Node *opNode, const std::string &prefix, bool remapInterp)
{
	const Parm::VRayPluginInfo *pluginInfo = Parm::GetVRayPluginInfo(pluginDesc.pluginID);
	if (NOT(pluginInfo)) {
		Log::getLog().error("Node \"%s\": Plugin \"%s\" description is not found!",
							opNode->getName().buffer(), pluginDesc.pluginID.c_str());
	}
	else {
		for (const auto &aIt : pluginInfo->attributes) {
			const std::string    &attrName = aIt.first;
			const Parm::AttrDesc &attrDesc = aIt.second;

			if (!(pluginDesc.contains(attrName) || attrDesc.custom_handling)) {
				const std::string &parmName = prefix.empty()
											  ? attrDesc.attr
											  : boost::str(Parm::FmtPrefixManual % prefix % attrDesc.attr);

				const PRM_Parm *parm = Parm::getParm(*opNode, parmName);

				// check for properties that are marked for custom handling on hou side
				if (parm) {
					auto spareData = parm->getSparePtr();
					if (spareData && spareData->getValue("vray_custom_handling")) {
						continue;
					}
				}

				bool isTextureAttr = (   attrDesc.value.type == Parm::eTextureInt
									|| attrDesc.value.type == Parm::eTextureFloat
									|| attrDesc.value.type == Parm::eTextureColor);

				if ( isTextureAttr
					&& parm
					&& parm->getType().isStringType())
				{
					UT_String parmVal;
					opNode->evalString(parmVal, parm->getToken(), 0, 0.0f);
					OP_Node *tex_node = OPgetDirector()->findNode(parmVal.buffer());
					if (tex_node) {
						VRay::Plugin texPlugin = exportVop(tex_node);
						if (texPlugin) {
							pluginDesc.addAttribute(Attrs::PluginAttr(attrName, texPlugin));
						}
					}
				}
				else if (!attrDesc.linked_only) {
					if (attrDesc.value.type == Parm::eRamp) {
						static StringSet rampColorAsPluginList;
						if (rampColorAsPluginList.empty()) {
							rampColorAsPluginList.insert("PhxShaderSim");
						}

						// TODO: Move to attribute description
						const bool asColorList = rampColorAsPluginList.count(pluginDesc.pluginID);

						Texture::exportRampAttribute(*this, pluginDesc, opNode,
													 /* Houdini ramp attr */ parmName,
													 /* V-Ray attr: colors */ attrDesc.value.defRamp.colors,
													 /* V-Ray attr: pos    */ attrDesc.value.defRamp.positions,
													 /* V-Ray attr: interp */ attrDesc.value.defRamp.interpolations,
													 /* As color list not plugin */ asColorList,
													 /* Remap to vray interpolations*/ remapInterp);

						pluginDesc.addAttribute(Attrs::PluginAttr(attrName, Attrs::PluginAttr::AttrTypeIgnore));
					}
					else if (attrDesc.value.type == Parm::eCurve) {

						VRay::IntList    interpolations;
						VRay::FloatList  positions;
						VRay::FloatList  values;
						VRay::FloatList *valuesPtr = attrDesc.value.defCurve.values.empty()
													 ? nullptr
													 : &values;

						Texture::getCurveData(*this, opNode,
											  /* Houdini curve attr */ parmName,
											  /* V-Ray attr: interp */ interpolations,
											  /* V-Ray attr: x      */ positions,
											  /* V-Ray attr: y      */ valuesPtr,
											  /* Don't need handles */ false,
											  /* Remap to vray interpolations*/ remapInterp);

						pluginDesc.addAttribute(Attrs::PluginAttr(attrDesc.value.defCurve.interpolations, interpolations));
						pluginDesc.addAttribute(Attrs::PluginAttr(attrDesc.value.defCurve.positions,      positions));
						if (valuesPtr) {
							pluginDesc.addAttribute(Attrs::PluginAttr(attrDesc.value.defCurve.values,     values));
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

	const Parm::VRayPluginInfo *pluginInfo = Parm::GetVRayPluginInfo(pluginDesc.pluginID);
	if (NOT(pluginInfo)) {
		return res;
	}

	for (const auto &aIt : pluginInfo->attributes) {
		const std::string    &attrName = aIt.first;
		const Parm::AttrDesc &attrDesc = aIt.second;

		if (NOT(options.hasOption(attrName))) {
			continue;
		}

		Attrs::PluginAttr attr;
		attr.paramName = attrDesc.attr;

		if (   attrDesc.value.type == Parm::eBool
			|| attrDesc.value.type == Parm::eInt
			|| attrDesc.value.type == Parm::eTextureInt)
		{
			attr.paramType = Attrs::PluginAttr::AttrTypeInt;
			attr.paramValue.valInt = options.getOptionI(attrName);
		}
		else if (attrDesc.value.type == Parm::eEnum) {
			const Parm::EnumItem &enumItem = attrDesc.value.defEnumItems.at(0);
			if (enumItem.valueType == Parm::EnumItem::EnumValueInt) {
				attr.paramType = Attrs::PluginAttr::AttrTypeInt;
				attr.paramValue.valInt = options.getOptionI(attrName);
			}
			else {
				attr.paramType = Attrs::PluginAttr::AttrTypeString;
				attr.paramValue.valString = options.getOptionS(attrName);
			}
		}
		else if (   attrDesc.value.type == Parm::eFloat
				 || attrDesc.value.type == Parm::eTextureFloat)
		{
			attr.paramType = Attrs::PluginAttr::AttrTypeFloat;
			attr.paramValue.valFloat = options.getOptionF(attrName);

			if (attrDesc.convert_to_radians) {
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


VRayExporter::VRayExporter(VRayRendererNode *rop)
	: m_rop(rop)
	, m_isIPR(false)
	, m_isAnimation(false)
	, m_error(ROP_CONTINUE_RENDER)
{
	Log::getLog().debug("VRayExporter()");
}


VRayExporter::~VRayExporter()
{
	Log::getLog().debug("~VRayExporter()");

	resetOpCallbacks();
}


void VRayExporter::fillSettingsOutput(Attrs::PluginDesc &pluginDesc)
{
	const fpreal t = getContext().getTime();
	OBJ_Node *camera = VRayExporter::getCamera(m_rop);

	fpreal pixelAspect = camera->evalFloat("aspect", 0, t);

	UT_String resfraction;
	m_rop->evalString(resfraction, "res_fraction", 0, t);
	if (   m_rop->evalInt("override_camerares", 0, t)
		&& NOT(resfraction.isFloat()) )
	{
		pixelAspect = m_rop->evalFloat("aspect_override", 0, t);
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("img_pixelAspect", pixelAspect));

	// NOTE: we are exporting animation related properties in frames
	// and compensating for this by setting SettingsUnitsInfo::seconds_scale
	// i.e. scaling V-Ray time unit (see function exportSettings())
	fpreal animStart = m_rop->FSTART();
	fpreal animEnd = m_rop->FEND();
	VRay::VUtils::ValueRefList frames(1);
	frames[0].setDouble(animStart);
	if (m_frames > 1) {
		if (m_rop->FINC() > 1) {
			frames = VRay::VUtils::ValueRefList(m_frames);
			for (int i = 0; i < m_frames; ++i) {
				frames[i].setDouble(animStart + i * m_rop->FINC());
			}
		}
		else {
			VRay::VUtils::ValueRefList frameRange(2);
			frameRange[0].setDouble(animStart);
			frameRange[1].setDouble(animEnd);
			frames[0].setList(frameRange);
		}
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("anim_start", animStart));
	pluginDesc.addAttribute(Attrs::PluginAttr("anim_end", animEnd));
	pluginDesc.addAttribute(Attrs::PluginAttr("frames_per_second", 1));
	pluginDesc.addAttribute(Attrs::PluginAttr("frame_start", animStart));
	pluginDesc.addAttribute(Attrs::PluginAttr("frames", frames));
}


void VRayExporter::exportSettings()
{
	for (const auto &sp : Parm::RenderSettingsPlugins) {
		const Parm::VRayPluginInfo *pluginInfo = Parm::GetVRayPluginInfo(sp);
		if (!pluginInfo) {
			Log::getLog().error("Plugin \"%s\" description is not found!",
								sp.c_str());
		}
		else {
			Attrs::PluginDesc pluginDesc(sp, sp);
			if (sp == "SettingsOutput") {
				fillSettingsOutput(pluginDesc);
			}

			setAttrsFromOpNodePrms(pluginDesc, m_rop, boost::str(Parm::FmtPrefix % sp));
			exportPlugin(pluginDesc);
		}
	}

	Attrs::PluginDesc pluginDesc("settingsUnitsInfo", "SettingsUnitsInfo");
	// Houdini's time unit is fixed to second
	// so SettingsUnitsInfo::seconds_scale shhould always be 1
	pluginDesc.addAttribute(Attrs::PluginAttr("scene_upDir", VRay::Vector(0.0f, 1.0f, 0.0f)));
	pluginDesc.addAttribute(Attrs::PluginAttr("meters_scale",
											  OPgetDirector()->getChannelManager()->getUnitLength()));
	pluginDesc.addAttribute(Attrs::PluginAttr("seconds_scale",
											  OPgetDirector()->getChannelManager()->getTimeDelta(1)));

	exportPlugin(pluginDesc);
}


void VRayExporter::exportEnvironment(OP_Node *op_node)
{
	exportVop(op_node);
}


void VRayExporter::exportEffects(OP_Node *op_net)
{
	// Test simulation export
	// Add simulations from ROP
	OP_Node *sim_node = VRayExporter::FindChildNodeByType(op_net, "VRayNodePhxShaderSimVol");
	if (sim_node) {
		exportVop(sim_node);
	}
}


void VRayExporter::phxAddSimumation(VRay::Plugin sim)
{
	m_phxSimulations.push_back(VRay::Value(sim));
}


void VRayExporter::exportRenderChannels(OP_Node *op_node)
{
	exportVop(op_node);
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

				if (fromOutputIdx < pluginInfo->outputs.size()) {
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


int VRayExporter::isNodeAnimated(OP_Node *op_node)
{
	int process = true;

	if (isAnimation() && (m_context.getTime() > m_timeStart)) {
		// TODO: Detect animation
		// process = op_node->hasAnimatedParms();
	}

	return process;
}


void VRayExporter::RtCallbackVop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

	Log::getLog().info("RtCallbackVop: %s from \"%s\"",
					   OPeventToString(type), caller->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED: {
			if (Parm::isParmSwitcher(*caller, long(data))) {
				break;
			}
		}
		case OP_INPUT_CHANGED:
		case OP_INPUT_REWIRED: {
			exporter.exportVop(caller, nullptr);
			break;
		}
		case OP_NODE_PREDELETE: {
			exporter.delOpCallback(caller, VRayExporter::RtCallbackVop);
			break;
		}
		default:
			break;
	}
}


VRay::Plugin VRayExporter::exportVop(OP_Node *op_node, ExportContext *parentContext)
{
	VOP_Node *vop_node = op_node->castToVOPNode();
	const UT_String &opType = vop_node->getOperator()->getName();

	Log::getLog().info("Exporting node \"%s\" [%s]...",
					   vop_node->getName().buffer(),
					   opType.buffer());

	if (opType == "switch") {
		const fpreal t = m_context.getTime();
		const int switcher = vop_node->evalInt("switcher", 0, t);
		return exportConnectedVop(vop_node, switcher+1, parentContext);
	}
	else if (opType == "null") {
		return exportConnectedVop(vop_node, 0, parentContext);
	}
	else if (opType.startsWith("VRayNode")) {
		VOP::NodeBase *vrayNode = static_cast<VOP::NodeBase*>(vop_node);

		addOpCallback(op_node, VRayExporter::RtCallbackVop);

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

			if (vrayNode->getVRayPluginType() == "RENDERCHANNEL") {
				Attrs::PluginAttr *attr_chan_name = pluginDesc.get("name");
				if (NOT(attr_chan_name) || attr_chan_name->paramValue.valString.empty()) {
					const std::string channelName = op_node->getName().buffer();
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
				VRay::Transform envMatrix;
				envMatrix.matrix.setCol(0, VRay::Vector(0.f,1.f,0.f));
				envMatrix.matrix.setCol(1, VRay::Vector(0.f,0.f,1.f));
				envMatrix.matrix.setCol(2, VRay::Vector(1.f,0.f,0.f));
				envMatrix.offset.makeZero();
				pluginDesc.addAttribute(Attrs::PluginAttr("uvw_matrix", envMatrix));
			}

			return exportPlugin(pluginDesc);
		}
	}
	else {
		Log::getLog().error("Unsupported VOP node: %s",
							opType.buffer());
	}

	return VRay::Plugin();
}


void VRayExporter::RtCallbackDisplacementObj(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

	Log::getLog().info("RtCallbackDisplacementObj: %s from \"%s\"",
					   OPeventToString(type), caller->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED: {
			if (Parm::isParmSwitcher(*caller, long(data))) {
				break;
			}

			const PRM_Parm *parm = Parm::getParm(*caller, reinterpret_cast<long>(data));
			if (parm) {
				OBJ_Node *obj_node = caller->castToOBJNode();
				if (boost::equals(parm->getToken(), "vray_use_displ") ||
					boost::equals(parm->getToken(), "vray_displ_type"))
				{
					exporter.exportObject(obj_node);
				}
				else {
					VRay::Plugin geom;
					exporter.exportDisplacement(obj_node, geom);
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
}


void VRayExporter::RtCallbackDisplacementShop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

	Log::getLog().info("RtCallbackDisplacementShop: %s from \"%s\"",
					   OPeventToString(type), caller->getName().buffer());

	if (type == OP_INPUT_REWIRED) {
		UT_String inputName;
		const int idx = reinterpret_cast<long>(data);
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
}


void VRayExporter::RtCallbackDisplacementVop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

	Log::getLog().info("RtCallbackDisplacementVop: %s from \"%s\"",
					   OPeventToString(type), caller->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED: {
			if (Parm::isParmSwitcher(*caller, long(data))) {
				break;
			}
		}
		case OP_INPUT_REWIRED: {
			const int idx = reinterpret_cast<long>(data);
			SHOP_Node *shop_node = caller->getParent()->castToSHOPNode();
			if (idx >= 0 && shop_node) {
				UT_String shopPath;
				shop_node->getFullPath(shopPath);

				OP_NodeList refs;
				shop_node->getExistingOpDependents(refs, true);
				for (OP_Node *node : refs) {
					UT_String nodePath;
					node->getFullPath(nodePath);

					OBJ_Node *obj_node = node->castToOBJNode();
					if (obj_node) {
						VRay::Plugin geom;
						exporter.exportDisplacement(obj_node, geom);
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
}


void VRayExporter::exportDisplacementDesc(OBJ_Node *obj_node, Attrs::PluginDesc &pluginDesc)
{
	const std::string parmPrefix = boost::str(Parm::FmtPrefixManual % pluginDesc.pluginID % "_");
	const PRM_Parm *parm = Parm::getParm(*obj_node, boost::str(Parm::FmtPrefixManual % parmPrefix % "displacement_tex_color"));
	if (parm) {
		UT_String texpath;
		obj_node->evalString(texpath, parm, 0, 0.0f);
		OP_Node *tex_node = OPgetDirector()->findNode(texpath.buffer());
		if (tex_node) {
			VRay::Plugin texture = exportVop(tex_node);
			if (texture) {
				pluginDesc.add(Attrs::PluginAttr("displacement_tex_color", texture));

				// Check if plugin has "out_intensity" output
				bool hasOutIntensity = false;
				const Parm::VRayPluginInfo *texPluginInfo = Parm::GetVRayPluginInfo(texture.getType());
				if (NOT(texPluginInfo)) {
					Log::getLog().error("Node \"%s\": Plugin \"%s\" description is not found!",
										obj_node->getName().buffer(), texture.getType());
					return;
				}
				if (texPluginInfo->outputs.size()) {
					for (const auto &sock : texPluginInfo->outputs) {
						if (StrEq(sock.name.getToken(), "out_intensity")) {
							hasOutIntensity = true;
							break;
						}
					}
				}

				// Wrap texture with TexOutput
				if (NOT(hasOutIntensity)) {
					Attrs::PluginDesc texOutputDesc(VRayExporter::getPluginName(tex_node, "Out@"), "TexOutput");
					texOutputDesc.add(Attrs::PluginAttr("texmap", texture));

					texture = exportPlugin(texOutputDesc);
					pluginDesc.add(Attrs::PluginAttr("displacement_tex_float", texture, "out_intensity"));
				}
			}
		}
	}

	setAttrsFromOpNodePrms(pluginDesc, obj_node, parmPrefix);
}


VRay::Plugin VRayExporter::exportDisplacement(OBJ_Node *obj_node, VRay::Plugin &geomPlugin)
{
	VRay::Plugin plugin;

	addOpCallback(obj_node, VRayExporter::RtCallbackDisplacementObj);

	bool useDisplacement = Parm::isParmExist(*obj_node, "vray_use_displ") && obj_node->evalInt("vray_use_displ", 0, 0.0);
	if (useDisplacement) {
		Attrs::PluginDesc pluginDesc;
		const int displType = obj_node->evalInt("vray_displ_type", 0, 0.0);
		switch (displType) {
			// use shopnet
			case 0:
			{
				UT_String shopPath;
				obj_node->evalString(shopPath, "vray_displshoppath", 0, 0.0);
				SHOP_Node *shop_node = OPgetDirector()->findSHOPNode(shopPath.buffer());
				if (shop_node) {
					OP_Node *op_node = VRayExporter::FindChildNodeByType(shop_node, "vray_material_output");
					if (op_node) {
						VOP::MaterialOutput *mtl_out = static_cast<VOP::MaterialOutput *>(op_node);
						addOpCallback(op_node, VRayExporter::RtCallbackDisplacementShop);

						if (mtl_out->error() < UT_ERROR_ABORT ) {
							const int idx = mtl_out->getInputFromName("Geometry");
							VOP::NodeBase *input = dynamic_cast<VOP::NodeBase*>(mtl_out->getInput(idx));
							if (input) {
								addOpCallback(input, VRayExporter::RtCallbackDisplacementVop);

								// TODO: use shop export context to handle material overrides
								ExportContext expContext(CT_OBJ, *this, *obj_node);
								OP::VRayNode::PluginResult res = input->asPluginDesc(pluginDesc, *this, &expContext);
								if (res == OP::VRayNode::PluginResultError) {
									Log::getLog().error("Error creating plugin descripion for node: \"%s\" [%s]",
														input->getName().buffer(), input->getOperator()->getName().buffer());
								}
								else if (res == OP::VRayNode::PluginResultNA ||
										 res == OP::VRayNode::PluginResultContinue)
								{
									if (geomPlugin) {
										pluginDesc.addAttribute(Attrs::PluginAttr("mesh", geomPlugin));
									}

									setAttrsFromOpNodeConnectedInputs(pluginDesc, input);
									setAttrsFromOpNodePrms(pluginDesc, input);
								}

								plugin = exportPlugin(pluginDesc);
							}
						}
					}
					else {
						Log::getLog().error("Can't find \"V-Ray Material Output\" operator under \"%s\"!",
											shop_node->getName().buffer());
					}
				}
				break;
			}
				// use GeomDisplacedMesh
			case 1:
			{
				pluginDesc.pluginName = VRayExporter::getPluginName(obj_node, "GeomDisplacedMesh@");
				pluginDesc.pluginID = "GeomDisplacedMesh";
				if (geomPlugin) {
					pluginDesc.addAttribute(Attrs::PluginAttr("mesh", geomPlugin));
				}
				exportDisplacementDesc(obj_node, pluginDesc);

				plugin = exportPlugin(pluginDesc);
				break;
			}
				// use GeomStaticSmoothedMesh
			case 2:
			{
				pluginDesc.pluginName = VRayExporter::getPluginName(obj_node, "GeomStaticSmoothedMesh@");
				pluginDesc.pluginID = "GeomStaticSmoothedMesh";
				if (geomPlugin) {
					pluginDesc.addAttribute(Attrs::PluginAttr("mesh", geomPlugin));
				}
				exportDisplacementDesc(obj_node, pluginDesc);

				plugin = exportPlugin(pluginDesc);
				break;
			}
			default:
				break;
		}
	}

	return plugin;
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
	// Install callbacks only for interactive session
	if (isIPR()) {
		if (!op_node->hasOpInterest(this, cb)) {
			Log::getLog().info("addOpInterest(%s)",
							   op_node->getName().buffer());

			op_node->addOpInterest(this, cb);

			// Store registered callback for faster removal
			m_opRegCallbacks.push_back(OpInterestItem(op_node, cb, this));
		}
	}
}


void VRayExporter::delOpCallback(OP_Node *op_node, OP_EventMethod cb)
{
	if (op_node->hasOpInterest(this, cb)) {
		Log::getLog().info("removeOpInterest(%s)",
						   op_node->getName().buffer());

		op_node->removeOpInterest(this, cb);
	}
}


void VRayExporter::delOpCallbacks(OP_Node *op_node)
{
	m_opRegCallbacks.erase(std::remove_if(m_opRegCallbacks.begin(), m_opRegCallbacks.end(),
										  [op_node](OpInterestItem &item) { return item.op_node == op_node; }), m_opRegCallbacks.end());
}


void VRayExporter::onDumpMessage(VRay::VRayRenderer& /*renderer*/, const char *msg, int level)
{
	QString message(msg);
	message = message.simplified();

	if (level <= VRay::MessageError) {
		Log::getLog().error("V-Ray: %s", message.toAscii().constData());
	}
	else if (level > VRay::MessageError && level <= VRay::MessageWarning) {
		Log::getLog().warning("V-Ray: %s", message.toAscii().constData());
	}
	else if (level > VRay::MessageWarning && level <= VRay::MessageInfo) {
		Log::getLog().info("V-Ray: %s", message.toAscii().constData());
	}
}


void VRayExporter::onProgress(VRay::VRayRenderer& /*renderer*/, const char *msg, int elementNumber, int elementsCount)
{
	QString message(msg);
	message = message.simplified();

	const float percentage = 100.0f * elementNumber / elementsCount;

	Log::getLog().progress("V-Ray: %s %.1f%% %s",
						   message.toAscii().constData(),
						   percentage,
						   (elementNumber >= elementsCount) ? "\n" : "\r");
}


void VRayExporter::onAbort(VRay::VRayRenderer &renderer)
{
	if (renderer.isAborted()) {
		setAbort();
	}
}


void VRayExporter::RtCallbackObjManager(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	Log::getLog().info("RtCallbackObjManager: %s from \"%s\"",
					   OPeventToString(type), caller->getName().buffer());

	VRayExporter &exporter = *reinterpret_cast< VRayExporter* >(callee);

	switch (type) {
		case OP_CHILD_CREATED:
		case OP_CHILD_DELETED:
		case OP_CHILD_REORDERED: /* undo */
		case OP_GROUPLIST_CHANGED:
		{
			exporter.getRop().startIPR(exporter.getContext().getTime());
			break;
		}
		case OP_NODE_PREDELETE:
		{
			exporter.delOpCallbacks(caller);
			break;
		}
	}
}


void VRayExporter::exportScene()
{
	setCurrentTime(m_context.getFloatFrame());

	Log::getLog().debug("VRayExporter::exportScene(%.3f)",
						m_context.getFloatFrame());

	exportView();

	// add RT update callbacks to detect scene export changes
	addOpCallback(m_rop, VRayRendererNode::RtCallbackRop);
	addOpCallback(OPgetDirector()->getManager("obj"), VRayExporter::RtCallbackObjManager);

	// export geometry nodes
	OP_Bundle *activeGeo = m_rop->getActiveGeometryBundle();
	if (activeGeo) {
		for (int i = 0; i < activeGeo->entries(); ++i) {
			OP_Node *node = activeGeo->getNode(i);
			if (!node) {
				continue;
			}

			OBJ_Node *objNode = node->castToOBJNode();
			if (!objNode) {
				continue;
			}

			exportObject(objNode);
		}
	}

	// export light nodes
	exportLights();

	UT_String env_network_path;
	m_rop->evalString(env_network_path, Parm::parm_render_net_environment.getToken(), 0, 0.0f);
	if (NOT(env_network_path.equal(""))) {
		OP_Node *env_network = OPgetDirector()->findNode(env_network_path.buffer());
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
	m_rop->evalString(channels_network_path, Parm::parm_render_net_render_channels.getToken(), 0, 0.0f);
	if (NOT(channels_network_path.equal(""))) {
		OP_Node *channels_network = OPgetDirector()->findNode(channels_network_path.buffer());
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

	// Add simulations from OBJ
	if (m_phxSimulations.size()) {
		Attrs::PluginDesc phxSims("VRayNodePhxShaderSimVol", "PhxShaderSimVol");
		phxSims.addAttribute(Attrs::PluginAttr("phoenix_sim", m_phxSimulations));

		exportPlugin(phxSims);
	}
}


void VRayExporter::fillMotionBlurParams(MotionBlurParams &mbParams)
{
	OBJ_Node *camera = getCamera(m_rop);
	if (camera && isPhysicalView(*camera)) {
		const int cameraType = Parm::getParmInt(*camera, "CameraPhysical_type");
		const fpreal frameDuration = OPgetDirector()->getChannelManager()->getSecsPerSample();

		switch (cameraType) {
			// Still camera
			case 0: {
				mbParams.mb_duration        = 1.0f / (Parm::getParmFloat(*camera, "CameraPhysical_shutter_speed") * frameDuration);
				mbParams.mb_interval_center = mbParams.mb_duration * 0.5f;
				break;
			}
				// Cinematic camera
			case 1: {
				mbParams.mb_duration        = Parm::getParmFloat(*camera, "CameraPhysical_shutter_angle") / 360.0f;
				mbParams.mb_interval_center = Parm::getParmFloat(*camera, "CameraPhysical_shutter_offset") / 360.0f + mbParams.mb_duration * 0.5f;
				break;
			}
				// Video camera
			case 2: {
				mbParams.mb_duration        = 1.0f + Parm::getParmFloat(*camera, "CameraPhysical_latency") / frameDuration;
				mbParams.mb_interval_center = -mbParams.mb_duration * 0.5f;
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
	removePlugin(Attrs::PluginDesc(VRayExporter::getPluginName(node), ""));
}


void VRayExporter::removePlugin(const std::string &pluginName)
{
	removePlugin(Attrs::PluginDesc(pluginName, ""));
}


void VRayExporter::removePlugin(const Attrs::PluginDesc &pluginDesc)
{
	m_renderer.removePlugin(pluginDesc);
}


void VRayExporter::setCurrentTime(fpreal time)
{
	m_renderer.setCurrentTime(time);
}


void VRayExporter::setIPR(int isIPR)
{
	m_isIPR = isIPR;
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


void VRayExporter::setRendererMode(int mode)
{
	m_renderer.setRendererMode(mode);
	m_isGPU = (mode >= 1);

	if (mode >= 0) {
		setSettingsRtEngine();
	}
}


void VRayExporter::setWorkMode(VRayExporter::ExpWorkMode mode)
{
	m_workMode = mode;
}


void VRayExporter::setContext(const OP_Context &ctx)
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

	if (m_vfb.isInitialized()) {
		m_vfb.resize(w, h);
	}

	m_renderer.setImageSize(w, h);
}


void VRayExporter::setSettingsRtEngine()
{
	VRay::Plugin settingsRTEngine = m_renderer.getVRay().getInstanceOrCreate("SettingsRTEngine");

	Attrs::PluginDesc settingsRTEngineDesc(settingsRTEngine.getName(), "SettingsRTEngine");

	settingsRTEngineDesc.addAttribute(Attrs::PluginAttr("stereo_mode",         isStereoView() ? Parm::getParmInt(*m_rop, "VRayStereoscopicSettings_use") : 0));
	settingsRTEngineDesc.addAttribute(Attrs::PluginAttr("stereo_eye_distance", isStereoView() ? Parm::getParmFloat(*m_rop, "VRayStereoscopicSettings_eye_distance") : 0));
	settingsRTEngineDesc.addAttribute(Attrs::PluginAttr("stereo_focus",        isStereoView() ? Parm::getParmInt(*m_rop, "VRayStereoscopicSettings_focus_method") : 0));

	setAttrsFromOpNodePrms(settingsRTEngineDesc, m_rop, "SettingsRTEngine_");

	exportPluginProperties(settingsRTEngine, settingsRTEngineDesc);
}


int VRayExporter::isStereoView() const
{
	return Parm::getParmInt(*m_rop, "VRayStereoscopicSettings_use");
}


int VRayExporter::renderFrame(int locked)
{
	setCurrentTime(getContext().getFloatFrame());

	Log::getLog().debug("VRayExporter::renderFrame(%.3f)", m_context.getFloatFrame());

	if (m_workMode == ExpWorkMode::ExpExport || m_workMode == ExpWorkMode::ExpExportRender) {
		const fpreal t = getContext().getTime();

		UT_String exportFilepath;
		m_rop->evalString(exportFilepath, "render_export_filepath", 0, t);

		if (exportFilepath.isstring()) {
			VRay::VRayExportSettings expSettings;
			expSettings.framesInSeparateFiles = m_rop->evalInt("exp_separatefiles", 0, t);
			expSettings.useHexFormat = m_rop->evalInt("exp_hexdata", 0, t);
			expSettings.compressed = m_rop->evalInt("exp_compressed", 0, t);

			exportVrscene(exportFilepath.toStdString(), expSettings);
		}
		else {
			Log::getLog().error("Export mode is selected, but no filepath specified!");
		}
	}

	if (m_workMode == ExpWorkMode::ExpRender || m_workMode == ExpWorkMode::ExpExportRender) {
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
	return m_renderer.exportScene(filepath, settings);
}


void VRayExporter::clearKeyFrames(float toTime)
{
	Log::getLog().debug("VRayExporter::clearKeyFrames(%.3f)",
						toTime);

	// TODO: Replace with clearFramesUpToFrame()
	// m_renderer.clearFrames(toTime);
}


void VRayExporter::setAnimation(bool on)
{
	Log::getLog().debug("VRayExporter::setAnimation(%i)", on);

	m_isAnimation = on;
	m_renderer.setAnimation(on);
}

int VRayExporter::initRenderer(int hasUI, int reInit)
{
	m_renderer.stopRender();
	return m_renderer.initRenderer(hasUI, reInit);
}


void VRayExporter::initExporter(int hasUI, int nframes, fpreal tstart, fpreal tend)
{
	OBJ_Node *camera = VRayExporter::getCamera(m_rop);
	if (!camera) {
		Log::getLog().error("Camera is not set!");
		m_error = ROP_ABORT_RENDER;
		return;
	}

	m_viewParams = ViewParams();
	m_exportedFrames.clear();
	m_phxSimulations.clear();
	m_frames    = nframes;
	m_timeStart = tstart;
	m_timeEnd   = tend;
	m_isAborted = false;

	setAnimation(nframes > 1);

	getRenderer().resetCallbacks();
	resetOpCallbacks();

	if (hasUI >= 0) {
#ifdef __APPLE__
		// Forse Qt FB
		const int hasUI = 1;
		getRenderer().showVFB(false);
#else
#endif
		if (hasUI == 0) {
#ifndef __APPLE__
			m_vfb.free();
			getRenderer().showVFB(m_workMode != ExpExport, m_rop->getFullPath());
#endif
		}
		else if (hasUI == 1) {
			if (m_workMode != ExpExport) {
				m_vfb.init();
				m_vfb.show();
				m_vfb.set_abort_callback(UI::AbortCb(boost::bind(&VRayPluginRenderer::stopRender, &getRenderer())));

				getRenderer().addCbOnDumpMessage(CbOnDumpMessage(boost::bind(&UI::VFB::on_dump_message, &m_vfb, _1, _2, _3)));
				getRenderer().addCbOnProgress(CbOnProgress(boost::bind(&UI::VFB::on_progress, &m_vfb, _1, _2, _3, _4)));

				getRenderer().addCbOnImageReady(CbOnImageReady(boost::bind(&UI::VFB::on_image_ready, &m_vfb, _1)));

				getRenderer().addCbOnBucketInit(CbOnBucketInit(boost::bind(&UI::VFB::on_bucket_init, &m_vfb, _1, _2, _3, _4, _5, _6)));
				getRenderer().addCbOnBucketFailed(CbOnBucketFailed(boost::bind(&UI::VFB::on_bucket_failed, &m_vfb, _1, _2, _3, _4, _5, _6)));
				getRenderer().addCbOnBucketReady(CbOnBucketReady(boost::bind(&UI::VFB::on_bucket_ready, &m_vfb, _1, _2, _3, _4, _5)));

				getRenderer().addCbOnRTImageUpdated(CbOnRTImageUpdated(boost::bind(&UI::VFB::on_rt_image_updated, &m_vfb, _1, _2)));
			}
		}
	}

	m_renderer.addCbOnProgress(CbOnProgress(boost::bind(&VRayExporter::onProgress, this, _1, _2, _3, _4)));
	m_renderer.addCbOnDumpMessage(CbOnDumpMessage(boost::bind(&VRayExporter::onDumpMessage, this, _1, _2, _3)));

	if (isAnimation()) {
		m_renderer.addCbOnImageReady(CbOnImageReady(boost::bind(&VRayExporter::onAbort, this, _1)));
	}
	else if (isIPR()) {
		m_renderer.addCbOnImageReady(CbVoid(boost::bind(&VRayExporter::resetOpCallbacks, this)));
		m_renderer.addCbOnRendererClose(CbVoid(boost::bind(&VRayExporter::resetOpCallbacks, this)));
	}

	m_isMotionBlur = hasMotionBlur(*m_rop, *camera);
	m_isVelocityOn = hasVelocityOn(*m_rop);

	// NOTE: Force animated values for motion blur
	if (!isAnimation()) {
		m_renderer.setAnimation(m_isMotionBlur || m_isVelocityOn);
	}

	m_error = ROP_CONTINUE_RENDER;
}


int VRayExporter::hasVelocityOn(OP_Node &rop) const
{
	const fpreal t = m_context.getTime();

	UT_String rcNetworkPath;
	rop.evalString(rcNetworkPath, Parm::parm_render_net_render_channels.getToken(), 0, t);
	OP_Network *rcNetwork = dynamic_cast< OP_Network * >(OPgetDirector()->findNode(rcNetworkPath));
	if (NOT(rcNetwork)) {
		return false;
	}

	UT_ValArray< OP_Node * > rcOutputList;
	if (NOT(rcNetwork->getOpsByName("VRayNodeRenderChannelsContainer", rcOutputList))) {
		return false;
	}

	UT_ValArray< OP_Node * > velVOPList;
	if (NOT(rcNetwork->getOpsByName("VRayNodeRenderChannelVelocity", velVOPList))) {
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
	int hasMB = false;
	if (isPhysicalView(camera)) {
		hasMB = camera.evalInt("CameraPhysical_use_moblur", 0, 0.0);
	}
	else {
		hasMB = rop.evalInt("SettingsMotionBlur_on", 0, 0.0);
	}
	return hasMB;
}


void MotionBlurParams::calcParams(fpreal currFrame)
{
	mb_start = currFrame - (mb_duration * (0.5 - mb_interval_center));
	mb_end   = mb_start + mb_duration;
	mb_frame_inc = mb_duration / std::max(mb_geom_samples - 1, 1);

	Log::getLog().info("  MB time: %.3f", currFrame);
	Log::getLog().info("  MB duration: %.3f", mb_duration);
	Log::getLog().info("  MB interval center: %.3f", mb_interval_center);
	Log::getLog().info("  MB geom samples: %i", mb_geom_samples);
	Log::getLog().info("  MB start: %.3f", mb_start);
	Log::getLog().info("  MB end:   %.3f", mb_end);
	Log::getLog().info("  MB inc:   %.3f", mb_frame_inc);
}


void VRayExporter::exportFrame(fpreal time)
{
	m_context.setTime(time);

	Log::getLog().debug("VRayExporter::exportFrame(%.3f)",
						m_context.getFloatFrame());

	if (   !m_isMotionBlur
		&& !m_isVelocityOn)
	{
		clearKeyFrames(m_context.getFloatFrame());
		exportScene();
	}
	else {
		MotionBlurParams mbParams;
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
			if (!m_exportedFrames.count(subframe)) {
				m_exportedFrames.insert(subframe);
				m_context.setFrame(subframe);
				exportScene();
			}

			subframe += mbParams.mb_frame_inc;
		}

		// Set time back to original time for rendering
		m_context.setTime(time);
	}

	if (isAborted()) {
		Log::getLog().info("Operation is aborted by the user!");
		m_error = ROP_ABORT_RENDER;
	}
	else {
		bool lock = (!HOU::isUIAvailable() || isAnimation());
		renderFrame(lock);
	}
}


void VRayExporter::exportEnd()
{
	Log::getLog().debug("VRayExporter::exportEnd()");

	if (isAnimation()) {
		clearKeyFrames(SYS_FP64_MAX);
	}

	m_error = ROP_CONTINUE_RENDER;
}
