//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_node_osl.h"
#include "getenvvars.h"

#include <QTemporaryFile>
#include <QDir>

#include <OSL/oslconfig.h>
#include <OSL/oslcomp.h>
#include <OSL/oslquery.h>

// OIIO
#include <OpenImageIO/errorhandler.h>
#include <OpenImageIO/typedesc.h>

// Template specializations neet to be implemented in the correct namespace!
namespace VRayForHoudini
{
namespace VOP
{
template <>
VOP_Type OSLNodeBase<true>::getOutputType() const
{
	return VOP_TYPE_BSDF;
}

template <>
VOP_Type OSLNodeBase<false>::getOutputType() const
{
	return VOP_TYPE_COLOR;
}


template <>
void OSLNodeBase<false>::setPluginType()
{
	pluginType = VRayPluginType::TEXTURE;
	pluginID = "TexOSL";
}

template <>
void OSLNodeBase<true>::setPluginType()
{
	pluginType = VRayPluginType::MATERIAL;
	pluginID = "MtlOSL";
}


} // namespace VOP
} // namespace VRayForHoudini



using namespace VRayForHoudini;
using namespace VOP;

namespace {

class OSLErrorHandle: public VRayOpenImageIO::ErrorHandler
{
public:
	OSLErrorHandle()
	{}
	virtual ~OSLErrorHandle() VRAY_OVERRIDE
	{}

	virtual void operator () (int errcode, const std::string &msg) VRAY_OVERRIDE
	{
		const ErrCode code = static_cast<ErrCode>((0xff << 16) & errcode); // code is in high 16 bits
		const int errorNo = ~(0xff << 16) & errcode;
		const char * errFormat = "VRayOSL Error (%d): %s";

		switch (code) {
		case EH_INFO:
			Log::getLog().info(errFormat, errorNo, msg.c_str());
			break;
		case EH_WARNING:
			Log::getLog().warning(errFormat, errorNo, msg.c_str());
			break;
		case EH_ERROR:
		case EH_SEVERE:
			Log::getLog().error(errFormat, errorNo, msg.c_str());
			break;
		case EH_DEBUG:
			Log::getLog().debug(errFormat, errorNo, msg.c_str());
			break;
		}
	}
} staticErrHandle;

/// All osl params in OSL multiparam
const std::string OSL_PARAM_TYPE_LIST[] = {
	"color_param",
	"vector_param",
	"float_param",
	"int_param",
	"string_param",
	"bool_param",
	"menu_param",
};

/// Env var reader for APPSDK_PATH
VUtils::GetEnvVar APPSDK_PATH("VRAY_APPSDK", "");
/// We ship the stdosl.h that comes with appsdk
const std::string stdOslPath = APPSDK_PATH.getValue().ptr() + std::string("/bin/stdosl.h");

const int OSL_PARAM_TYPE_COUNT = sizeof(OSL_PARAM_TYPE_LIST) / sizeof(OSL_PARAM_TYPE_LIST[0]);

template <bool MTL>
std::string mapTypeToParam(const typename OSLNodeBase<MTL>::ParamInfo & info)
{
	// keep in-sync with OSL_PARAM_TYPE_LIST and vfh_osl_base.ds
	switch (info.type) {
		case VOP_TYPE_COLOR:
			return "color_param";
		case VOP_TYPE_VECTOR:
		case VOP_TYPE_POINT:
		case VOP_TYPE_NORMAL:
			return "vector_param";
		case VOP_TYPE_INTEGER:
			if (info.widget== OSLNodeBase<MTL>::ParamInfo::Menu) {
				return "menu_param";
			} else if (info.widget == OSLNodeBase<MTL>::ParamInfo::Checkbox) {
				return "bool_param";
			} else {
				return "int_param";
			}
		case VOP_TYPE_FLOAT:
			return "float_param";
		case VOP_TYPE_STRING:
			return "string_param";
		default:
			return "";
	}
}

typedef std::vector<VRayOSL::OSLQuery::Parameter> param_list;
typedef param_list::const_iterator param_iter;

param_iter findMetaParam(const param_list & list, const char * key)
{
	return std::find_if(list.begin(), list.end(), [key](const VRayOSL::OSLQuery::Parameter & param) {
		return !stricmp(key, param.name.c_str());
	});
}

template <bool MTL>
void parseMetadata(const VRayOSL::OSLQuery::Parameter *param, typename OSLNodeBase<MTL>::ParamInfo & info)
{
	using namespace OIIO;
	using namespace VRayOSL;
	using namespace std;

	using Widget = typename OSLNodeBase<MTL>::ParamInfo::WidgetType;
	info.widget = Widget::Unspecified;

	auto notFound = param->metadata.end();

	auto widgetItem = findMetaParam(param->metadata, "widget");
	if (widgetItem != notFound) {
		if (   param->type.aggregate == TypeDesc::SCALAR
			&& param->type.basetype == TypeDesc::INT
			&& !stricmp(widgetItem->sdefault[0].c_str(), "mapper")) {

			// mapper needs options string which is in this format:  key1:value1|key2:value2|key3:value3
			auto mapperItem = findMetaParam(param->metadata, "options");
			if (mapperItem != notFound) {
				// only set menu type here, so we can display options
				info.widget = Widget::Menu;
				// save the string default, which is the formatted string with keys and values
				info.stringDefault = mapperItem->sdefault[0];
			}
		} else if (!stricmp(widgetItem->sdefault[0].c_str(), "checkBox")) {
			info.widget = Widget::Checkbox;
		} else if (!stricmp(widgetItem->sdefault[0].c_str(), "string")) {
			info.widget = Widget::String;
		}
	}
}

} // namespace

