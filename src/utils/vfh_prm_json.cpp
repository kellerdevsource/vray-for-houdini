//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
//

#include "vfh_defines.h"
#include "vfh_prm_def.h"
#include "vfh_prm_json.h"
#include "vfh_prm_globals.h"
#include "vfh_plugin_info.h"
#include "vfh_ui.h"

#include "vop/material/vop_MtlMulti.h"
#include "vop/brdf/vop_BRDFLayered.h"
#include "vop/texture/vop_TexLayered.h"
#include "vop/texture/vop_texture_output.h"
#include "sop/sop_node_def.h"
#include "obj/obj_node_def.h"

#include <QtCore/QDirIterator>

#ifdef _MSC_VER
#  include <boost/config/compiler/visualc.hpp>
#endif
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>

#define SKIP_TYPE(attrType) (\
	attrType == "LIST"     || \
	attrType == "MAPCHANNEL_LIST" || \
	attrType == "COLOR_LIST" || \
	attrType == "VECTOR_LIST" || \
	attrType == "TRANSFORM_LIST" || \
	attrType == "INT_LIST" || \
	attrType == "FLOAT_LIST")

#define OUTPUT_TYPE(attrType) (\
	attrType == "OUTPUT_PLUGIN"         || \
	attrType == "OUTPUT_COLOR"          || \
	attrType == "OUTPUT_TEXTURE"        || \
	attrType == "OUTPUT_INT_TEXTURE"    || \
	attrType == "OUTPUT_FLOAT_TEXTURE"  || \
	attrType == "OUTPUT_VECTOR_TEXTURE" || \
	attrType == "OUTPUT_MATRIX_TEXTURE" || \
	attrType == "OUTPUT_TRANSFORM_TEXTURE")

#define MAPPABLE_TYPE(attrType) (\
	attrType == "BRDF"              || \
	attrType == "MATERIAL"          || \
	attrType == "GEOMETRY"          || \
	attrType == "PLUGIN"            || \
	attrType == "VECTOR"            || \
	attrType == "UVWGEN"            || \
	attrType == "MATRIX"            || \
	attrType == "TRANSFORM"         || \
	attrType == "TEXTURE"           || \
	attrType == "FLOAT_TEXTURE"     || \
	attrType == "INT_TEXTURE"       || \
	attrType == "VECTOR_TEXTURE"    || \
	attrType == "MATRIX_TEXTURE"    || \
	attrType == "TRANSFORM_TEXTURE")

#define NOT_ANIMATABLE_TYPE(attrType) (\
	attrType == "BRDF"     || \
	attrType == "MATERIAL" || \
	attrType == "GEOMETRY" || \
	attrType == "UVWGEN"   || \
	attrType == "PLUGIN")


using namespace VRayForHoudini;
using namespace VRayForHoudini::Parm;


typedef boost::property_tree::ptree       JsonTree;
typedef JsonTree::value_type              JsonItem;
typedef std::map<std::string, JsonTree>   JsonDescs;


struct JsonPluginDescGenerator {
public:
	JsonPluginDescGenerator() {}
	~JsonPluginDescGenerator() { freeData(); }

	JsonTree  *getTree(const std::string &pluginID);

	void       init();
	bool       hasData() { return parsedData.size(); }
	void       parseData();
	void       freeData();

private:
	JsonDescs  parsedData;

} JsonPluginInfoParser;


void JsonPluginDescGenerator::init()
{
	PRINT_INFO("JsonPluginDescGenerator::init()");

	// Some compilers doesn't support initialization lists still.
	// Move elsewhere...
	//
	// RenderSettingsPlugins.insert("SettingsRTEngine");

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

	RenderSettingsPlugins.insert("SettingsCamera");
	RenderSettingsPlugins.insert("SettingsMotionBlur");

	RenderGIPlugins.insert("SettingsGI");
	RenderGIPlugins.insert("SettingsLightCache");
	RenderGIPlugins.insert("SettingsIrradianceMap");
	RenderGIPlugins.insert("SettingsDMCGI");
}


