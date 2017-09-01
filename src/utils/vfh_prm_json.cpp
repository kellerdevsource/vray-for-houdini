//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
#include "vfh_log.h"

#include <QDirIterator>

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

	/// Get a json object for given pluginID
	/// @pluginID - the requested plugin's ID
	/// @return pointer - to the json describing the plugin
	///         nullptr - json not found for given plugin ID
	JsonTree *getTree(const std::string &pluginID);

	/// Check if we already loaded data from files
	bool hasData() { return parsedData.size(); }
	/// Load data from json files
	void parseData();
	/// Free all loaded data
	void freeData();

private:
	JsonDescs  parsedData; ///< Json objects for all plugins

	VfhDisableCopy(JsonPluginDescGenerator)
} JsonPluginInfoParser;

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


VRayPluginInfo* Parm::generatePluginInfo(const std::string &pluginID)
{
	if (!JsonPluginInfoParser.hasData()) {
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
				if (pluginID == "TexColorToFloat") {
					pluginInfo->outputs.push_back(SocketDesc(PRM_Name("Output", "Output"), VOP_TYPE_FLOAT));
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
				else if (pluginID == "PhxShaderSim") {
					pluginInfo->outputs.push_back(SocketDesc(PRM_Name("PhxShaderSim", "PhxShaderSim"), VOP_ATMOSPHERE_SHADER));
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
					else if (option == "EXPORT_AS_RADIANS") {
						attrDesc.convert_to_radians = true;
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
			else if (attrType == "VECTOR") {
				attrDesc.value.type = ParmType::eVector;

				int c = 0;
				for (const auto &_v : v.second.get_child("default")) {
					attrDesc.value.defColor[c++] = _v.second.get<float>("");
				}

				socketType = VOP_TYPE_VECTOR;
			}
			else if (attrType == "MATRIX") {
				attrDesc.value.type = ParmType::eMatrix;

				int c = 0;
				for (const auto &_v : v.second.get_child("default")) {
					attrDesc.value.defMatrix[c++] = _v.second.get<float>("");
				}

				socketType = VOP_TYPE_MATRIX3;
			}
			else if (attrType == "TRANSFORM") {
				attrDesc.value.type = ParmType::eTransform;

				int c = 0;
				for (const auto &_v : v.second.get_child("default")) {
					attrDesc.value.defMatrix[c++] = _v.second.get<float>("");
				}

				socketType = VOP_TYPE_MATRIX4;
			}
			else if (attrType == "BRDF") {
				attrDesc.value.type = ParmType::ePlugin;
				socketType = VOP_TYPE_BSDF;
			}
			else if (attrType == "MATERIAL") {
				attrDesc.value.type = ParmType::ePlugin;
				socketType = VOP_SURFACE_SHADER;
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

	}

	return pluginInfo;
}