template <bool MTL>
void OSLNodeBase<MTL>::getOSLCode(UT_String & oslCode, bool &needCompile) const
{
	UT_String oslSourceLocation;
	evalString(oslSourceLocation, "osl_source", 0, 0.f);
	if (oslSourceLocation == "OSL") {
		evalString(oslCode, "osl_code", 0, 0.f);
		needCompile = true;
	} else {
		UT_String filePath;
		evalString(filePath, "osl_file", 0, 0.f);
		if (filePath == "" || filePath.buffer() == nullptr) {
			return;
		}

		needCompile = filePath.endsWith(".osl");

		// TODO: is it worth it to cache path + last change time
		QFile oslFile(filePath.nonNullBuffer());
		if (!oslFile.open(QIODevice::ReadOnly)) {
			Log::getLog().error("Failed to open \"%s\" selected as osl source file", filePath.nonNullBuffer());
		} else {
			QByteArray data = oslFile.readAll();
			oslFile.close();

			if (!data.size()) {
				Log::getLog().error("Failed to read \"%s\" selected as osl source file", filePath.nonNullBuffer());
			} else {
				oslCode = UT_String(data.constData(), true, data.size());
			}
		}
	}
}

template <bool MTL>
void OSLNodeBase<MTL>::updateParamsIfNeeded() const
{
	using namespace Hash;

	UT_String oslCode;
	bool needCompile = false;
	getOSLCode(oslCode, needCompile);

	auto * self = SYSconst_cast(this);
	if (oslCode.length() <= 0) {
		self->m_codeHash = 0;
		return;
	}

	MHash sourceHash;
	MurmurHash3_x86_32(oslCode.buffer(), oslCode.length(), 42, &sourceHash);

	// nothing changed since last run
	if (sourceHash == m_codeHash) {
		return;
	}

	self->m_codeHash = 0;
	self->m_paramList.clear();
	self->m_inputList.clear();
	self->m_outputName = "";
	self->m_outputNameBuff[0] = 0;

	// not error, just empty code
	if (oslCode.length() == 0) {
		return;
	}

	using namespace OIIO;
	using namespace VRayOSL;

	std::string osoCode;
	if (needCompile) {
		OSLCompiler * compiler = OSLCompiler::create();
		OSLCompilerInput settings;
		settings.inputMode = OSL_MEMORY_BUFFER;
		settings.outputMode = OSL_MEMORY_BUFFER;
		settings.buffer = oslCode.toStdString();
		settings.stdoslpath = stdOslPath;
		settings.errorHandler = &staticErrHandle;

		if (!compiler->compile(settings)) {
			Log::getLog().error("Failed to compile OSL source.");
			return;
		}

		const int size = compiler->get_compiled_shader_code(nullptr, 0);
		osoCode.resize(size + 1, ' ');
		const int written = compiler->get_compiled_shader_code(&osoCode[0], size + 1);
		UT_ASSERT_MSG(written == size, "Subsequent calls to get_compiled_shader_code return different size");
	} else {
		osoCode = oslCode.toStdString();
	}

	OSLQuery query;
	if (!query.load(osoCode, &staticErrHandle)) {
		Log::getLog().error("Failed to query OSL parameters.");
		return;
	}

	for (int c = 0; c < query.nparams(); c++) {
		const OSLQuery::Parameter *param = query.getparam(c);
		ParamInfo info = {param->name, VOP_TYPE_UNDEF};
		info.validDefault = param->validdefault;
		parseMetadata<MTL>(param, info);

		if (param->isoutput) {
			if (param->isclosure) {
				if (MTL) {
					self->m_outputName = param->name;
				} else {
					Log::getLog().warning("TexOSL \"%s\" does not support closure color as output parameter!", this->getName().nonNullBuffer());
				}
			} else {

				if (param->type.vecsemantics == TypeDesc::COLOR) {
					self->m_outputName = param->name;
				} else {
					Log::getLog().warning("Output %s is n");
				}
			}
			continue;
		}

		switch (param->type.vecsemantics) {
		case TypeDesc::COLOR:
			info.type = VOP_TYPE_COLOR;
			break;
		case TypeDesc::POINT:
			info.type = VOP_TYPE_POINT;
			break;
		case TypeDesc::VECTOR:
			info.type = VOP_TYPE_VECTOR;
			break;
		case TypeDesc::NORMAL:
			info.type = VOP_TYPE_NORMAL;
			break;
		}

		if (param->validdefault) {
			if (info.type != VOP_TYPE_UNDEF) {
				// its one of (color, point, vector, normal)
				for (int r = 0; r < 3; r++) {
					info.numberDefault[r] = param->fdefault[r];
				}
			}
		}

		if (param->type.aggregate == TypeDesc::SCALAR) {
			if (param->type.basetype == TypeDesc::INT) {
				info.type = VOP_TYPE_INTEGER;
				if (param->validdefault) {
					info.numberDefault[0] = param->idefault[0];
				}
			} else if (param->type.basetype == TypeDesc::FLOAT) {
				info.type = VOP_TYPE_FLOAT;
				if (param->validdefault) {
					info.numberDefault[0] = param->fdefault[0];
				}
			} else if (param->type.basetype == TypeDesc::STRING) {
				// NOTE: strings are only for plugin inputs
				// TODO: VOP_TYPE_STRUCT ?
				info.type = VOP_TYPE_STRING;
				if (param->validdefault) {
					info.stringDefault = param->sdefault[0];
				}
				if (info.widget != ParamInfo::String) {
					// if the metadata type is string, this means it is not plugin input so don't add to inputs
					self->m_inputList.push_back(param->name);
				}
			}
		}

		if (info.type != VOP_TYPE_UNDEF) {
			self->m_paramList.push_back(info);
		}
	}

	if (MTL && m_outputName == "") {
		self->m_outputName = "Ci";
	}

	// TODO: find if there is way to query number of instances
	//       or use max(prevCount, currentCount)
	for (int c = 0; c < 100; c++) {
		self->removeMultiParmItem("osl_input_params", c);
	}

	// label and seperator
	const char * oslParams[OSL_PARAM_TYPE_COUNT + 2] = {
		"label",
		"separator"
	};
	const int oslParamCount = sizeof(oslParams) / sizeof(oslParams[0]);
	// add osl-code specific params
	for (int c = 0; c < OSL_PARAM_TYPE_COUNT; c++) {
		oslParams[c + 2] = OSL_PARAM_TYPE_LIST[c].c_str();
	}

	int paramIdx = 1;
	int inputIdx = 0;
	for (const ParamInfo & param : m_paramList) {
		// strings, that are not string widgets are inputs, so dont create params for them
		if (param.type == VOP_TYPE_UNDEF) {
			continue;
		}

		self->insertMultiParmItem("osl_input_params", paramIdx);

		// Hide everything for paramIdx
		for (int r = 0; r < oslParamCount; r++) {
			char paramName[256] = {0};
			// Example param: osl3color_param
			sprintf(paramName, "osl%d%s", paramIdx, oslParams[r]);
			if (!self->setVisibleState(paramName, false)) {
				Log::getLog().warning("Failed to hide %s", paramName);
			}
		}

		if (param.type == VOP_TYPE_STRING && param.widget != ParamInfo::String) {
			paramIdx++;
			// for string params that are not widget string, just leave param here
			// so input's value is put on correct index
			continue;
		}

		// Set the param name in string field becasue we cant change labels of params
		self->setStringInst(UT_String(param.name.c_str(), param.name.length()),
			CH_StringMeaning::CH_STRING_LITERAL, "osl#label", &paramIdx, 0, 0);

		// show only the type this param is
		const std::string & oslParamName = mapTypeToParam<MTL>(param);

		if (oslParamName != "") {
			char paramName[256] = {0};
			for (int f = 0; f < (oslParamCount - OSL_PARAM_TYPE_COUNT); f++) {
				// label and separator
				sprintf(paramName, "osl%d%s", paramIdx, oslParams[f]);
				if (!self->setVisibleState(paramName, true)) {
					Log::getLog().warning("Failed to show %s", paramName);
				}
			}


			// the appropriate param for the type
			sprintf(paramName, "osl%d%s", paramIdx, oslParamName.c_str());
			if (!self->setVisibleState(paramName, true)) {
				Log::getLog().warning("Failed to show %s", paramName);
			}

			if (param.validDefault) {
				switch (param.type) {
				case VOP_TYPE_COLOR:
				case VOP_TYPE_VECTOR:
				case VOP_TYPE_POINT:
				case VOP_TYPE_NORMAL:
					for (int r = 0; r < 3; r++) {
						self->setFloat(paramName, r, 0, param.numberDefault[r]);
					}
					break;
				case VOP_TYPE_INTEGER:
					if (param.widget == ParamInfo::Menu) {
						const std::string & menuParamItems = paramName + std::string("_items");
						// set osl#menu_param_items to the items string
						self->setString(UT_String(param.stringDefault.c_str(), true), CH_STRING_LITERAL, menuParamItems.c_str(), 0, 0);
					} else {
						self->setInt(paramName, 0, 0, param.numberDefault[0]);
					}
					break;
				case VOP_TYPE_FLOAT:
					self->setFloat(paramName, 0, 0, param.numberDefault[0]);
					break;
				case VOP_TYPE_STRING:
					// if metadata widget is String, this is not input
					if (param.widget == ParamInfo::String) {
						self->setString(UT_String(param.stringDefault.c_str(), true), CH_STRING_LITERAL, paramName, 0, 0);
					}
					break;
				}
			}
		}

		paramIdx++;
	}

	self->addOrRemoveMultiparmInstance();
	self->m_codeHash = sourceHash;
}

