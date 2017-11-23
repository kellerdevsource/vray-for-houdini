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

#include <boost/filesystem.hpp>

#include <OSL/oslconfig.h>
#include <OSL/oslcomp.h>
#include <OSL/oslquery.h>

// OIIO
#include <OpenImageIO/errorhandler.h>
#include <OpenImageIO/typedesc.h>

using namespace VRayForHoudini;
using namespace VOP;

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

const std::string OSL_PARAM_TYPE_LIST[] = {
	"color_param",
	"vector_param",
	"float_param",
	"int_param",
};

const int OSL_PARAM_TYPE_COUNT = sizeof(OSL_PARAM_TYPE_LIST) / sizeof(OSL_PARAM_TYPE_LIST[0]);

static const std::map<VOP_Type, std::string> OSL_PARAM_MAP = {
	{VOP_TYPE_COLOR, OSL_PARAM_TYPE_LIST[0]},
	{VOP_TYPE_VECTOR, OSL_PARAM_TYPE_LIST[1]},
	{VOP_TYPE_POINT, OSL_PARAM_TYPE_LIST[1]},
	{VOP_TYPE_NORMAL, OSL_PARAM_TYPE_LIST[1]},
	{VOP_TYPE_INTEGER, OSL_PARAM_TYPE_LIST[2]},
	{VOP_TYPE_FLOAT, OSL_PARAM_TYPE_LIST[3]},
};
std::string mapTypeToParam(VOP_Type type)
{
	auto iter = OSL_PARAM_MAP.find(type);
	if (iter == OSL_PARAM_MAP.end()) {
		return "";
	}
	return iter->second;
}
}

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
		needCompile = filePath.endsWith(".osl");

		// TODO: is it worth it to cache path + last change time
		std::ifstream oslFile(filePath, std::ios::binary);
		if (!oslFile) {
			return;
		}

		oslFile.seekg(0, std::ios::end);
		const int size = oslFile.tellg().seekpos();
		std::unique_ptr<char[]> data = std::unique_ptr<char[]>(new char[size + 1]);
		oslFile.seekg(0, std::ios::beg);
		if (!oslFile.read(data.get(), size)) {
			Log::getLog().error("Failed to read \"%s\" selected as osl source file", filePath.nonNullBuffer());
			return;
		}

		data[size] = 0;
		oslCode.adopt(data.release());
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
		static VUtils::GetEnvVar APPSDK_PATH("VRAY_APPSDK", "");
		OSLCompiler * compiler = OSLCompiler::create();
		OSLCompilerInput settings;
		settings.inputMode = OSL_MEMORY_BUFFER;
		settings.outputMode = OSL_MEMORY_BUFFER;
		settings.buffer = std::move(oslCode.toStdString());
		settings.stdoslpath = APPSDK_PATH.getValue().ptr() + std::string("/bin/stdosl.h");
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
		osoCode = std::move(oslCode.toStdString());
	}

	OSLQuery query;
	if (!query.load(osoCode, &staticErrHandle)) {
		Log::getLog().error("Failed to query OSL parameters.");
		return;
	}

	for (int c = 0; c < query.nparams(); c++) {
		const OSLQuery::Parameter *param = query.getparam(c);

		if (param->isoutput) {
			if (param->isclosure) {
				if (MTL) {
					self->m_outputName = param->name;
				} else {
					// TODO: error
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
		ParamInfo info = {param->name, VOP_TYPE_UNDEF};
		info.validDefault = param->validdefault;

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
				self->m_inputList.push_back(param->name);
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

		// Set the param name in string field becasue we cant change labels of params
		self->setStringInst(UT_String(param.name.c_str(), param.name.length()),
			CH_StringMeaning::CH_STRING_LITERAL, "osl#label", &paramIdx, 0, 0);

		// show only the type this param is
		const std::string & oslParamName = mapTypeToParam(param.type);
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
					self->setInt(paramName, 0, 0, param.numberDefault[0]);
					break;
				case VOP_TYPE_FLOAT:
					self->setFloat(paramName, 0, 0, param.numberDefault[0]);
					break;
				case VOP_TYPE_STRING:
					self->setString(UT_String(param.stringDefault.c_str(), true), CH_STRING_LITERAL, m_inputList[inputIdx++].c_str(), 0, 0);
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
		// TODO: figgure out why Houdini queries 1 more input type than there are
		//if (sockIdx >= customInputsCount()) {
		//	Log::getLog().warning("getInputTypeInfoSubclass(%d [%d]) out of range", idx, sockIdx);
		//}

	}
}

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

	boost::filesystem::path oslCodePath = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("%%%%-%%%%-%%%%-%%%%.osl");

	UT_String oslCode;
	evalString(oslCode, "osl_code", 0, 0);
	{
		std::ofstream tmpFile(oslCodePath.c_str(), std::ios::trunc | std::ios::binary);
		if (!tmpFile || !tmpFile.write(oslCode.c_str(), oslCode.length())) {
			// TODO: Log err
			return PluginResult::PluginResultError;
		}
	}
	pluginDesc.add(Attrs::PluginAttr("shader_file", oslCodePath.string()));
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

		const std::string & paramName = "osl#" + mapTypeToParam(m_paramList[c].type);
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
		case VOP_TYPE_INTEGER:
			paramValue = VRay::Value(static_cast<int>(evalIntInst(paramName.c_str(), &paramIdx, 0, t)));
			break;
		case VOP_TYPE_FLOAT:
			paramValue = VRay::Value(static_cast<float>(evalFloatInst(paramName.c_str(), &paramIdx, 0, t)));
			break;
		case VOP_TYPE_STRING:
			paramValue = VRay::Value(exporter.exportConnectedVop(this, UT_String(m_inputList[inputIdx++].c_str(), true), parentContext));
			break;
		}
		oslParams.push_back(paramValue);
	}
	pluginDesc.add(Attrs::PluginAttr("input_parameters", oslParams));

	return PluginResult::PluginResultContinue;
}
