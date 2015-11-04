//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_prm_def.h"
#include "vfh_prm_json.h"
#include "vfh_prm_globals.h"
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
#include <boost/format.hpp>

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

	VfhDisableCopy(JsonPluginDescGenerator)
} JsonPluginInfoParser;


void JsonPluginDescGenerator::init()
{
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

	RenderGIPlugins.insert("SettingsGI");
	RenderGIPlugins.insert("SettingsLightCache");
	RenderGIPlugins.insert("SettingsIrradianceMap");
	RenderGIPlugins.insert("SettingsDMCGI");
}


void JsonPluginDescGenerator::parseData()
{
	Log::getLog().info("Parse plugin description data...");

	const char *jsonDescsFilepath = getenv("VRAY_PLUGIN_DESC_PATH");
	if (NOT(jsonDescsFilepath)) {
		Log::getLog().error("VRAY_PLUGIN_DESC_PATH environment variable is not found!");
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
				Log::getLog().error("Error parsing %s",
							fileName.c_str());
			}
		}
	}

	if (NOT(parsedData.size())) {
		Log::getLog().error("No descriptions parsed! May be VRAY_PLUGIN_DESC_PATH points to an empty / incorrect directory?");
	}
}


JsonTree* JsonPluginDescGenerator::getTree(const std::string &pluginID)
{
	return parsedData.count(pluginID) ? &parsedData[pluginID] : nullptr;
}


void JsonPluginDescGenerator::freeData()
{
	parsedData.clear();
}


static void makeSpaceSeparatedTitleCase(std::string &attrName)
{
	// Title case and "_" to space
	for (int i = 0; i < attrName.length(); ++i) {
		const char &c = attrName[i];
		if (!(c >= 'A' && c <= 'Z')) {
			if ((i == 0) || (attrName[i-1] == '_')) {
				if (i) {
					attrName[i-1] = ' ';
				}
				// Don't modify digits
				if (!(c >= '0' && c <= '9')) {
					attrName[i] -= 32;
				}
			}
		}
	}
}


static bool AttrNeedWidget(const AttrDesc &attrDesc)
{
	if (attrDesc.value.type < ParmType::ePlugin) {
		return true;
	}
	return false;
}


