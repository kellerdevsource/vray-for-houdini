import sys
import os
import json
import re

import vray
import uiguides

vr = None

def getVRay():
    global vr
    if not vr:
        vr = vray.VRayRenderer()

    return vr


def enum(*sequential, **named):
    enums = dict(zip(sequential, range(len(sequential))), **named)
    reverse = dict((value, key) for key, value in enums.iteritems())
    enums['reverseMapping'] = reverse
    enums['getName'] = classmethod(lambda cls, enumVal: cls.reverseMapping[enumVal] )
    return type('Enum', (), enums)


def forceAlphaNumeric(mystr, mych=" "):
    mystr = ''.join([c if c.isalnum() else mych for c in mystr])
    return mystr


def capitalizeLabel(mystr):
    mystr = " ".join( map(lambda x: x.capitalize(), mystr.split(" ")) )
    return mystr


def loadJSONDesc(filepath):
    if not os.path.isfile(filepath):
        return None

    jsonDesc = None
    with open(filepath, "r") as file:
        jsonDesc = json.load(file)

    return jsonDesc


def getPluginDescriptorFromJSON(filepath):
    # returns None if filepath is invalid json desc
    #         descriptor (vrayPlugin, jsonDesc)
    # NOTE that vrayPlugin will be None
    # if no corresponding plugin exists
    jsonDesc = loadJSONDesc(filepath)
    if not jsonDesc:
        return None

    vr = getVRay()
    vrayPlugin = getattr(vr.classes, jsonDesc["ID"], None)
    return (vrayPlugin, jsonDesc)


def getPluginDescriptorsFromJSON(plugindescpath):
    # yields descriptor for every valid json desc
    # NOTE that corresponding plugin might not exists
    for dirpath, dirnames, filenames in os.walk(plugindescpath):
        for f in filenames:
            filename, ext = os.path.splitext(f)
            if ext == ".json":
                abspath = os.path.join(dirpath, f)
                descriptor = getPluginDescriptorFromJSON(abspath)
                # if descriptor is invalid skip this file
                if not descriptor:
                    continue
                # at least valid json desc exists
                yield descriptor


def getPluginDescriptorFromVRayPlugin(vrayPlugin, plugindescpath):
    # returns descriptor (vrayPlugin, jsonDesc) for first valid json desc
    #         descriptor (vrayPlugin, None) if no valid json desc found
    for dirpath, dirnames, filenames in os.walk(plugindescpath):
            for f in filenames:
                filename, ext = os.path.splitext(f)
                if ext == ".json" and vrayPlugin.getName() == filename:
                    abspath = os.path.join(dirpath, f)
                    descriptor = (vrayPlugin, loadJSONDesc(abspath))
                    # second is json desc
                    found = bool( descriptor[1] )
                    # if current file is invalid json desc
                    # continue searching in rest of the file
                    if not found:
                        continue
                    # return first found valid json desc
                    return descriptor
    # no valid json desc has been found
    return (vrayPlugin, None)


def getPluginDescriptorsFromVRayPlugin(plugindescpath):
    # yields descriptor (vrayPlugin, jsonDesc) for first valid json desc
    #        descriptor (vrayPlugin, None) if no valid json desc found
    # NOTE that corresponding json desc might not exists
    vr = getVRay()
    for vrayPlugin in vr.classes:
        descriptor = getPluginDescriptorFromVRayPlugin(vrayPlugin, plugindescpath)
        if not descriptor:
            continue
        # NOTE that corresponding json desc might not exists
        yield descriptor


def getJSONParameter(jsonDesc, parmName, defaultVal = None):
    if not jsonDesc:
        return defaultVal

    for jsonParm in jsonDesc["Parameters"]:
        if jsonParm["attr"] == parmName:
            return jsonParm

    return defaultVal