template <bool MTL>
const char * OSLNodeBase<MTL>::getOutputName() const
{
	strcpy(m_outputNameBuff, m_outputName.c_str());
	return m_outputNameBuff;
}

template <bool MTL>
const char * OSLNodeBase<MTL>::outputLabel(unsigned idx) const
{
	const int numBaseOut = NodeBase::getNumVisibleOutputs();
	if (idx < numBaseOut) {
		return NodeBase::inputLabel(idx);
	}

	updateParamsIfNeeded();
	if (m_outputName != "") {
		return OSLNodeBase<MTL>::getOutputName();
	} else {
		Log::getLog().warning("outputLabel(%d) out of range", idx);
	}
	return "UNDEFINED";
}

template <bool MTL>
const char* OSLNodeBase<MTL>::inputLabel(unsigned idx) const
{
	const int numBaseInputs = NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		return NodeBase::inputLabel(idx);
	}

	const int socketIndex = idx - numBaseInputs;
	updateParamsIfNeeded();
	if (socketIndex < m_inputList.size()) {
		return m_inputList[socketIndex].c_str();
	} else {
		Log::getLog().warning("inputLabel(%d [%d]) out of range", idx, socketIndex);
	}

	return "UNDEFINED";
}

template <bool MTL>
int OSLNodeBase<MTL>::getInputFromName(const UT_String &in) const
{
	return OSLNodeBase<MTL>::getInputFromNameSubclass(in);
}