static PRM_Template AttrDescAsPrmTemplate(const AttrDesc &attrDesc, const std::string &prefix)
{
	// PRM_Name stores pointer to name so we have to store generated names
	//
	struct PrmNameInfo {
		PrmNameInfo(const std::string &name, const std::string &label)
			: m_name(name.c_str())
			, m_label(label.c_str())
			, m_prm(PRM_Name(m_name.ptr(), m_label.ptr()))
		{}

		PRM_Name           *getPrm() { return &m_prm; }

	private:
		VUtils::CharString  m_name;
		VUtils::CharString  m_label;
		PRM_Name            m_prm;

		VfhDisableCopy(PrmNameInfo);
	};

	static struct PrmNameInfos {
		PrmNameInfos() {}
		~PrmNameInfos() {
			for (const auto &prmNamePtr : m_prmNames) {
				delete prmNamePtr;
			}
		}

		PrmNameInfo &add(PrmNameInfo *prmNameInfo) {
			m_prmNames.push_back(prmNameInfo);
			return *m_prmNames.back();
		}

	private:
		std::vector<PrmNameInfo*> m_prmNames;

		VfhDisableCopy(PrmNameInfos);
	} prmNameInfos;

	const std::string &attrName = prefix.empty()
								  ? attrDesc.attr
								  : boost::str(Parm::FmtPrefixAuto % prefix % attrDesc.attr);

	PrmNameInfo &prmName = prmNameInfos.add(new PrmNameInfo(attrName, attrDesc.label));

	PRM_Template tmpl;
	if (attrDesc.value.type == ParmType::eBool) {
		tmpl = PRM_Template(PRM_TOGGLE, 1, prmName.getPrm(), attrDesc.value.getDefBool());
	}
	else if (attrDesc.value.type == ParmType::eInt ||
			 attrDesc.value.type == ParmType::eTextureInt) {
		tmpl = PRM_Template(PRM_INT, 1, prmName.getPrm(), attrDesc.value.getDefInt());
	}
	else if (attrDesc.value.type == ParmType::eFloat ||
			 attrDesc.value.type == ParmType::eTextureFloat) {
		tmpl = PRM_Template(PRM_FLT, 1, prmName.getPrm(), attrDesc.value.getDefFloat());
	}
	else if (attrDesc.value.type == ParmType::eColor ||
			 attrDesc.value.type == ParmType::eAColor ||
			 attrDesc.value.type == ParmType::eTextureColor) {
		tmpl = PRM_Template(PRM_RGB_J, PRM_Template::PRM_EXPORT_TBX, 4, prmName.getPrm(), attrDesc.value.getDefColor());
	}
	else if (attrDesc.value.type == ParmType::eEnum) {
		PRM_Name *enumMenuItems = new PRM_Name[attrDesc.value.defEnumItems.size()+1];
		int i = 0;
		for (const auto &item : attrDesc.value.defEnumItems) {
			enumMenuItems[i++] = PRM_Name(item.label.c_str());
		}
		enumMenuItems[i] = PRM_Name(); // Items terminator

		PRM_ChoiceList *enumMenu = new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, enumMenuItems);

		tmpl = PRM_Template(PRM_ORD, 1, prmName.getPrm(), PRMzeroDefaults, enumMenu);
	}
	else if (attrDesc.value.type == ParmType::eString) {
		// TODO:
		//  [ ] Proper default value
		//  [ ] Template type base on subtype (filepath, dirpath or name)
		//
		tmpl = PRM_Template(PRM_FILE, 1, prmName.getPrm(), &PRMemptyStringDefault);
	}
	else if (attrDesc.value.type == ParmType::eCurve) {
		tmpl = PRM_Template(PRM_MULTITYPE_RAMP_FLT,  NULL /*tmpl*/, 1 /*vec_size*/, prmName.getPrm(), PRMtwoDefaults, NULL /*range*/, &Parm::PRMcurveDefault);
	}
	else if (attrDesc.value.type == ParmType::eRamp) {
		tmpl = PRM_Template(PRM_MULTITYPE_RAMP_RGB,  NULL /*tmpl*/, 1 /*vec_size*/, prmName.getPrm(), PRMtwoDefaults, NULL /*range*/, &Parm::RPMrampDefault);
	}

	if (tmpl.getType() == PRM_LIST_TERMINATOR) {
		Log::getLog().error("Incorrect attribute template: %s (%s)",
					prmName.getPrm()->getToken(), prmName.getPrm()->getLabel());
	}

	return tmpl;
}


static UI::StateInfo ActiveStateGetStateInfo(const JsonItem &item)
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

		if (conditionDesc.count("at")) {
			stateInfo.conditionPlugin = conditionDesc.get_child("at").data();
		}

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
			else if (cond_type == "in") {
				stateInfo.condition = UI::StateInfo::CondIn;
			}
			else if (cond_type == "not_in") {
				stateInfo.condition = UI::StateInfo::CondNotIn;
			}

			try {
				if (stateInfo.condition == UI::StateInfo::CondIn ||
					stateInfo.condition == UI::StateInfo::CondNotIn) {

					for (const auto &item : conditionDesc.get_child("value")) {
						stateInfo.conditionValues.push_back(item.second.get_value<int>());
					}
				}
				else {
					stateInfo.conditionValue = conditionDesc.get<int>("value");
				}
			}
			catch (...) {
				stateInfo.conditionValue = 0;
			}
		}
	}

	return stateInfo;
}


static void ActiveStateProcessWidgetLayout(const std::string &pluginID, VRayPluginInfo *nodeType, const UI::StateInfo &parentState, const JsonItem &item)
{
	bool debug = false; // pluginID == "SettingsGI";

	// Current layout could have different active property
	const UI::StateInfo &currenLayoutStateInfo = ActiveStateGetStateInfo(item);

	for (const auto &at : item.second.get_child("attrs")) {
		if (at.second.count("name")) {
			const std::string &attrName = at.second.get_child("name").data();
			if (!attrName.empty() && nodeType->attributes.count(attrName)) {
				const AttrDesc &attrDesc = nodeType->attributes[attrName];

				if (AttrNeedWidget(attrDesc)) {
					// Parent layout state
					if (parentState) {
						UI::ActiveStateDeps::addStateInfo(pluginID, attrName, parentState);

						if (debug) {
							Log::getLog().info("Parent active layout property: \"%s\" => \"%s\"",
									   parentState.conditionAttr.c_str(), attrName.c_str());
						}
					}

					if (currenLayoutStateInfo) {
						UI::ActiveStateDeps::addStateInfo(pluginID, attrName, currenLayoutStateInfo);

						if (debug) {
							Log::getLog().info("Active attr property: \"%s\" => \"%s\"",
									   currenLayoutStateInfo.conditionAttr.c_str(), attrName.c_str());
						}
					}
				}
			}
		}
	}
}