def dumpInvalidDescriptors(plugindescpath):
    # check vray plugins
    print("=============== checking VRay plugins =============")
    nMissingJSONs = 0
    nMissingJSONParm = 0
    for descriptor in getPluginDescriptorsFromVRayPlugin(plugindescpath):
        # assert we have valid descriptor
        assert( descriptor )
        # assert we have valid vrayPlugin set
        assert( descriptor[0] )
        vrayPlugin, jsonDesc = descriptor
        if not jsonDesc:
            print("!!WARNING!! No JSON description found for plugin {}".format( descriptor[0].getName() ))
            nMissingJSONs +=1
            continue

        # check if all plugin parms exist on JSON
        for pluginParm in vrayPlugin:
            jsonParm = getJSONParameter(jsonDesc, pluginParm, None)
            if not jsonParm :
                print("!!WARNING!! JSON parameter NOT found {}::{}".format( vrayPlugin.getName(), pluginParm))
                nMissingJSONParm += 1

    # check json desc
    print("=============== checking JSON descriptions against VRay plugins =============")
    nMissingPlugins = 0
    nMissingPluginParm = 0
    for descriptor in getPluginDescriptorsFromJSON(plugindescpath):
        # assert we have valid descriptor
        assert( descriptor )
        # assert we have valid jsonDesc set
        assert( descriptor[1] )
        # check if plugin exists
        vrayPlugin, jsonDesc = descriptor
        if not vrayPlugin:
            print("!!WARNING!! No VRay plugin found for JSON description {}".format( jsonDesc["ID"] ))
            nMissingPlugins +=1
            continue

        # check if all JSON parms exist on plugin
        for jsonParm in jsonDesc["Parameters"]:
            pluginParm = getattr(vrayPlugin, jsonParm["attr"], None)
            if not pluginParm:
                print("!!WARNING!! VRay plugin parameter NOT found {}::{}".format( vrayPlugin.getName(), jsonParm["attr"]))
                nMissingPluginParm += 1

    if nMissingJSONs > 0:
        print("TOTAL (missing JSON descriptions) = {}".format( nMissingJSONs ))

    if nMissingPlugins:
        print("TOTAL (JSON descriptions missing corresponding VRayPlugin) = {}".format( nMissingPlugins ))

    if nMissingJSONParm:
        print("TOTAL (VRayPlugin parms missing corresponding JSON parm) = {}".format( nMissingJSONParm ))

    if nMissingPluginParm:
        print("TOTAL (JSON parms missing corresponding VRayPlugin parm) = {}".format( nMissingPluginParm ))


def getAllJSONTypes(plugindescpath):
    typeDict = {}
    for vrayPlugin, jsonDesc in getPluginDescriptorsFromJSON(plugindescpath):
        assert( jsonDesc )

        for jsonParm in jsonDesc["Parameters"]:
            parmType = jsonParm["type"]
            if not typeDict.has_key( parmType ):
                typeDict[ parmType ] = {
                                        "count": 0,
                                        "matching_types": {},
                                        }

            # gather type info from jsonParm
            typeDict[ parmType ]["count"] += 1
            pluginParm = getattr(vrayPlugin, jsonParm["attr"], None)
            pluginParmType = pluginParm["type"] if pluginParm else None

            if not typeDict[ parmType ]["matching_types"].has_key( pluginParmType ):
                typeDict[ parmType ]["matching_types"][ pluginParmType ] = 0

            typeDict[ parmType ]["matching_types"][ pluginParmType ] += 1

    return typeDict


def getAllVRayTypes(plugindescpath):
    typeDict = {}
    for vrayPlugin, jsonDesc in getPluginDescriptorsFromVRayPlugin(plugindescpath):
        assert( vrayPlugin )

        for parmName in vrayPlugin:
            pluginParm = getattr(vrayPlugin, parmName , None)
            parmType = pluginParm["type"] if pluginParm else None
            if not typeDict.has_key( parmType ):
                typeDict[ parmType ] = {
                                        "count": 0,
                                        "matching_types": {},
                                        }

            # gather type info from jsonParm
            typeDict[ parmType ]["count"] += 1
            jsonParm = getJSONParameter(jsonDesc, parmName, None)
            jsonParmType = jsonParm["type"] if jsonParm else None

            if not typeDict[ parmType ]["matching_types"].has_key( jsonParmType ):
                typeDict[ parmType ]["matching_types"][ jsonParmType ] = 0

            typeDict[ parmType ]["matching_types"][ jsonParmType ] += 1

    return typeDict


def getVRayTypes():
    typeDict = {}
    for plugin in getVRay().classes:
        for k, v in plugin.getMeta().iteritems():
            parmType = v.get("type", None)
            typeDict.setdefault(parmType, 0)
            typeDict[parmType] += 1

    return typeDict