template <bool MTL>
int	OSLNodeBase<MTL>::getOutputFromName(const UT_String &out) const
{
	updateParamsIfNeeded();
	if (out.equal(m_outputName)) {
		return OSLNodeBase<MTL>::getNumVisibleOutputs() + 0; // this is index so number of outputs before us is our index
	}
	return NodeBase::getOutputFromName(out);
}

template <bool MTL>
void OSLNodeBase<MTL>::getInputNameSubclass(UT_String &in, int idx) const
{
	const int numBaseInputs = NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		NodeBase::getInputNameSubclass(in, idx);
	} else {
		// name and label are the same
		in = OSLNodeBase<MTL>::inputLabel(idx);
	}
}

template <bool MTL>
void OSLNodeBase<MTL>::getOutputNameSubclass(UT_String &out, int idx) const
{
	if (idx >= NodeBase::getNumVisibleOutputs()) {
		updateParamsIfNeeded();
		if (m_outputName != "") {
			out = m_outputName;
		} else {
			Log::getLog().warning("Output index out of range");
		}
	} else {
		return NodeBase::getOutputNameSubclass(out, idx);
	}
}

template <bool MTL>
int OSLNodeBase<MTL>::getInputFromNameSubclass(const UT_String &in) const
{
	updateParamsIfNeeded();
	const int numBaseInputs = NodeBase::orderedInputs();
	for (int c = 0; c < m_inputList.size(); c++) {
		if (in.equal(m_inputList[c].c_str())) {
			return c + numBaseInputs;
		}
	}

	return NodeBase::getInputFromNameSubclass(in);
}