void JsonPluginDescGenerator::parseData()
{
	PRINT_INFO("JsonPluginDescGenerator::parseData()");

	const char *jsonDescsFilepath = getenv("VRAY_PLUGIN_DESC_PATH");
	if (NOT(jsonDescsFilepath)) {
		PRINT_ERROR("VRAY_PLUGIN_DESC_PATH environment variable is not found!");
		return;
	}

	QDirIterator it(jsonDescsFilepath, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QString &filePath = it.next();
		if (filePath.endsWith(".json")) {
			QFileInfo fileInfo(filePath);

#ifdef __APPLE__
			std::ifstream fileStream(filePath.toAscii().constData());
#else
			std::ifstream fileStream(filePath.toStdString());
#endif
			const std::string &fileName = fileInfo.baseName().toStdString();

			try {
				JsonTree &pTree = parsedData[fileName];
				boost::property_tree::json_parser::read_json(fileStream, pTree);
			}
			catch (...) {
				PRINT_ERROR("Error parsing %s",
							fileName.c_str());
			}
		}
	}

	if (NOT(parsedData.size())) {
		PRINT_ERROR("No descriptions parsed! May be VRAY_PLUGIN_DESC_PATH points to an empty / incorrect directory?");
	}
}


JsonTree* JsonPluginDescGenerator::getTree(const std::string &pluginID)
{
	if (parsedData.count(pluginID)) {
		return &parsedData[pluginID];
	}
	return nullptr;
}


void JsonPluginDescGenerator::freeData()
{
	parsedData.clear();
}


static bool AttrNeedWidget(const AttrDesc &attrDesc)
{
	if (attrDesc.value.type < ParmType::ePlugin) {
		return true;
	}
	return false;
}


static PRM_Template AttrDescAsPrmTemplate(const AttrDesc &attrDesc)
{
	PRM_Template tmpl;

	// TODO:
	//  [ ] Collect and clean up ptrs (or does it really needed?)
	//
	PRM_Name *prm_name = new PRM_Name(attrDesc.attr.c_str(), attrDesc.label.c_str());

	if (attrDesc.value.type == ParmType::eBool) {
		tmpl = PRM_Template(PRM_TOGGLE, 1, prm_name, attrDesc.value.getDefBool());
	}
	else if (attrDesc.value.type == ParmType::eInt ||
			 attrDesc.value.type == ParmType::eTextureInt) {
		tmpl = PRM_Template(PRM_INT, 1, prm_name, attrDesc.value.getDefInt());
	}
	else if (attrDesc.value.type == ParmType::eFloat ||
			 attrDesc.value.type == ParmType::eTextureFloat) {
		tmpl = PRM_Template(PRM_FLT, 1, prm_name, attrDesc.value.getDefFloat());
	}
	else if (attrDesc.value.type == ParmType::eColor ||
			 attrDesc.value.type == ParmType::eAColor ||
			 attrDesc.value.type == ParmType::eTextureColor) {
		tmpl = PRM_Template(PRM_RGB_J, PRM_Template::PRM_EXPORT_TBX, 4, prm_name, attrDesc.value.getDefColor());
	}
	else if (attrDesc.value.type == ParmType::eEnum) {
		PRM_Name *enumMenuItems = new PRM_Name[attrDesc.value.defEnumItems.size()+1];
		int i = 0;
		for (const auto &item : attrDesc.value.defEnumItems) {
			enumMenuItems[i++] = PRM_Name(item.label.c_str());
		}
		enumMenuItems[i] = PRM_Name(); // Items terminator

		PRM_ChoiceList *enumMenu = new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, enumMenuItems);

		tmpl = PRM_Template(PRM_ORD, 1, prm_name, PRMzeroDefaults, enumMenu);
	}
	else if (attrDesc.value.type == ParmType::eString) {
		// TODO:
		//  [ ] Proper default value
		//  [ ] Template type base on subtype (filepath, dirpath or name)
		//
		tmpl = PRM_Template(PRM_FILE, 1, prm_name, &PRMemptyStringDefault);
	}
	else if (attrDesc.value.type == ParmType::eCurve) {
		tmpl = PRM_Template(PRM_MULTITYPE_RAMP_FLT,  NULL /*tmpl*/, 1 /*vec_size*/, prm_name, PRMtwoDefaults, NULL /*range*/, &Parm::PRMcurveDefault);
	}
	else if (attrDesc.value.type == ParmType::eRamp) {
		tmpl = PRM_Template(PRM_MULTITYPE_RAMP_RGB,  NULL /*tmpl*/, 1 /*vec_size*/, prm_name, PRMtwoDefaults, NULL /*range*/, &Parm::RPMrampDefault);
	}

	if (tmpl.getType() == PRM_LIST_TERMINATOR) {
		PRINT_ERROR("Incorrect attribute template: %s (%s)",
					prm_name->getToken(), prm_name->getLabel());
	}

	return tmpl;
}