def parseVRayValue(valueStr):
    brex = re.compile( "true|false", re.I)
    irex = re.compile( "[+-]*\d+", re.I)
    frex = re.compile( "{}(\.\d+([eE]\d+)*)*".format(irex.pattern), re.I)
    crex = re.compile( "Color\(\s*({})\s*,\s*({})\s*,\s*({})\s*\)".format(frex.pattern, frex.pattern, frex.pattern), re.I)
    acrex = re.compile( "AColor\(\s*({})\s*,\s*({})\s*,\s*({})\s*\,\s*({})\s*\)".format(frex.pattern, frex.pattern, frex.pattern, frex.pattern), re.I)
    vrex = re.compile( "Vector\(\s*({})\s*,\s*({})\s*,\s*({})\s*\)".format(frex.pattern, frex.pattern, frex.pattern), re.I)
    mrex = re.compile( "Matrix\(\s*({})\s*,\s*({})\s*,\s*({})\s*\)".format(vrex.pattern, vrex.pattern, vrex.pattern), re.I)
    trex = re.compile( "Transform\(\s*({})\s*,\s*({})\s*\)".format(mrex.pattern, vrex.pattern), re.I)
    # bool
    m = brex.match(valueStr)
    if m:
        return True if m.group(0).lower() == "true" else False
    # number
    m = frex.match(valueStr)
    if m:
        return uiguides.parseValue(valueStr)
    # color
    m = crex.match(valueStr)
    if m:
        return map(lambda x: uiguides.parseValue(x), [m.group(1), m.group(2+frex.groups), m.group(3+2*frex.groups)])
    # acolor
    m = acrex.match(valueStr)
    if m:
        return map(lambda x: uiguides.parseValue(x), [m.group(1), m.group(2+frex.groups), m.group(3+2*frex.groups), m.group(4+3*frex.groups)])
    # vector
    m = vrex.match(valueStr)
    if m:
        return map(lambda x: uiguides.parseValue(x), [m.group(1), m.group(2+frex.groups), m.group(3+2*frex.groups)])
    # matrix
    m = mrex.match(valueStr)
    if m:
        return [item for val in [m.group(1), m.group(2+vrex.groups), m.group(3+2*vrex.groups)] for item in parseVRayValue(val)]
    # transform
    m = trex.match(valueStr)
    if m:
        return [item for val in [m.group(1), m.group(2+mrex.groups)] for item in parseVRayValue(val)]
    # default leave as is
    return valueStr


def asJSONParm(parmName, parmProps):
    jsonDesc = {
        "attr": parmName,
        "desc": parmProps.get("description", ""),
        "type": parmProps.get("type", ""),
        "default": parseVRayValue(parmProps.get("defaultValue", "")),
    }

    if parmProps.has_key("elementsCount"):
        jsonDesc.setdefault("elements_count", parmProps["elementsCount"])

    typeMapping = {
        "String":                "STRING",
        "boolean":               "BOOL",
        "int":                   "INT",
        "double":                "FLOAT",
        "float":                 "FLOAT",
        "Vector":                "VECTOR",
        "Matrix":                "MATRIX",
        "Transform":             "TRANSFORM",
        "Color":                 "COLOR",
        "AColor":                "ACOLOR",
        "Texture":               "TEXTRE",
        "TextureInt":            "INT_TEXTURE",
        "TextureFloat":          "FLOAT_TEXTURE",
        "TextureVector":         "VECTOR_TEXTURE",
        "TextureMatrix":         "MATRIX_TEXTURE",
        "TextureTransform":      "TRANSFORM_TEXTURE",
        "Object":                "PLUGIN",
    }

    mappedType = typeMapping.get(jsonDesc["type"], None)
    if mappedType:
        jsonDesc["type"] = mappedType

    else:
        orex = re.compile("Output(.*)", re.I)
        lrex = re.compile("List(<(\w+)>)?", re.I)
        m = orex.match(jsonDesc["type"])
        if m:
            mappedType = typeMapping.get(m.group(1), None)
            jsonDesc["type"] = "OUTPUT_{}".format(mappedType if mappedType else m.group(1).upper())
        else :
            m = lrex.match(jsonDesc["type"])
            if m:
                mappedType = typeMapping.get(m.group(2), None) if m.group(2) else None
                jsonDesc["type"] = "{}_LIST".format(mappedType) if mappedType else "LIST"

    if jsonDesc["type"] == "BOOL":
        jsonDesc["default"] = bool(jsonDesc["default"])

    return jsonDesc


def asJSONDesc(vrayPlugin):
    assert(vrayPlugin)

    jsonDesc = {
        "ID": vrayPlugin.getName(),
        "Name": vrayPlugin.getName(),
        "Desciption": vrayPlugin.getName(),
        "Type": vrayPlugin.getName(),
        "Parameters": map(lambda x: asJSONParm(x[0], x[1]), vrayPlugin.getMeta().iteritems()),
        "Widget": {
            "widgets": []
        }
    }

    return jsonDesc