template <bool MTL>
void OSLNodeBase<MTL>::getInputTypeInfoSubclass(VOP_TypeInfo &typeInfo, int idx)
{
	const int numBaseInputs = NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		NodeBase::getInputTypeInfoSubclass(typeInfo, idx);
	} else {
		// VRay OSL supports string(plugin) but textures are VOP_TYPE_COLOR
		updateParamsIfNeeded();
		typeInfo.setType(VOP_TYPE_COLOR);
		const int sockIdx = idx - numBaseInputs;
		if (sockIdx >= customInputsCount()) {
			Log::getLog().warning("getInputTypeInfoSubclass(%d [%d]) out of range", idx, sockIdx);
		}

	}
}

template <bool MTL>
void OSLNodeBase<MTL>::getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	if (idx >= NodeBase::getNumVisibleOutputs()) {
		updateParamsIfNeeded();
		if (m_outputName != "") {
			type_info.setType(OSLNodeBase<MTL>::getOutputType());
		} else {
			Log::getLog().warning("Trying to get output type of non-existent output!");
		}
	} else {
		NodeBase::getOutputTypeInfoSubclass(type_info, idx);
	}
}

template <bool MTL>
void OSLNodeBase<MTL>::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &typeInfoList)
{
	const int numBaseInputs = NodeBase::orderedInputs();

	if (idx < numBaseInputs) {
		NodeBase::getAllowedInputTypeInfosSubclass(idx, typeInfoList);
	} else {
		VOP_TypeInfo typeInfo;
		OSLNodeBase<MTL>::getInputTypeInfoSubclass(typeInfo, idx);
		typeInfoList.append(typeInfo);
	}
}

template <bool MTL>
int OSLNodeBase<MTL>::customInputsCount() const
{
	updateParamsIfNeeded();
	return m_inputList.size();
}

template <bool MTL>
unsigned OSLNodeBase<MTL>::getNumVisibleOutputs() const
{
	return OSLNodeBase<MTL>::maxOutputs();
}

template <bool MTL>
unsigned OSLNodeBase<MTL>::getNumVisibleInputs() const
{
	return OSLNodeBase<MTL>::orderedInputs();
}

template <bool MTL>
unsigned OSLNodeBase<MTL>::orderedInputs() const
{
	int orderedInputs = NodeBase::orderedInputs();
	orderedInputs += customInputsCount();

	return orderedInputs;
}

template <bool MTL>
unsigned OSLNodeBase<MTL>::maxOutputs() const
{
	updateParamsIfNeeded();
	return NodeBase::maxOutputs() + (m_outputName != "");
}