VRayPluginInfo* Parm::generatePluginInfo(const std::string &pluginID)
{
	if (!JsonPluginInfoParser.hasData()) {
		JsonPluginInfoParser.init();
		JsonPluginInfoParser.parseData();
	}

	VRayPluginInfo *pluginInfo = nullptr;

	const JsonTree *jsonPluginDesc = JsonPluginInfoParser.getTree(pluginID);
	if (jsonPluginDesc) {
		const std::string &pluginTypeStr = jsonPluginDesc->get_child("Type").data();

		Parm::PluginType pluginType = Parm::PluginTypeUnknown;

		if (pluginTypeStr == "BRDF") {
			pluginType = Parm::PluginTypeBRDF;
		}
		else if (pluginTypeStr == "MATERIAL") {
			pluginType = Parm::PluginTypeMaterial;
		}
		else if (pluginTypeStr == "UVWGEN") {
			pluginType = Parm::PluginTypeUvwgen;
		}
		else if (pluginTypeStr == "TEXTURE") {
			pluginType = Parm::PluginTypeTexture;
		}
		else if (pluginTypeStr == "GEOMETRY") {
			pluginType = Parm::PluginTypeGeometry;
		}
		else if (pluginTypeStr == "RENDERCHANNEL") {
			pluginType = Parm::PluginTypeRenderChannel;
		}
		else if (pluginTypeStr == "EFFECT") {
			pluginType = Parm::PluginTypeEffect;
		}
		else if (pluginTypeStr == "SETTINGS") {
			pluginType = Parm::PluginTypeSettings;
		}
		else if (pluginTypeStr == "LIGHT") {
			pluginType = Parm::PluginTypeLight;
		}

		pluginInfo = Parm::NewVRayPluginInfo(pluginID);
		pluginInfo->pluginType = pluginType;

		// Create default outputs
		switch (pluginType) {
			case Parm::PluginTypeBRDF: {
				pluginInfo->outputs.push_back(SocketDesc(PRM_Name("BRDF", "BRDF"), VOP_TYPE_BSDF));
				break;
			}
			case Parm::PluginTypeMaterial: {
				pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Material", "Material"), VOP_SURFACE_SHADER));
				break;
			}
			case Parm::PluginTypeUvwgen: {
				pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Mapping", "Mapping"), VOP_TYPE_VECTOR));
				break;
			}
			case Parm::PluginTypeTexture: {
				if (pluginID == "BitmapBuffer") {
					pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Bitmap", "Bitmap"), VOP_TYPE_VOID));
				}
				else {
					pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Output", "Output"), VOP_TYPE_COLOR));
				}
				break;
			}
			case Parm::PluginTypeGeometry: {
				pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Geometry", "Geometry"), VOP_GEOMETRY_SHADER));
				break;
			}
			case Parm::PluginTypeRenderChannel: {
				if (pluginID != "SettingsRenderChannels") {
					pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Channel", "Channel"), VOP_TYPE_VOID));
				}
				break;
			}
			case Parm::PluginTypeEffect: {
				if (pluginID == "PhxShaderCache") {
					pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Cache", "Cache"), VOP_TYPE_VOID));
				}
				else {
					pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Effect", "Effect"), VOP_TYPE_VOID));
				}
				break;
			}
		}

		for (const auto &v : jsonPluginDesc->get_child("Parameters")) {
			const std::string &attrName = v.second.get_child("attr").data();
			const std::string &attrType = v.second.get_child("type").data();

			if (SKIP_TYPE(attrType)) {
				continue;
			}

			if (v.second.count("available")) {
				std::set<std::string> availableIn;
				for (const auto &item : v.second.get_child("available")) {
					availableIn.insert(item.second.get_value<std::string>(""));
				}
				if (!availableIn.count("HOUDINI")) {
					continue;
				}
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
				makeSpaceSeparatedTitleCase(attrDesc.label);
			}

			if (v.second.count("options")) {
				for (const auto &item : v.second.get_child("options")) {
					const std::string &option = item.second.get_value<std::string>("");
					if (option == "LINKED_ONLY") {
						attrDesc.linked_only = true;
					}
				}
			}

			attrDesc.attr = attrName;
			attrDesc.desc = v.second.get_child("desc").data();

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
				pluginInfo->inputs.push_back(SocketDesc(socketName, socketType, attrDesc.value.type));
			}
			else if (OUTPUT_TYPE(attrType)) {
				pluginInfo->outputs.push_back(SocketDesc(socketName, socketType, attrDesc.value.type));
			}
		}

		// Add specific attributes
		//
		// TODO: Add WIDGET_CURVE to JSON desc
		if (pluginID == "TexFalloff") {
			AttrDesc &curveAttrDesc = pluginInfo->attributes["curve"];
			curveAttrDesc.attr  = "curve";
			curveAttrDesc.label = "Falloff Curve";
			curveAttrDesc.value.type = ParmType::eCurve;
		}

		// Parse widget and extract parameter active state
		// information
		if (jsonPluginDesc->count("Widget")) {
			// For widget
			for (const auto &w : jsonPluginDesc->get_child("Widget")) {
				// For widget item
				for (const auto &item : w.second.get_child("")) {
					const UI::StateInfo &parentState = ActiveStateGetStateInfo(item);

					// If layout
					if (item.second.count("attrs")) {
						ActiveStateProcessWidgetLayout(pluginID, pluginInfo, parentState, item);
					}

					// If sub-layout
					if (item.second.count("splits")) {
						for (const auto &sp : item.second.get_child("splits")) {
							ActiveStateProcessWidgetLayout(pluginID, pluginInfo, parentState, sp);
						}
					}
				}
			}
		}
	}

	return pluginInfo;
}