def getAllJSONParmAttrs(plugindescpath):
    attrDict = {}

    for vrayPlugin, jsonDesc in getPluginDescriptorsFromJSON(plugindescpath):
        assert( jsonDesc )

        for jsonParm in jsonDesc["Parameters"]:
            for attrToken in jsonParm.keys():
                if not attrDict.has_key( attrToken ):
                    attrDict[attrToken] = 0

                attrDict[attrToken] += 1

    return attrDict


def dumpJSONAttrInfo(plugindescpath):
    print("========== JSON parm attributes =========")
    for k, v in getAllJSONParmAttrs(plugindescpath).iteritems():
        print("'{}': {}".format(k, v))
    print("=========================================")


def dumpJSONTypeInfo(plugindescpath):
    print("========== JSON type info =========")
    for mytype, typeDict in getAllJSONTypes(plugindescpath).iteritems():
        print("'{}' : {{".format( mytype ))
        for othertype, count in typeDict["matching_types"].iteritems():
            print("\t\t'{}' : {},".format(othertype, count))
        print("},")
    print("=========================================")


def dumpVRayTypeInfo(plugindescpath):
    print("========== JSON type info =========")
    for mytype, typeDict in getAllVRayTypes(plugindescpath).iteritems():
        print("'{}' : {{".format( mytype ))
        for othertype, count in typeDict["matching_types"].iteritems():
            print("\t\t{} : {},".format(othertype, count))
        print("},")
    print("=========================================")


def getPluginTypeMismatch(plugindescpath, jsonParmType, pluginParmType):
    descriptors = getPluginDescriptorsFromJSON(plugindescpath)
    for vrayPlugin, jsonDesc in descriptors:
        # skip descriptors which don't have corresponding plugin
        if not vrayPlugin:
            continue

        for jsonParm in jsonDesc["Parameters"]:
            # skip unmatching type
            if jsonParm["type"].lower() != jsonParmType.lower():
                continue

            # skip parameters which don't exist on vrayPlugin
            pluginParm = getattr(vrayPlugin, jsonParm["attr"], None)
            if not pluginParm:
                continue

            if pluginParm["type"].lower() == pluginParmType.lower():
                yield (vrayPlugin, jsonParm["attr"], pluginParm, jsonParm)


def dumpPluginTypeMismatch(plugindescpath, jsonParmType, pluginParmType):
    print("--------------------")
    print("('{}' , '{}')".format( jsonParmType, pluginParmType))
    print("--------------------")

    nMismatch = 0
    for vrayPlugin, parmName, pluginParm, jsonParm in getPluginTypeMismatch(plugindescpath, jsonParmType, pluginParmType):
        print("{}::{}".format(vrayPlugin.getName(), parmName))
        print("VRay plugin parm: {}".format( pluginParm) )
        print("JSON parm : {}".format( jsonParm) )
        print("--------------------")
        nMismatch += 1

    print("TOTAL = {}".format( nMismatch ))
    print("--------------------")


def dumpPluginParms(vrayPlugin):
    print "================== {} ==================".format(vrayPlugin.getName())
    for k, v in vrayPlugin.getMeta().iteritems():
        print "{:20} [{:5}]={:20} :::{}".format(k, v['type'], v['defaultValue'], v['description'])


def filterPlugins(rex):
    return filter(lambda plg: rex.match(plg.getName()), getVRay().classes)


def getJSONTypesEnum(plugindescpath):
    jsonTypes = getAllJSONTypes(plugindescpath).keys()
    jsonTypes = dict( zip( map( lambda jsonType: forceAlphaNumeric(jsonType, mych="_").lower(), jsonTypes ), jsonTypes ) )
    return enum(**jsonTypes)


def getVRayTypesEnum(plugindescpath):
    vrayTypes = getAllVRayTypes(plugindescpath).keys()
    vrayTypes = dict( zip( map( lambda vrayType: forceAlphaNumeric(vrayType, mych="_").lower(), vrayTypes ), vrayTypes ) )
    return enum(**vrayTypes)