static UI::StateInfo ActiveStateGetStateInfo(const std::string &pluginID, const JsonItem &item, bool prefix=false)
{
	UI::StateInfo stateInfo;

	if (item.second.count("active") || item.second.count("show")) {
		stateInfo.visual = item.second.count("active")
						   ? UI::StateInfo::VisualDisable
						   : UI::StateInfo::VisualHide;

		const auto &conditionDesc = stateInfo.visual == UI::StateInfo::VisualDisable
									? item.second.get_child("active")
									: item.second.get_child("show");

		stateInfo.conditionAttr = conditionDesc.get_child("prop").data();

		std::string cond_type("equal");
		if (conditionDesc.count("condition")) {
			cond_type = conditionDesc.get_child("condition").data();
		}

		if (!(conditionDesc.count("value"))) {
			// Treat as toggle
			stateInfo.conditionValue = true;
			stateInfo.condition      = UI::StateInfo::CondEqual;
		}
		else {
			try {
				// TODO: bool
				stateInfo.conditionValue = conditionDesc.get<int>("value");
			}
			catch (...) {
				stateInfo.conditionValue = 0;
			}
			if (cond_type == "equal") {
				stateInfo.condition = UI::StateInfo::CondEqual;
			}
			else if (cond_type == "not_equal") {
				stateInfo.condition = UI::StateInfo::CondNonEqual;
			}
			else if (cond_type == "greater") {
				stateInfo.condition = UI::StateInfo::CondGreater;
			}
			else if (cond_type == "greater_or_equal") {
				stateInfo.condition = UI::StateInfo::CondGreaterOrEqual;
			}
			else if (cond_type == "less") {
				stateInfo.condition = UI::StateInfo::CondLess;
			}
			else if (cond_type == "less_or_equal") {
				stateInfo.condition = UI::StateInfo::CondLessOrEqual;
			}
		}

		if (prefix) {
			stateInfo.conditionAttr = pluginID + "." + stateInfo.conditionAttr;
		}
	}

	return stateInfo;
}


static void ActiveStateProcessWidgetLayout(const std::string &pluginID, VRayPluginInfo *nodeType, const UI::StateInfo &parentState, const JsonItem &item, bool prefix=false)
{
	bool debug = false; // pluginID == "SettingsImageSampler";

	for (const auto &at : item.second.get_child("attrs")) {
		if (at.second.count("name")) {
			const std::string &attrName = at.second.get_child("name").data();
			const std::string &generatedAttrName = prefix
												   ? pluginID + "." + attrName
												   : attrName;

			if (NOT(attrName.empty()) && nodeType->attributes.count(attrName)) {
				const AttrDesc &attrDesc = nodeType->attributes[attrName];

				if (NOT(AttrNeedWidget(attrDesc))) {
					continue;
				}

				if (parentState) {
					UI::ActiveStateDeps::addStateInfo(pluginID, generatedAttrName, parentState);

					if (debug) {
						PRINT_INFO("Active layout property: \"%s\" => \"%s\"",
								   parentState.conditionAttr.c_str(), generatedAttrName.c_str());
					}
				}
				else {
					const UI::StateInfo &stateInfo = ActiveStateGetStateInfo(pluginID, at, prefix);
					if (stateInfo) {
						UI::ActiveStateDeps::addStateInfo(pluginID, generatedAttrName, stateInfo);

						if (debug) {
							PRINT_INFO("Active attr property: \"%s\" => \"%s\"",
									   stateInfo.conditionAttr.c_str(), generatedAttrName.c_str());
						}
					}
				}
			}
		}
	}
}