PRMTmplList* Parm::generatePrmTemplate(const std::string &pluginID, const std::string &prefix)
{
	typedef std::map<std::string, PRMTmplList> PrmTemplatesMap;
	static PrmTemplatesMap PrmTemplates;

	PRMTmplList *prmTmplList = nullptr;

	VRayPluginInfo *pluginInfo = Parm::GetVRayPluginInfo(pluginID);
	if (!pluginInfo) {
		pluginInfo = generatePluginInfo(pluginID);
	}
	if (pluginInfo) {
		const std::string &tmplPluginID = prefix.empty()
										  ? boost::str(Parm::FmtPrefixAuto % prefix % pluginID)
										  : pluginID;

		if (PrmTemplates.count(tmplPluginID)) {
			prmTmplList = &PrmTemplates[tmplPluginID];
		}
		else {
			prmTmplList = &PrmTemplates[tmplPluginID];

			if (pluginID == "BRDFLayered") {
				VOP::BRDFLayered::addPrmTemplate(*prmTmplList);
			}
			else if (pluginID == "TexLayered") {
				VOP::TexLayered::addPrmTemplate(*prmTmplList);
			}
			else if (pluginID == "MtlMulti") {
				VOP::MtlMulti::addPrmTemplate(*prmTmplList);
			}
			else if (pluginID == "GeomPlane") {
				SOP::GeomPlane::addPrmTemplate(*prmTmplList);
			}
			else if (pluginID == "GeomMeshFile") {
				SOP::VRayProxy::addPrmTemplate(*prmTmplList);
			}
			else if (pluginID == "CustomTextureOutput") {
				VOP::TextureOutput::addPrmTemplate(*prmTmplList);
			}
			else if (pluginID == "LightDome") {
				OBJ::LightDome::addPrmTemplate(*prmTmplList);
			}

			for (const auto &aIt : pluginInfo->attributes) {
				const AttrDesc &attrDesc = aIt.second;
				if (AttrNeedWidget(attrDesc)) {
					prmTmplList->push_back(AttrDescAsPrmTemplate(attrDesc, prefix));
				}
			}

			prmTmplList->push_back(PRM_Template()); // List terminator
		}
	}

	return prmTmplList;
}


PRM_Template* Parm::getPrmTemplate(const std::string &pluginID)
{
	PRM_Template *prmTmpl = nullptr;

	PRMTmplList *prmTmplList = generatePrmTemplate(pluginID);
	if (prmTmplList) {
		prmTmpl = &(*prmTmplList)[0];
	}

	return prmTmpl;
}
