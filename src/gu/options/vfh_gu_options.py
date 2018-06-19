#
# Copyright (c) 2015-2018, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import os
import sys

global HDK

BOOL = "bool"
INT = "exint"
FLOAT = "fpreal"
CHAR = "const char*"
VECTOR = "fpreal*"

def toIntrType(intrType):
	if intrType == 'BOOL':
		return BOOL
	if intrType == 'INT':
		return INT
	if intrType == 'FLOAT':
		return FLOAT
	if intrType == 'CHAR':
		return CHAR
	if intrType == 'VECTOR':
		return VECTOR

class IntrAttr:
	def __init__(self, intrType, intrName, intrValue, intrSize = 1):
		self.intrType = intrType
		self.intrName = intrName
		self.intrValue = intrValue
		self.intrSize = intrSize

def toCamelCase(s):
	s = s.replace("_", " ").title().replace(" ", "")
	return s

def replaceByDict(s, d):
	for k in d:
		s = s.replace("%%%s%%"%(k), d[k])
	return s

def getIntrAttrGetSizeArgs(intrAttr):
	if HDK > 16.0:
		return "const GU_PrimPacked *prim"
	return ""

def getIntrAttrGetArgs(intrAttr):
	args = ""

	if intrAttr.intrSize != 1:
		if intrAttr.intrType == CHAR:
			args += "UT_StringArray &value"
		elif intrAttr.intrType == VECTOR:
			args += "fpreal64 *value, exint size"

	return args

def getIntrAttrRetType(intrAttr):
	if intrAttr.intrSize == 1:
		return intrAttr.intrType
	return "void"

def getIntrAttrSetArgs(intrAttr):
	args = ""

	if intrAttr.intrSize == 1:
		args += "%s value" % intrAttr.intrType
	else:
		if intrAttr.intrType == CHAR:
			args += "const UT_StringArray &value"
		elif intrAttr.intrType == VECTOR:
			args += "const fpreal64 *value, exint size"

	return args

def getIntrAttrDefValue(intrAttr):
	if intrAttr.intrType == FLOAT:
		return "%.3f" % intrAttr.intrValue
	if intrAttr.intrType == CHAR:
		return '"%s"' % intrAttr.intrValue
	if intrAttr.intrType == BOOL:
		return '%i' % intrAttr.intrValue
	return "%s" % intrAttr.intrValue

def getIntrAttrOptionsSetter(intrAttr):
	setter = None
	if intrAttr.intrType == FLOAT:
		setter = "setOptionF"
	elif intrAttr.intrType == INT:
		setter = "setOptionI"
	elif intrAttr.intrType == CHAR:
		setter = "setOptionS"
	elif intrAttr.intrType == BOOL:
		setter = "setOptionB"
	elif intrAttr.intrType == VECTOR:
		setter = "setOptionV3"

	assert setter != None

	if intrAttr.intrSize != 1:
		if intrAttr.intrType != VECTOR:
			setter = "%sArray" % setter

	return setter

def getIntrAttrRegSetterCast(intrAttr, forArray = False):
	setterCast = None
	if intrAttr.intrType == FLOAT:
		setterCast = "Float"
	elif intrAttr.intrType == INT:
		setterCast = "Int"
	elif intrAttr.intrType == CHAR:
		setterCast = "String"
	elif intrAttr.intrType == BOOL:
		setterCast = "Bool"
	elif intrAttr.intrType == VECTOR:
		setterCast = "F64Vector"

	assert setterCast

	if forArray:
		if intrAttr.intrType != VECTOR:
			setterCast = "%sArray" % setterCast

	return "%sSetterCast" %  setterCast

def getIntrAttrOptionsGetter(intrAttr):
	getter = None
	if intrAttr.intrType == FLOAT:
		getter = "getOptionF"
	elif intrAttr.intrType == INT:
		getter = "getOptionI"
	elif intrAttr.intrType == CHAR:
		getter = "getOptionS"
	elif intrAttr.intrType == BOOL:
		getter = "getOptionB"
	elif intrAttr.intrType == VECTOR:
		getter = "getOptionV3"

	assert getter != None

	if intrAttr.intrSize != 1:
		if intrAttr.intrType != VECTOR:
			getter = "%sArray" % getter

	return getter