static void AddAttrsTemplate(VRayPluginInfo *nodeType, const JsonItem &item, int &i)
{
	for (const auto &at : item.second.get_child("attrs")) {
		if (at.second.count("name")) {
			const std::string &attrName = at.second.get_child("name").data();

			if (NOT(attrName.empty()) && nodeType->attributes.count(attrName)) {
				const AttrDesc &attrDesc = nodeType->attributes[attrName];

				if (NOT(AttrNeedWidget(attrDesc))) {
					continue;
				}

				nodeType->prm_template.push_back(AttrDescAsPrmTemplate(attrDesc));
			}
		}
	}
}


PRM_Template* Parm::GeneratePrmTemplate(const std::string &pluginType, const std::string &pluginID, const bool &useWidget, const bool &usePrefix, const std::string &pluginPrefix)
{
	if (NOT(JsonPluginInfoParser.hasData())) {
		JsonPluginInfoParser.init();
		JsonPluginInfoParser.parseData();
	}

	const std::string &fullPluginID = usePrefix
									  ? pluginPrefix + pluginID
									  : pluginID;

	VRayPluginInfo *pluginInfo = Parm::GetVRayPluginInfo(fullPluginID);
	if (pluginInfo) {
		return &pluginInfo->prm_template[0];
	}

	pluginInfo = Parm::NewVRayPluginInfo(fullPluginID);
	pluginInfo->pluginType = pluginType;

	// Create default outputs
	if (pluginType == "BRDF") {
		pluginInfo->outputs.push_back(SocketDesc(PRM_Name("BRDF", "BRDF"), VOP_TYPE_BSDF));
	}
	else if (pluginType == "MATERIAL") {
		pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Material", "Material"), VOP_SURFACE_SHADER));
	}
	else if (pluginType == "UVWGEN") {
		pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Mapping", "Mapping"), VOP_TYPE_VECTOR));
	}
	else if (pluginType == "TEXTURE") {
		if (pluginID == "BitmapBuffer") {
			pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Bitmap", "Bitmap"), VOP_TYPE_VOID));
		}
		else {
			pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Output", "Output"), VOP_TYPE_COLOR));
		}
	}
	else if (pluginType == "GEOMETRY") {
		pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Geometry", "Geometry"), VOP_GEOMETRY_SHADER));
	}
	else if (pluginType == "RENDERCHANNEL" && NOT(pluginID == "SettingsRenderChannels")) {
		pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Channel", "Channel"), VOP_TYPE_VOID));
	}
	else if (pluginType == "EFFECT") {
		if (pluginID == "PhxShaderCache") {
			pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Cache", "Cache"), VOP_TYPE_VOID));
		}
		else {
			pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Effect", "Effect"), VOP_TYPE_VOID));
		}
	}

#define CGR_DEBUG_JSON_ATTR_GEN 0
#if CGR_DEBUG_JSON_ATTR_GEN
	PRINT_INFO("Generating description: \"%s\"...",
			   pluginID.c_str());
#endif

	const JsonTree *jsonPluginDesc = JsonPluginInfoParser.getTree(pluginID);
	if (jsonPluginDesc) {
		for (const auto &v : jsonPluginDesc->get_child("Parameters")) {
			const std::string &attrName = v.second.get_child("attr").data();
			const std::string &attrType = v.second.get_child("type").data();

			if (SKIP_TYPE(attrType)) {
				continue;
			}

			AttrDesc &attrDesc = pluginInfo->attributes[attrName];

			if (v.second.count("skip")) {
				// Skip was used in a meaning of non-automatic export,
				// so param should be generated
				attrDesc.custom_handling = v.second.get<bool>("skip");
			}

			if (v.second.count("name")) {
				attrDesc.label = v.second.get_child("name").data();
			}
			else {
				attrDesc.label = attrName;

				// Title case and "_" to space
				for (int i = 0; i < attrDesc.label.length(); ++i) {
					const char &c = attrDesc.label[i];
					if (c >= 'A' && c <= 'Z') {
						continue;
					}

					if ((i == 0) || (attrDesc.label[i-1] == '_')) {
						if (i) {
							attrDesc.label[i-1] = ' ';
						}
						// Don't modify digits
						if (NOT(c >= '0' && c <= '9')) {
							attrDesc.label[i] -= 32;
						}
					}
				}
			}

			attrDesc.attr = usePrefix
							? pluginPrefix + pluginID + "." + attrName
							: attrName;
			attrDesc.desc = v.second.get_child("desc").data();
#if CGR_DEBUG_JSON_ATTR_GEN
			PRINT_INFO("Processing attribute: name=\"%s\" label=\"%s\"",
					   attrDesc.attr.c_str(), attrDesc.label.c_str());
#endif
			VOP_Type socketType = VOP_TYPE_UNDEF;

			if (attrType == "BOOL") {
				attrDesc.value.type = ParmType::eBool;
				attrDesc.value.defBool = v.second.get<bool>("default");
				socketType = VOP_TYPE_INTEGER;
			}
			else if (attrType == "INT") {
				attrDesc.value.type = ParmType::eInt;
				attrDesc.value.defInt = v.second.get<int>("default");
				socketType = VOP_TYPE_INTEGER;
			}
			else if (attrType == "INT_TEXTURE") {
				attrDesc.value.type = ParmType::eTextureInt;
				attrDesc.value.defInt = v.second.get<int>("default");
				socketType = VOP_TYPE_INTEGER;
			}
			else if (attrType == "FLOAT") {
				attrDesc.value.type = ParmType::eFloat;
				attrDesc.value.defFloat = v.second.get<float>("default");
				socketType = VOP_TYPE_FLOAT;
			}
			else if (attrType == "FLOAT_TEXTURE") {
				attrDesc.value.type = ParmType::eTextureFloat;
				attrDesc.value.defFloat = v.second.get<float>("default");
				socketType = VOP_TYPE_FLOAT;
			}
			else if (attrType == "ENUM") {
				attrDesc.value.type = ParmType::eEnum;

				Parm::EnumItem::EnumValueType enumType = Parm::EnumItem::EnumValueInt;

				// Collect enum items
				// Enum item definition ("value", "UI Name", "Description")

				for (const auto &_v : v.second.get_child("items")) {
					std::vector<std::string> r;
					for (const auto &item : _v.second.get_child("")) {
						r.push_back(item.second.get_value<std::string>(""));
					}

					Parm::EnumItem item;
					try {
						item.value = boost::lexical_cast<int>(r[0]);
					}
					catch (...) {
						enumType = Parm::EnumItem::EnumValueString;
						item.valueString = r[0];
						item.valueType   = enumType;
					}

					item.label = r[1];
					item.desc  = r[2];

					attrDesc.value.defEnumItems.push_back(item);
				}

				// Menu items are 0 based, but our enum indexes could be -1,
				// so we need to find defaut menu index
				int menuDefIndex = 0;
				if (enumType == Parm::EnumItem::EnumValueInt) {
					const int enumDefValue = boost::lexical_cast<int>(v.second.get<int>("default"));
					for (const auto &eIt : attrDesc.value.defEnumItems) {
						if (eIt.value == enumDefValue)
							break;
						menuDefIndex++;
					}
				}
				else {
					const std::string &enumDefValue = v.second.get_child("default").data();
					for (const auto &eIt : attrDesc.value.defEnumItems) {
						if (eIt.valueString == enumDefValue) {
							break;
						}
						menuDefIndex++;
					}
				}
				attrDesc.value.defEnum = menuDefIndex;
			}
			else if (attrType == "COLOR") {
				attrDesc.value.type = ParmType::eColor;

				int c = 0;
				for (const auto &_v : v.second.get_child("default")) {
					attrDesc.value.defColor[c++] = _v.second.get<float>("");
				}

				socketType = VOP_TYPE_COLOR;
			}
			else if (attrType == "ACOLOR") {
				attrDesc.value.type = ParmType::eAColor;

				int c = 0;
				for (const auto &_v : v.second.get_child("default")) {
					attrDesc.value.defAColor[c++] = _v.second.get<float>("");
				}

				socketType = VOP_TYPE_COLOR;
			}
			else if (attrType == "TEXTURE") {
				attrDesc.value.type = ParmType::eTextureColor;

				int c = 0;
				for (const auto &_v : v.second.get_child("default")) {
					attrDesc.value.defAColor[c++] = _v.second.get<float>("");
				}

				socketType = VOP_TYPE_COLOR;
			}
			else if (attrType == "STRING") {
				attrDesc.value.type = ParmType::eString;

				// TODO:
				//  [ ] Set proper default
				//  [ ] Check for subtype (filepath, dirpath or name)
				attrDesc.value.defString = "";
			}
			else if (attrType == "BRDF") {
				attrDesc.value.type = ParmType::ePlugin;
				socketType = VOP_TYPE_BSDF;
			}
			else if (attrType == "UVWGEN") {
				attrDesc.value.type = ParmType::ePlugin;
				socketType = VOP_TYPE_VECTOR;
			}
			else if (attrType == "PLUGIN") {
				attrDesc.value.type = ParmType::ePlugin;
				socketType = VOP_TYPE_VOID;
			}
			else if (attrType == "OUTPUT_PLUGIN") {
				attrDesc.value.type = ParmType::eOutputPlugin;
				socketType = VOP_TYPE_VOID;
			}
			else if (attrType == "OUTPUT_COLOR") {
				attrDesc.value.type = ParmType::eOutputColor;
				socketType = VOP_TYPE_COLOR;
			}
			else if (attrType == "OUTPUT_TEXTURE") {
				attrDesc.value.type = ParmType::eOutputTextureColor;
				socketType = VOP_TYPE_COLOR;
			}
			else if (attrType == "OUTPUT_FLOAT_TEXTURE") {
				attrDesc.value.type = ParmType::eOutputTextureFloat;
				socketType = VOP_TYPE_FLOAT;
			}
			else if (attrType == "OUTPUT_INT_TEXTURE") {
				attrDesc.value.type = ParmType::eOutputTextureInt;
				socketType = VOP_TYPE_INTEGER;
			}
			else if (attrType == "OUTPUT_VECTOR_TEXTURE") {
				attrDesc.value.type = ParmType::eOutputTextureVector;
				socketType = VOP_TYPE_VECTOR;
			}
			else if (attrType == "OUTPUT_MATRIX_TEXTURE") {
				attrDesc.value.type = ParmType::eOutputTextureMatrix;
				socketType = VOP_TYPE_MATRIX3;
			}
			else if (attrType == "OUTPUT_TRANSFORM_TEXTURE") {
				attrDesc.value.type = ParmType::eOutputTextureTransform;
				socketType = VOP_TYPE_MATRIX4;
			}
			else if (attrType == "WIDGET_RAMP") {
				attrDesc.value.type = ParmType::eRamp;

				const auto &rampDesc = v.second.get_child("attrs");
				if (rampDesc.count("colors")) {
					attrDesc.value.defRamp.colors = rampDesc.get_child("colors").data();
				}
				if (rampDesc.count("positions")) {
					attrDesc.value.defRamp.positions = rampDesc.get_child("positions").data();
				}
				if (rampDesc.count("interpolations")) {
					attrDesc.value.defRamp.interpolations = rampDesc.get_child("interpolations").data();
				}
			}
			else if (attrType == "WIDGET_CURVE") {
				attrDesc.value.type = ParmType::eCurve;

				const auto &curveDesc = v.second.get_child("attrs");
				if (curveDesc.count("values")) {
					attrDesc.value.defCurve.values = curveDesc.get_child("values").data();
				}
				if (curveDesc.count("positions")) {
					attrDesc.value.defCurve.positions = curveDesc.get_child("positions").data();
				}
				if (curveDesc.count("interpolations")) {
					attrDesc.value.defCurve.interpolations = curveDesc.get_child("interpolations").data();
				}
			}

			PRM_Name socketName(attrDesc.attr.c_str(), attrDesc.label.c_str());
			if (MAPPABLE_TYPE(attrType)) {
#if CGR_DEBUG_JSON_ATTR_GEN
				PRINT_INFO("  Adding Input socket: \"%s\"...",
						   attrName.c_str());
#endif
				pluginInfo->inputs.push_back(SocketDesc(socketName, socketType, attrDesc.value.type));
			}
			else if (OUTPUT_TYPE(attrType)) {
#if CGR_DEBUG_JSON_ATTR_GEN
				PRINT_INFO("  Adding Output socket: \"%s\"...",
						   attrName.c_str());
#endif
				pluginInfo->outputs.push_back(SocketDesc(socketName, socketType, attrDesc.value.type));
			}
		}

		// Add specific attributes
		//
		if (pluginID == "TexFalloff") {
			AttrDesc &curveAttrDesc = pluginInfo->attributes["curve"];
			curveAttrDesc.attr  = "curve";
			curveAttrDesc.label = "Falloff Curve";
			curveAttrDesc.value.type = ParmType::eCurve;
		}
		else if (pluginID == "TexGradRamp" || pluginID == "TexRemap") {
			AttrDesc &rampAttrDesc = pluginInfo->attributes["ramp"];
			rampAttrDesc.attr  = "ramp";
			rampAttrDesc.label = "Color Ramp";
			rampAttrDesc.value.type = ParmType::eRamp;
		}
		else if (pluginID == "BRDFLayered") {
			VOP::BRDFLayered::AddAttributes(pluginInfo);
		}
		else if (pluginID == "TexLayered") {
			VOP::TexLayered::AddAttributes(pluginInfo);
		}
		else if (pluginID == "MtlMulti") {
			VOP::MtlMulti::AddAttributes(pluginInfo);
		}
		else if (pluginID == "GeomPlane") {
			SOP::GeomPlane::AddAttributes(pluginInfo);
		}
		else if (pluginID == "CustomTextureOutput") {
			VOP::TextureOutput::AddAttributes(pluginInfo);
		}
		else if (pluginID == "LightDome") {
			OBJ::LightDome::AddAttributes(pluginInfo);
		}

		// Parse widget and extract parameter active state
		// information
		if (jsonPluginDesc->count("Widget")) {
			// For widget
			for (const auto &w : jsonPluginDesc->get_child("Widget")) {
				// For widget item
				for (const auto &item : w.second.get_child("")) {
					const UI::StateInfo &parentState = ActiveStateGetStateInfo(pluginID, item, usePrefix);

					// If layout
					if (item.second.count("attrs") || item.second.count("splits")) {
						// If sub-layout
						if (item.second.count("splits")) {
							for (const auto &sp : item.second.get_child("splits")) {
								ActiveStateProcessWidgetLayout(pluginID, pluginInfo, parentState, sp, usePrefix);
							}
						}
						else {
							ActiveStateProcessWidgetLayout(pluginID, pluginInfo, parentState, item, usePrefix);
						}
					}
				}
			}
		}

#if CGR_DEBUG_JSON_ATTR_GEN
		PRINT_INFO("Generating PRM_Template [%i] list for \"%s\":",
				   pluginInfo->attributes.size(), pluginID.c_str());
#endif
		// NOTE:
		//   Widget is optimized for Blender ATM,
		//   thus all texturable params are missing.
		//   Modify widgets to specify that attribute
		//   is texturable and skip it if needed.
		//
#define CGR_USE_WIDGET 0
#if CGR_USE_WIDGET
		// Parse widget and extract parameter order
		// Since all parameters in Houdini are inserted in one column,
		// simply insert items in the order defined in widget
		if (jsonPluginDesc->count("Widget")) {
			// For widget
			for (const auto &w : jsonPluginDesc->get_child("Widget")) {
				// For widget item
				for (const auto &item : w.second.get_child("")) {
					// If layout
					if (item.second.count("attrs") || item.second.count("splits")) {
						hasWidget = true;

						// If sub-layout
						if (item.second.count("splits")) {
							for (const auto &sp : item.second.get_child("splits")) {
								AddAttrsTemplate(pluginInfo, sp, i);
							}
						}
						else {
							AddAttrsTemplate(pluginInfo, item, i);
						}
					}
				}
			}
		}
#else
		for (const auto &aIt : pluginInfo->attributes) {
			const AttrDesc &attrDesc = aIt.second;

			if (NOT(AttrNeedWidget(attrDesc))) {
				continue;
			}

			pluginInfo->prm_template.push_back(AttrDescAsPrmTemplate(attrDesc));
		}
#endif
		pluginInfo->prm_template.push_back(PRM_Template()); // List terminator
	}

#if 0
	UI::ActiveDependencies::showDependencies();
#endif

	return &pluginInfo->prm_template[0];
}