def getCondition(item):
    cond = item.get("active", None)
    condType = "disable_when"

    if not cond:
        cond = item.get("show", None)
        condType = "hide_when"

    if not cond:
        cond = item.get("visible", None)
        condType = "hide_when"

    if not cond:
        return None

    parm = cond.get("prop", None)
    if not parm:
        return None

    value = cond.get("value", 1)

    # we need to translate conditions
    # from active(enabled) to disable_when and show(visible) to hide_when
    # thus we need to reverse the conditional operation
    # so "equal" becomes !=, "less" - >=, etc.
    # set default to not equal
    op = "!="
    opvalue = "and"
    parmCond = cond.get("condition", None)
    if parmCond is True:
        op = "!="
        value = 1
    elif parmCond is False:
        op = "!="
        value = 0
    elif parmCond == "equal":
        op = "!="
    elif parmCond == "not_equal":
        op = "=="
    elif parmCond == "greater":
        op = "<="
    elif parmCond == "greater_or_equal":
        op = "<"
    elif parmCond == "less":
        op = ">="
    elif parmCond == "less_or_equal":
        op = ">"
    elif parmCond == "in":
        op = "!="
    elif parmCond == "not_in":
        op = "=="
        opvalue = "or"

    # make value iterable if it's not one already
    if not hasattr(value, '__iter__'):
        value = (value, )

    # condition contains
    #    parm - parmName
    #    op - comparison operator
    #    value - iterable set of values(one or more) to compare against
    condition = {
                "type" : condType,
                "parm" : parm,
                "op" : op,
                "values" : value,
                "opvalue" : opvalue,
                "parmowner" : cond.get("at", None),
                }

    return condition


def addCondtion(jsonParm, conditions, parmConditions):
    disableCond = filter( lambda cond: bool(cond) and cond.get("type") == "disable_when", conditions )
    hideCond = filter( lambda cond: bool(cond) and cond.get("type") == "hide_when", conditions )

    if disableCond:
        if not parmConditions.has_key( jsonParm["attr"] ):
            parmConditions[ jsonParm["attr"] ] = {
                                                    "disable_when": [],
                                                    "hide_when": [],
                                                    }

        parmConditions[ jsonParm["attr"] ]["disable_when"] += disableCond

    if hideCond:
        if not parmConditions.has_key( jsonParm["attr"] ):
            parmConditions[ jsonParm["attr"] ] = {
                                                    "disable_when": [],
                                                    "hide_when": [],
                                                    }

        parmConditions[ jsonParm["attr"] ]["hide_when"] += hideCond

    return parmConditions


def addJSONItemCondtions(jsonDesc, item, conditions, parmConditions):
    attrs = item.get("attrs", None)
    if attrs:
        for attr in attrs:
            parmName = attr.get("name", None)
            if not parmName:
                continue

            jsonParm = getJSONParameter(jsonDesc, parmName, None)
            if not jsonParm:
                continue

            acond = getCondition(attr)
            addCondtion(jsonParm, conditions + (acond, ), parmConditions)

    return parmConditions


def getJSONParmConditions(vrayPlugin, jsonDesc):
    parmConditions = {}

    widgets = jsonDesc.get("Widget" , None)
    widgets = widgets["widgets"] if widgets else None
    if not widgets:
        return parmConditions

    for witem in widgets:
        wcond = getCondition(witem)
        addJSONItemCondtions(jsonDesc, witem, (wcond, ), parmConditions)

        splits = witem.get("splits", None)
        if splits:
            for sitem in splits:
                scond = getCondition(sitem)
                addJSONItemCondtions(jsonDesc, sitem, (wcond, scond), parmConditions)

    return parmConditions


def updateUIGuides(vrayParm, jsonParm):
    assert( vrayParm )
    assert( jsonParm )

    # adjust jsonParm ui by parsing vrayPlugin parm uiGuides
    try:
        uiGuides = jsonParm.get("ui", {})
        uiGuides.update( uiguides.parseUIGuides( vrayParm.get("uiGuides", None) ) )
        if uiGuides:
            jsonParm["ui"] = uiGuides
    except Exception as e:
        print("!!ERROR!! {}".format(e.message))
        print("!!ERROR!! uiGuides:", jsonParm)
        print("!!ERROR!! uiGuides:", vrayParm.get("uiGuides", None))
        raise e



def dumpUIGuides(plugindescpath):
    descriptors = getPluginDescriptorsFromJSON(plugindescpath)
    for vrayPlugin, jsonDesc in descriptors:
        # skip descriptors which don't have corresponding plugin
        if not vrayPlugin:
            continue

        # print("==================== {} =================".format(vrayPlugin.getName()))
        for jsonParm in jsonDesc["Parameters"]:
            # skip parameters which don't exist on vrayPlugin
            pluginParm = getattr(vrayPlugin, jsonParm["attr"], None)
            if not pluginParm:
                continue

            if jsonParm.get("ui", None):
                if not pluginParm.get("uiGuides", None):
                    print("----------------")
                    print("{}::{}".format(vrayPlugin.getName(), jsonParm["attr"]))
                    print(jsonParm.get("ui", None))
                    print(pluginParm.get("uiGuides", None))