def getIntrAttrRegGetterCast(intrAttr, forArray = False):
	getterCast = None
	if intrAttr.intrType == FLOAT:
		getterCast = "Float"
	elif intrAttr.intrType == INT:
		getterCast = "Int"
	elif intrAttr.intrType == CHAR:
		getterCast = "String"
	elif intrAttr.intrType == BOOL:
		getterCast = "Bool"
	elif intrAttr.intrType == VECTOR:
		getterCast = "F64Vector"

	assert getterCast

	if forArray:
		if intrAttr.intrType != VECTOR:
			getterCast = "%sArray" % getterCast

	return "%sGetterCast" %  getterCast

def main(fileRoot, jsonFilePath):
	import json
	guDict = json.loads(open(jsonFilePath, 'r').read())

	className = os.path.splitext(os.path.basename(jsonFilePath))[0]

	OPT_CLASS_NAME = "%sBase" % className

	intrAttrs = []

	for guAttr in guDict['intrinsics']:
		intrAttrs.append(IntrAttr(toIntrType(guAttr['type']), guAttr['name'], guAttr['value'], guAttr.get('size', 1)))

	hFileName = "vfh_%s.h" % OPT_CLASS_NAME
	cppFileName = "vfh_%s.cpp" % OPT_CLASS_NAME

	hFilePath = os.path.join(fileRoot, hFileName)
	cppFilePath = os.path.join(fileRoot, cppFileName)

	classTmpl = """//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//  https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

// NOTE: This file is auto-generated! Do not edit!

#ifndef VRAY_FOR_HOUDINI_%CLASS_NAME_UPPER%_H
#define VRAY_FOR_HOUDINI_%CLASS_NAME_UPPER%_H

#include "vfh_gu_base.h"

#include <GU/GU_PackedFactory.h>
"""
	if HDK > 16.0:
		classTmpl += "#include <GU/GU_PrimPacked.h>\n"

	classTmpl += """#include <UT/UT_Options.h>

namespace VRayForHoudini {

class %CLASS_NAME%
	: public VRayBaseRef
{
public:
	%CLASS_NAME%()
	{}
	%CLASS_NAME%(const %CLASS_NAME% &other)
		: VRayBaseRef(other)
	{}
%METHODS%
	%REG_METHODS%
	/// Returns current options.
	const UT_Options& getOptions() const { return m_options; }

protected:
	/// Intrinsic names storage.
	struct IntrinsicNames {
	%INTR_NAMES%
	};
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_%CLASS_NAME_UPPER%_H
"""

	classCppTmpl = r"""//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//  https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

// NOTE: This file is auto-generated! Do not edit!

#include "%HEADER_NAME%"

using namespace VRayForHoudini;

%INTR_INIT_NAMES%
"""

	intrMethodsArr = []

	intrMethods = []
	intrNames = []
	intrInitNames = []

	for intrAttr in intrAttrs:
		funcName = toCamelCase(intrAttr.intrName)

		intrGetter = "get%s%s" % (funcName, "Prim" if HDK > 16.0 else "")
		intrSetter = "set%s%s" % (funcName, "Prim" if HDK > 16.0 else "")

		intrAttrMethods = {
			'intr' : intrAttr,
			'getter' : intrGetter,
			'setter' : intrSetter,
		}

		getSetTmpl = ""

		def getGetterImpl(intrAttr, getArgs, primAttr = False):
			getSetTmpl = "\n\t/// Returns \"%ATTR_NAME%\" intrinsic value."
			if primAttr:
				getSetTmpl += "\n\t/// @param prim Packed primitive instance."
			getSetTmpl += "\n\t%TYPE% %GET_METHOD_NAME%"
			if primAttr:
				getSetTmpl += "Prim"
			getSetTmpl += "("
			if primAttr:
				getSetTmpl += "const GU_PrimPacked *prim"
				if getArgs:
					getSetTmpl += ", "
			getSetTmpl += getArgs;
			getSetTmpl += ") const {"
			getSetTmpl += "\n\t\tif (!m_options.hasOption(IntrinsicNames::%ATTR_NAME%)) {"

			if intrAttr.intrSize != 1:
				if intrAttr.intrType == VECTOR:
					getSetTmpl += "\n\t\t\treturn;"
					getSetTmpl += "\n\t\t}"
					getSetTmpl += "\n\t\tconst UT_Vector3D &vector = m_options.getOptionV3(IntrinsicNames::%ATTR_NAME%);"
					getSetTmpl += "\n\t\tvalue[0] = vector.x();"
					getSetTmpl += "\n\t\tvalue[1] = vector.y();"
					getSetTmpl += "\n\t\tvalue[2] = vector.z();"
				else:
					getSetTmpl += "\n\t\t\treturn;"
					getSetTmpl += "\n\t\t}"
					getSetTmpl += "\n\t\tvalue = m_options.%GETTER%(IntrinsicNames::%ATTR_NAME%);"
			else:
				getSetTmpl += "\n\t\t\treturn %ATTR_DEF%;"
				getSetTmpl += "\n\t\t}"
				getSetTmpl += "\n\t\treturn m_options.%GETTER%(IntrinsicNames::%ATTR_NAME%);"

			getSetTmpl += "\n\t}\n"
			return getSetTmpl

		getSetTmpl += getGetterImpl(intrAttr, getIntrAttrGetArgs(intrAttr), HDK > 16.0)
		if HDK > 16.0:
			getSetTmpl += getGetterImpl(intrAttr, getIntrAttrGetArgs(intrAttr), False)

		# Add size method.
		if intrAttr.intrSize != 1:
			intrAttrMethods['getterSize'] = "%sSize" % intrGetter

			getSetTmpl += "\n\t/// Returns \"%ATTR_NAME%\" intrinsic array size."
			getSetTmpl += "\n\texint %GET_METHOD_NAME%"
			if HDK > 16.0:
				getSetTmpl += "Prim"
			getSetTmpl += "Size(%GET_SIZE_ARGS%) const {"
			getSetTmpl += "\n\t\tif (!m_options.hasOption(IntrinsicNames::%ATTR_NAME%)) {"
			getSetTmpl += "\n\t\t\treturn 0;"
			getSetTmpl += "\n\t\t}"
			if intrAttr.intrType == VECTOR:
				getSetTmpl += "\n\t\treturn %ATTR_SIZE%;"
			else:
				getSetTmpl += "\n\t\treturn m_options.%GETTER%(IntrinsicNames::%ATTR_NAME%).size();"

			getSetTmpl += "\n\t}\n"

		def getOptionsGetterImpl(intrAttr):
			pass

		def getOptionsSetterImpl(intrAttr, primAttr = False):
			setterImpl = ""
			def getSetOption(intrType, options):
				if intrAttr.intrType == VECTOR:
					return options + ".setOptionV3(IntrinsicNames::%ATTR_NAME%, value[0], value[1], value[2]);"
				else:
					return options + ".%SETTER%(IntrinsicNames::%ATTR_NAME%, value);"
			if primAttr:
				setterImpl += "UT_Options options;"
				setterImpl += "\n\t\toptions.merge(m_options);"
				setterImpl += "\n\t\t" + getSetOption(intrAttr.intrType, "options")
				setterImpl += "\n\t\tvassert(prim);"
				setterImpl += "\n\t\tvassert(prim->implementation());"
				setterImpl += "\n\t\tprim->implementation()->update(prim, options);"
			else:
				setterImpl += getSetOption(intrAttr.intrType, "m_options")
			return setterImpl

		def getSetterImpl(intrAttr, setArgs, primAttr = False):
			getSetTmpl = "\n\t/// Sets \"%ATTR_NAME%\" intrinsic value."
			if primAttr:
				getSetTmpl += "\n\t/// @param prim Packed primitive instance."

			getSetTmpl += "\n\t/// @param value Value for \"%ATTR_NAME%\" intrinsic."
			getSetTmpl += "\n\tvoid %SET_METHOD_NAME%"
			if primAttr:
				getSetTmpl += "Prim"
			getSetTmpl += "("
			if primAttr:
				if intrAttr.intrType == "FLOAT":
					getSetTmpl += "const "
				getSetTmpl += "GU_PrimPacked *prim"
				if setArgs:
					getSetTmpl += ", "
			getSetTmpl += setArgs

			getSetTmpl += ") {"
			getSetTmpl += "\n\t\t%OPTION_SETTER_IMPL%"
			getSetTmpl += "\n\t}\n"

			getSetTmpl = replaceByDict(getSetTmpl, {
				'OPTION_SETTER_IMPL' : getOptionsSetterImpl(intrAttr, primAttr),
			})
			return getSetTmpl

		getSetTmpl += getSetterImpl(intrAttr, getIntrAttrSetArgs(intrAttr), HDK > 16.0)
		if HDK > 16.0:
			getSetTmpl += getSetterImpl(intrAttr, getIntrAttrSetArgs(intrAttr), False)

		intrMethodsArr.append(intrAttrMethods)

		getSetDict = {
			'TYPE' : getIntrAttrRetType(intrAttr),
			'ATTR_NAME' : intrAttr.intrName,
			'ATTR_SIZE' : str(intrAttr.intrSize),
			'ATTR_DEF' : getIntrAttrDefValue(intrAttr),
			'GET_SIZE_ARGS' : getIntrAttrGetSizeArgs(intrAttr),
			'GET_METHOD_NAME' : intrGetter[:-4] if intrGetter.endswith("Prim") else intrGetter,
			'GET_ARGS' : getIntrAttrGetArgs(intrAttr),
			'GETTER' : getIntrAttrOptionsGetter(intrAttr),
			'SET_METHOD_NAME' : intrSetter[:-4] if intrSetter.endswith("Prim") else intrSetter,
			'SET_ARGS' : getIntrAttrSetArgs(intrAttr),
			'SETTER' : getIntrAttrOptionsSetter(intrAttr),
		}
		intrMethods.append(replaceByDict(getSetTmpl, getSetDict))

		intrNameTmpl = "\tstatic const UT_String %INTR_NAME%;"
		intrDict = {
			'INTR_NAME' : intrAttr.intrName,
		}
		intrNames.append(replaceByDict(intrNameTmpl, intrDict))

		intrInitTmpl = "const UT_String %CLASS_NAME%::IntrinsicNames::%INTR_NAME% = \"%INTR_NAME%\";";
		intrInitTmplDict = {
			'CLASS_NAME' : OPT_CLASS_NAME,
			'INTR_NAME' : intrAttr.intrName,
		}
		intrInitNames.append(replaceByDict(intrInitTmpl, intrInitTmplDict))

	intrRegs = []
	for intrMethodsDesc in intrMethodsArr:
		intrAttr = intrMethodsDesc['intr']

		if intrAttr.intrSize != 1:
			regIntrTmpl = "\tfactory.registerTupleIntrinsic(IntrinsicNames::%ATTR_NAME%, factory.IntGetterCast(&T::%GETTER%Size), factory.%GETTER_ARRAY_CAST%(&T::%GETTER%), factory.%SETTER_ARRAY_CAST%(&T::%SETTER%));"
		else:
			regIntrTmpl = "\tfactory.registerIntrinsic(IntrinsicNames::%ATTR_NAME%, factory.%GETTER_CAST%(&T::%GETTER%), factory.%SETTER_CAST%(&T::%SETTER%));"

		regIntrDict = {
			getIntrAttrRegSetterCast
		}

		intrRegs.append(replaceByDict(regIntrTmpl, {
			'ATTR_NAME' : intrAttr.intrName,
			'GETTER_CAST' : getIntrAttrRegGetterCast(intrAttr),
			'GETTER_ARRAY_CAST' : getIntrAttrRegGetterCast(intrAttr, True),
			'GETTER' : intrMethodsDesc['getter'],
			'SETTER_CAST' : getIntrAttrRegSetterCast(intrAttr),
			'SETTER_ARRAY_CAST' : getIntrAttrRegSetterCast(intrAttr, True),
			'SETTER' : intrMethodsDesc['setter'],
		}))

	regTmpl = """/// Registers intrinsics on the factory.
	/// @param factory Packed primitive factory.
	template <typename T>
	static void registerIntrinsics(GU_PackedFactory &factory) {
	%REGS%
	}
"""
	regDict = {
		'REGS' : "\n\t".join(intrRegs),
	}

	classDict = {
		'CLASS_NAME' : OPT_CLASS_NAME,
		'CLASS_NAME_UPPER': OPT_CLASS_NAME.upper(),
		'METHODS' : "".join(intrMethods),
		'INTR_NAMES' : "\n\t".join(intrNames),
		'REG_METHODS' : replaceByDict(regTmpl, regDict),
	}

	classCppDict = {
		'CLASS_NAME' : OPT_CLASS_NAME,
		'HEADER_NAME' : hFileName,
		'INTR_INIT_NAMES' : "\n".join(intrInitNames),
	}

	classTmpl = replaceByDict(classTmpl, classDict)
	classCppTmpl = replaceByDict(classCppTmpl, classCppDict)

	open(hFilePath, 'w').write(classTmpl)
	open(cppFilePath, 'w').write(classCppTmpl)

if __name__ == '__main__':
	global HDK

	import argparse

	parser = argparse.ArgumentParser()
	parser.add_argument("--hdk", default=16.5)
	parser.add_argument("--jsonFile")
	parser.add_argument("--outDir")

	args = parser.parse_args()
	if not args.outDir:
		args.outDir = "C:\\build\\vray_for_houdini\\src"
	if not args.jsonFile:
		args.jsonFile = os.path.join(os.path.dirname(__file__), "json", "VRaySceneRef.json")

	HDK = float(args.hdk)

	main(args.outDir, args.jsonFile)