template <bool MTL>
OP::VRayNode::PluginResult OSLNodeBase<MTL>::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	if (m_codeHash == 0) {
		Log::getLog().warning("Exporting \"%s\" does not have valid OSL code.", getName().c_str());
		return PluginResult::PluginResultContinue;
	}
	const fpreal t = exporter.getContext().getTime();

	UT_String oslSource;
	evalString(oslSource, "osl_source", 0, 0);
	if (oslSource == "OSL") {
		const QString tmpDir = QDir::tempPath() + "/";
		QTemporaryFile oslCodeFile(tmpDir + "XXXXXX.osl");
		oslCodeFile.setAutoRemove(false);
		if (!oslCodeFile.open()) {
			Log::getLog().error("Failed to open temp file for OSL \"%s\"", this->getName().nonNullBuffer());
			return PluginResult::PluginResultError;
		}

		UT_String oslCode;
		evalString(oslCode, "osl_code", 0, 0);
		if (oslCodeFile.write(oslCode.buffer(), oslCode.length()) != oslCode.length()) {
			Log::getLog().error("Failed to save inline OSL code from \"%s\"", this->getName().nonNullBuffer());
			return PluginResult::PluginResultError;
		}
		const QString & oslCodePath = oslCodeFile.fileName();
		pluginDesc.add(Attrs::PluginAttr("shader_file", oslCodePath.toStdString().c_str()));
	} else {
		UT_String oslPath;
		evalString(oslPath, "osl_file", 0, 0);
		pluginDesc.add(Attrs::PluginAttr("shader_file", oslPath.nonNullBuffer()));
	}

	if (MTL) {
		// TODO: if output is not closure, we can insert MTL single here
		pluginDesc.add(Attrs::PluginAttr("output_closure", m_outputName));
	} else {
		// TODO: TexOSL supports output_alpha also
		pluginDesc.add(Attrs::PluginAttr("output_color", m_outputName));
	}
	VRay::ValueList oslParams;
	int inputIdx = 0;
	for (int c = 0; c < m_paramList.size(); c++) {
		const int paramIdx = c + 1;
		oslParams.push_back(VRay::Value(m_paramList[c].name));

		const std::string & paramTypeName = mapTypeToParam<MTL>(m_paramList[c]);
		if (paramTypeName == "") {
			continue;
		}
		const std::string & paramName = "osl#" + paramTypeName;
		VRay::Value paramValue;
		switch (m_paramList[c].type) {
		case VOP_TYPE_COLOR:
		case VOP_TYPE_VECTOR:
		case VOP_TYPE_POINT:
		case VOP_TYPE_NORMAL: {
				VRay::FloatList list; // OSL param is always list
				for (int vi = 0; vi < 3; vi++) {
					list.push_back(evalFloatInst(paramName.c_str(), &paramIdx, vi, t));
				}
				paramValue = VRay::Value(list);
			}
			break;
		case VOP_TYPE_INTEGER: {
			int value = -1;
			// for integers that are menu, we must read the key of the selected menu option
			// as it was obtained from code
			if (m_paramList[c].widget == ParamInfo::Menu) {
				UT_String stringVal;
				evalStringInst(paramName.c_str(), &paramIdx, stringVal, 0, t);
				if (stringVal.isInteger(1)) {
					value = stringVal.toInt();
				} else {
					value = evalIntInst(paramName.c_str(), &paramIdx, 0, t);
				}

			} else {
				value = evalIntInst(paramName.c_str(), &paramIdx, 0, t);
			}
			paramValue = VRay::Value(value);

			break;
		}
		case VOP_TYPE_FLOAT:
			paramValue = VRay::Value(static_cast<float>(evalFloatInst(paramName.c_str(), &paramIdx, 0, t)));
			break;
		case VOP_TYPE_STRING:
			// if widget is String, this is not input
			if (m_paramList[c].widget == ParamInfo::String) {
				UT_String stringVal;
				evalStringInst(paramName.c_str(), &paramIdx, stringVal, 0, t);
				paramValue = VRay::Value(stringVal.nonNullBuffer());
			} else {
				// TODO: if exporting .vrscene file, appsdk will export empty element here which is incorrect for .vrscene
				//       it is possible to patch this by setting some dummy plugin that will return always black (to preserve default OSL behaviour)
				paramValue = VRay::Value(exporter.exportConnectedVop(this, UT_String(m_inputList[inputIdx++].c_str(), true), parentContext));
			}
			break;
		}
		oslParams.push_back(paramValue);
	}
	pluginDesc.add(Attrs::PluginAttr("input_parameters", oslParams));

	return PluginResult::PluginResultContinue;
}


/// Instantiate both versions of the class so all methods can be generated
/// NOTE [MacOS]: Use full namespace.
template class VRayForHoudini::VOP::OSLNodeBase<true>;
template class VRayForHoudini::VOP::OSLNodeBase<false>;
