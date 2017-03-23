import houmodule as hou
import pluginutils as plgutils


try:
    # attempt to evaluate basestring
    basestring
    def isstr(s):
        return isinstance(s, basestring)
except NameError:
    def isstr(s):
        return isinstance(s, str)


def adjustParmDisplayName(pt, vrayPlugin, jsonParm):
    ptLabel = plgutils.forceAlphaNumeric( pt.label() )

    if jsonParm.has_key("name"):
        ptLabel = jsonParm["name"]

    ui = jsonParm.get("ui", None)
    if ui:
        if ui.has_key("display_name"):
            ptLabel = ui["display_name"]

    ptLabel = plgutils.capitalizeLabel( ptLabel )
    pt.setLabel(ptLabel)

    return pt


def adjustParmMinMax(pt, vrayPlugin, jsonParm):
    pttype = pt.dataType()
    if pttype == hou.parmData.Int:
        pttype = int

    elif pttype == hou.parmData.Float:
        pttype = float

    else:
        return pt

    try:
        ui = jsonParm.get("ui", None)
        if ui:
            if ui.has_key("min"):
                pt.setMinValue( pttype(ui["min"]) )
                pt.setMinIsStrict(False)

            if ui.has_key("soft_min"):
                pt.setMinValue( pttype(ui["soft_min"]) )
                pt.setMinIsStrict(False)

            if ui.has_key("max"):
                pt.setMaxValue( pttype(ui["max"]) )
                pt.setMaxIsStrict(False)

            if ui.has_key("soft_max"):
                pt.setMaxValue( pttype(ui["soft_max"]) )
                pt.setMaxIsStrict(False)

            if ui.has_key("spin_step"):
                tags = pt.tags()
                tags["vray_spin_step"] = str(ui["spin_step"])
                pt.setTags(tags)

    except Exception as e:
        print(vrayPlugin, jsonParm)
        raise e

    return pt


def adjustParmUnits(pt, vrayPlugin, jsonParm):
    ui = jsonParm.get("ui", None)
    if ui:
        if ui.has_key("units"):
            tags = pt.tags()
            tags["vray_units"] = ui["units"]
            pt.setTags(tags)
            if ui["units"] == "radians":
                pt.setLook(hou.parmLook.Angle)
            elif ui["units"] == "degrees":
                pt.setLook(hou.parmLook.Angle)

    return pt


def createParmTemplatePluginData(vrayPlugin, jsonDesc):
    tags = {
            "vray_plugin" : jsonDesc["ID"],
            "vray_type" : jsonDesc["Type"],
            }
    if jsonDesc.has_key("Subtype"):
        tags["vray_category"] = jsonDesc["Subtype"].lower()

    helpstr = jsonDesc.get("Description", jsonDesc.get("Desciption", None))

    pt = hou.DataParmTemplate("plugindata",
                            jsonDesc["Name"],
                            1,
                            is_hidden=True,
                            help=helpstr,
                            tags= tags
                        )

    return pt


def createParmTemplateToggle(vrayPlugin, jsonParm):
    pt = hou.ToggleParmTemplate(
                            jsonParm["attr"],
                            jsonParm["attr"],
                            default_value=jsonParm["default"],
                            help=jsonParm["desc"]
                        )

    return pt


def createParmTemplateMenu(vrayPlugin, jsonParm):
    assert( jsonParm.has_key("items") )

    menuitems = map(lambda elem: elem[0] , jsonParm["items"])
    menulabels = map(lambda elem: elem[1] , jsonParm["items"])
    defaultval = jsonParm["default"]
    defaultidx = menuitems.index( str(defaultval) ) if menuitems.count( str(defaultval) ) else 0
    pt = hou.MenuParmTemplate(
                            jsonParm["attr"],
                            jsonParm["attr"],
                            # menuitems,
                            menulabels,
                            menu_labels=menulabels,
                            default_value=defaultidx,
                            help=jsonParm["desc"],
                            tags={
                            "vray_enumkeys": ",".join(menuitems),
                            }
                        )

    # ui = jsonParm.get("ui", None)
    # if ui:
    #     if ui.has_key("enum"):
    #         menuitems = map(lambda elem: elem[0] , ui["enum"])
    #         menulabels = map(lambda elem: elem[1] , ui["enum"])
    #         defaultidx = menuitems.index( str(defaultval) ) if menuitems.count( str(defaultval) ) else 0
    #         # pt.setMenuItems(menuitems)
    #         pt.setMenuItems(menulabels)
    #         pt.setMenuLabels(menulabels)
    #         pt.setMenuType(hou.menuType.Normal)
    #         pt.setDefaultValue(defaultidx)
    #         tags = pt.tags()
    #         tags["vray_enumkeys"] = ",".join(menuitems)
    #         pt.setTags(tags)

    #     if ui.has_key("string_enum"):
    #         menuitems = ui["string_enum"]
    #         defaultidx = menuitems.index( str(defaultval) ) if menuitems.count( str(defaultval) ) else 0
    #         pt.setMenuItems(menuitems)
    #         pt.setMenuType(hou.menuType.Normal)
    #         pt.setDefaultValue(defaultidx)
    #         tags = pt.tags()
    #         tags["vray_enumkeys"] = ",".join(menuitems)
    #         pt.setTags(tags)

    return pt


def createParmTemplateInt(vrayPlugin, jsonParm):
    defaultval = jsonParm["default"]
    pt = hou.IntParmTemplate(
                            jsonParm["attr"],
                            jsonParm["attr"],
                            1,
                            default_value=( defaultval, ),
                            naming_scheme=hou.parmNamingScheme.Base1,
                            help=jsonParm["desc"]
                        )

    ui = jsonParm.get("ui", None)
    if ui:
        # handle enum int parameters
        if ui.has_key("enum"):
            menuitems = map(lambda elem: elem[0] , ui["enum"])
            menulabels = map(lambda elem: elem[1] , ui["enum"])
            defaultidx = menuitems.index( str(defaultval) ) if menuitems.count( str(defaultval) ) else 0
            # pt.setMenuItems(menuitems)
            pt.setMenuItems(menulabels)
            pt.setMenuLabels(menulabels)
            pt.setMenuType(hou.menuType.Normal)
            pt.setDefaultValue( (defaultidx, ) )
            tags = pt.tags()
            tags["vray_enumkeys"] = ",".join(menuitems)
            pt.setTags(tags)

    adjustParmMinMax(pt, vrayPlugin, jsonParm)
    adjustParmUnits(pt, vrayPlugin, jsonParm)

    return pt


def createParmTemplateFloat(vrayPlugin, jsonParm):
    pt = hou.FloatParmTemplate(
                            jsonParm["attr"],
                            jsonParm["attr"],
                            1,
                            default_value=( jsonParm["default"], ),
                            look=hou.parmLook.Regular,
                            naming_scheme=hou.parmNamingScheme.Base1,
                            help=jsonParm["desc"]
                        )

    adjustParmMinMax(pt, vrayPlugin, jsonParm)
    adjustParmUnits(pt, vrayPlugin, jsonParm)

    return pt


def createParmTemplateVector(vrayPlugin, jsonParm):
    defaultval = jsonParm["default"]
    defaultval = defaultval + [1]*(3-len(defaultval))
    pt = hou.FloatParmTemplate(
                            jsonParm["attr"],
                            jsonParm["attr"],
                            3,
                            default_value=defaultval,
                            look=hou.parmLook.Vector,
                            naming_scheme=hou.parmNamingScheme.XYZW,
                            help=jsonParm["desc"]
                        )

    adjustParmMinMax(pt, vrayPlugin, jsonParm)
    adjustParmUnits(pt, vrayPlugin, jsonParm)

    return pt


def createParmTemplateColor(vrayPlugin, jsonParm):
    defaultval = jsonParm["default"]
    defaultval = defaultval + [1]*(3-len(defaultval))
    pt = hou.FloatParmTemplate(
                            jsonParm["attr"],
                            jsonParm["attr"],
                            3,
                            default_value=defaultval,
                            look=hou.parmLook.ColorSquare,
                            naming_scheme=hou.parmNamingScheme.RGBA,
                            help=jsonParm["desc"]
                        )

    adjustParmMinMax(pt, vrayPlugin, jsonParm)

    return pt


def createParmTemplateAColor(vrayPlugin, jsonParm):
    defaultval = jsonParm["default"]
    defaultval = defaultval + [1]*(4-len(defaultval))
    pt = hou.FloatParmTemplate(
                            jsonParm["attr"],
                            jsonParm["attr"],
                            4,
                            default_value=defaultval,
                            look=hou.parmLook.ColorSquare,
                            naming_scheme=hou.parmNamingScheme.RGBA,
                            help=jsonParm["desc"]
                        )

    adjustParmMinMax(pt, vrayPlugin, jsonParm)

    return pt


def createParmTemplateXOrder():
    # transformation order
    menuitems = (
        'srt',
        'str',
        'rst',
        'rts',
        'tsr',
        'trs'
    )
    menulabels = (
        'Scale Rotate Translate',
        'Scale Translate Rotate',
        'Rotate Scale Translate',
        'Rotate Translate Scale',
        'Translate Scale Rotate',
        'Translate Rotate Scale'
    )
    pt = hou.MenuParmTemplate(
                            "xOrd",
                            "Transform Order",
                            menuitems,
                            menu_labels=menulabels,
                            default_value=0
                        )
    return pt


def createParmTemplateROrder():
    # rotation order
    menuitems = (
        'xyz',
        'xzy',
        'yxz',
        'yzx',
        'zxy',
        'zyx'
    )
    menulabels = (
        'Rx Ry Rz',
        'Rx Rz Ry',
        'Ry Rx Rz',
        'Ry Rz Rx',
        'Rz Rx Ry',
        'Rz Ry Rx'
    )
    pt = hou.MenuParmTemplate(
                            "rOrd",
                            "Rotation Order",
                            menuitems,
                            menu_labels=menulabels,
                            default_value=0
                        )
    return pt


def createParmTemplateTransform(vrayPlugin, jsonParm):
    isMatrix = ( jsonParm["type"].lower().find("matrix") > -1)

    ptf = hou.FolderParmTemplate(
                            jsonParm["attr"],
                            jsonParm["attr"],
                            folder_type = hou.folderType.Simple
                        )
    # transformation order
    pt = createParmTemplateXOrder()
    pt.setName("{}_{}".format( jsonParm["attr"], pt.name() ))
    ptf.addParmTemplate(pt)
    # rotation order
    pt = createParmTemplateROrder()
    pt.setName("{}_{}".format( jsonParm["attr"], pt.name() ))
    ptf.addParmTemplate(pt)
    # translation
    pt = hou.FloatParmTemplate(
                            "{}_trans".format(jsonParm["attr"]),
                            "Translate",
                            3,
                            look=hou.parmLook.Regular,
                            naming_scheme=hou.parmNamingScheme.Base1,
                            is_hidden=isMatrix,
                            help=jsonParm["desc"]
                        )
    ptf.addParmTemplate(pt)
    # rotation
    pt = hou.FloatParmTemplate(
                            "{}_rot".format(jsonParm["attr"]),
                            "Rotate",
                            3,
                            look=hou.parmLook.Angle,
                            naming_scheme=hou.parmNamingScheme.Base1,
                            help=jsonParm["desc"]
                        )
    ptf.addParmTemplate(pt)
    # scale
    pt = hou.FloatParmTemplate(
                            "{}_scale".format(jsonParm["attr"]),
                            "Scale",
                            3,
                            default_value=(1,1,1),
                            look=hou.parmLook.Regular,
                            naming_scheme=hou.parmNamingScheme.Base1,
                            help=jsonParm["desc"]
                        )
    ptf.addParmTemplate(pt)
    # pivot
    pt = hou.FloatParmTemplate(
                            "{}_pivot".format(jsonParm["attr"]),
                            "Pivot",
                            3,
                            look=hou.parmLook.Regular,
                            naming_scheme=hou.parmNamingScheme.Base1,
                            is_hidden=isMatrix,
                            help=jsonParm["desc"]
                        )
    ptf.addParmTemplate(pt)

    return ptf


def createParmTemplateString(vrayPlugin, jsonParm):
    pt = hou.StringParmTemplate(
                            jsonParm["attr"],
                            jsonParm["attr"],
                            1,
                            default_value=( jsonParm["default"], ),
                            naming_scheme=hou.parmNamingScheme.Base1,
                            help=jsonParm["desc"]
                        )

    if jsonParm.has_key("subtype"):
        subtype = jsonParm["subtype"].lower()
        if subtype == "file_path":
            pt.setStringType(hou.stringParmType.FileReference)
            pt.setFileType(hou.fileType.Any)
        elif subtype == "dir_path":
            pt.setStringType(hou.stringParmType.FileReference)
            pt.setFileType(hou.fileType.Directory)

    ui = jsonParm.get("ui", None)
    if ui:
        if ui.has_key("file_extensions"):
            tags = pt.tags()
            tags["filechooser_pattern"] = ",".join( map(lambda x: "*.{}".format(x) ,ui["file_extensions"]) )

            if ui.has_key("file_names"):
                tags["vray_file_names"] = ",".join(ui["file_names"])

            if ui.has_key("file_op"):
                fileMode = {
                "save": "write",
                "load": "read",
                }
                tags["filechooser_mode"] = fileMode.get( ui["file_op"].lower(), "read" )

            pt.setTags(tags)
            pt.setStringType(hou.stringParmType.FileReference)
            pt.setFileType(hou.fileType.Any)

        if ui.has_key("string_enum"):
            menuitems = ui["string_enum"]
            pt.setMenuItems(menuitems)
            pt.setMenuType(hou.menuType.Normal)
            tags = pt.tags()
            tags["vray_enumkeys"] = ",".join(menuitems)
            pt.setTags(tags)

    return pt


def createParmTemplatePlugin(vrayPlugin, jsonParm):
    pt = hou.StringParmTemplate(
                            jsonParm["attr"],
                            jsonParm["attr"],
                            1,
                            string_type=hou.stringParmType.NodeReference,
                            naming_scheme=hou.parmNamingScheme.Base1,
                            help=jsonParm["desc"]
                        )

    ui = jsonParm.get("ui", None)
    if ui:
        if ui.has_key("attributes"):
            opfilters = map( lambda x : str(x).lower(), ui["attributes"][0::2])
            for opfilter in opfilters:
                if opfilter == "objectset":
                    tags = pt.tags()
                    tags["opfilter"] = "!!OBJ!!"
                    tags["oprelative"] = "/"
                    pt.setTags(tags)
                    pt.setStringType(hou.stringParmType.NodeReferenceList)
                elif opfilter == "lightset":
                    tags = pt.tags()
                    tags["opfilter"] = "!!OBJ/LIGHT!!"
                    tags["oprelative"] = "/"
                    pt.setTags(tags)
                    pt.setStringType(hou.stringParmType.NodeReferenceList)

    return pt


def createParmTemplatePluginList(vrayPlugin, jsonParm):
    pt = createParmTemplatePlugin(vrayPlugin, jsonParm)
    pt.setStringType(hou.stringParmType.NodeReferenceList)
    return pt


def createParmTemplateTexture(vrayPlugin, jsonParm):
    pt = createParmTemplateAColor(vrayPlugin, jsonParm)
    return pt


def createParmTemplateTextureInt(vrayPlugin, jsonParm):
    pt = createParmTemplateInt(vrayPlugin, jsonParm)
    return pt


def createParmTemplateTextureFloat(vrayPlugin, jsonParm):
    pt = createParmTemplateFloat(vrayPlugin, jsonParm)
    return pt


def createParmTemplateTextureVector(vrayPlugin, jsonParm):
    pt = createParmTemplateVector(vrayPlugin, jsonParm)
    return pt


def createParmTemplateTextureMatrix(vrayPlugin, jsonParm):
    pt = createParmTemplateTransform(vrayPlugin, jsonParm)
    return pt


def createParmTemplateTextureTransform(vrayPlugin, jsonParm):
    pt = createParmTemplateTransform(vrayPlugin, jsonParm)
    return pt


def createParmTemplateColorRamp(vrayPlugin, jsonParm):
    tags = {
    "rampcolordefault": "1pos ( 0 ) 1c ( 0 0 1 ) 1interp ( linear )  2pos ( 1 ) 2c ( 1 0 0 ) 2interp ( linear )",
    }
    attrs = jsonParm.get("attrs", {})
    if attrs.get("interpolations", None):
        tags["rampbasis_var"] = attrs.get("interpolations", None)

    if attrs.get("positions", None):
        tags["rampkeys_var"] = attrs.get("positions", None)

    if attrs.get("colors", None):
        tags["rampvalues_var"] = attrs.get("colors", None)

    pt = hou.RampParmTemplate(
                            jsonParm["attr"],
                            jsonParm["attr"],
                            hou.rampParmType.Color,
                            default_basis=hou.rampBasis.Linear,
                            help=jsonParm["desc"],
                            tags=tags
                        )

    return pt


def createParmTemplateFloatRamp(vrayPlugin, jsonParm):
    tags = {
        "rampshowcontrolsdefault": "1",
    }
    attrs = jsonParm.get("attrs", {})
    if attrs.get("interpolations", None):
        tags["rampbasis_var"] = attrs.get("interpolations", None)

    if attrs.get("positions", None):
        tags["rampkeys_var"] = attrs.get("positions", None)

    if attrs.get("values", None):
        tags["rampvalues_var"] = attrs.get("values", None)

    pt = hou.RampParmTemplate(
                            jsonParm["attr"],
                            jsonParm["attr"],
                            hou.rampParmType.Float,
                            default_basis=hou.rampBasis.Linear,
                            help=jsonParm["desc"],
                            tags=tags
                        )

    return pt


def createParmTemplateFloatTuple(vrayPlugin, jsonParm):
    pt = createParmTemplateFloat(vrayPlugin, jsonParm)

    ncnt = jsonParm.get("elements_count", 1) 
    ncnt = max(ncnt, 1)
    pt.setNumComponents(ncnt)

    return pt


def createParmTemplateStringList(vrayPlugin, jsonParm):
    pt = createParmTemplateString(vrayPlugin, jsonParm)
    pt.setName( "{}#".format(pt.name()) )

    adjustParmDisplayName(pt, vrayPlugin, jsonParm)

    ptf = hou.FolderParmTemplate(
                    jsonParm["attr"],
                    jsonParm["attr"],
                    parm_templates=(pt, ),
                    folder_type = hou.folderType.MultiparmBlock
                )

    return ptf


VHFPRM_Enum = plgutils.enum(
'Toggle',
'Menu',
'Int',
'Float',
'String',
'Vector',
'Color',
'AColor',
'Matrix',
'Transform',
'Texture',
'TextureInt',
'TextureFloat',
'TextureVector',
'TextureMatrix',
'TextureTransform',
'Plugin',
'PluginList',
'StringList',
'FloatTuple',
'FloatRamp',
'ColorRamp',
)


VFHPRM_Builder = {
VHFPRM_Enum.Toggle:           createParmTemplateToggle,
VHFPRM_Enum.Menu:             createParmTemplateMenu,
VHFPRM_Enum.Int:              createParmTemplateInt,
VHFPRM_Enum.Float:            createParmTemplateFloat,
VHFPRM_Enum.String:           createParmTemplateString,
VHFPRM_Enum.Vector:           createParmTemplateVector,
VHFPRM_Enum.Color:            createParmTemplateColor,
VHFPRM_Enum.AColor:           createParmTemplateAColor,
VHFPRM_Enum.Matrix:           createParmTemplateTransform,
VHFPRM_Enum.Transform:        createParmTemplateTransform,
VHFPRM_Enum.Texture:          createParmTemplateTexture,
VHFPRM_Enum.TextureInt:       createParmTemplateTextureInt,
VHFPRM_Enum.TextureFloat:     createParmTemplateTextureFloat,
VHFPRM_Enum.TextureVector:    createParmTemplateTextureVector,
VHFPRM_Enum.TextureMatrix:    createParmTemplateTextureMatrix,
VHFPRM_Enum.TextureTransform: createParmTemplateTextureTransform,
VHFPRM_Enum.Plugin:           createParmTemplatePlugin,
VHFPRM_Enum.PluginList:       createParmTemplatePluginList,
VHFPRM_Enum.StringList:       createParmTemplateStringList,
VHFPRM_Enum.FloatTuple:       createParmTemplateFloatTuple,
VHFPRM_Enum.FloatRamp:        createParmTemplateFloatRamp,
VHFPRM_Enum.ColorRamp:        createParmTemplateColorRamp,
}


VRAYTYPE_TO_VFHPRM={
'boolean' :          VHFPRM_Enum.Toggle,
'int' :              VHFPRM_Enum.Int,
'float' :            VHFPRM_Enum.Float,
'double' :           VHFPRM_Enum.Float,
'String' :           VHFPRM_Enum.String,
'Vector' :           VHFPRM_Enum.Vector,
'Color' :            VHFPRM_Enum.Color,
'AColor' :           VHFPRM_Enum.AColor,
'Matrix' :           VHFPRM_Enum.Matrix,
'Transform' :        VHFPRM_Enum.Transform,
'Texture' :          VHFPRM_Enum.Texture,
'TextureInt' :       VHFPRM_Enum.TextureInt,
'TextureFloat' :     VHFPRM_Enum.TextureFloat,
'TextureVector' :    VHFPRM_Enum.TextureVector,
'TextureMatrix' :    VHFPRM_Enum.TextureMatrix,
'TextureTransform' : VHFPRM_Enum.TextureTransform,
'Object' :           VHFPRM_Enum.Plugin,
'List<String>' :     VHFPRM_Enum.StringList,
'List<Object>' :     VHFPRM_Enum.PluginList,
}


# DONE need a list of all possible jsonParm types and subtypes
# DONE need mapping between jsonParm parm creator function
# DONE handle all possible jsonParm attributes

JSONTYPE_TO_VFHPRM={
'BOOL' : {
        'undefined' : VHFPRM_Enum.Toggle,
        'boolean' : VHFPRM_Enum.Toggle,
        'int' : VHFPRM_Enum.Toggle,
        'float' : VHFPRM_Enum.Float,
        'List<Boolean>' : VHFPRM_Enum.Toggle,
},
'ENUM' : {
        'undefined' : VHFPRM_Enum.Menu,
        'boolean' : VHFPRM_Enum.Menu,
        'int' : VHFPRM_Enum.Menu,
        'String' : VHFPRM_Enum.Menu,
        'List<Integer>' : VHFPRM_Enum.Menu,
},
'INT' : {
        'undefined' : VHFPRM_Enum.Int,
        'int' : VHFPRM_Enum.Int,
        'boolean' : VHFPRM_Enum.Int,
        'List<Integer>' : VHFPRM_Enum.Int,
},
'FLOAT' : {
        'undefined' : VHFPRM_Enum.Float,
        'float' : VHFPRM_Enum.Float,
        'double' : VHFPRM_Enum.Float,
        'List<Float>' : VHFPRM_Enum.Float,
        'Texture' : VHFPRM_Enum.Texture,
        'TextureFloat' : VHFPRM_Enum.TextureFloat,
},
'STRING' : {
        'undefined' : VHFPRM_Enum.String,
        'String' : VHFPRM_Enum.String,
        'List<Object>' : VHFPRM_Enum.PluginList,
        'List<Integer>' : VHFPRM_Enum.String,
},
'VECTOR' : {
        'undefined' : VHFPRM_Enum.Vector,
        'Vector' : VHFPRM_Enum.Vector,
},
'COLOR' : {
        'undefined' : VHFPRM_Enum.Color,
        'Color' : VHFPRM_Enum.Color,
        'AColor' : VHFPRM_Enum.AColor,
        'OutputTexture' : None,
        'List<Color>' : VHFPRM_Enum.Color,
},
'ACOLOR' : {
        'undefined' : VHFPRM_Enum.AColor,
        'AColor' : VHFPRM_Enum.AColor,
},
'MATRIX' : {
        'undefined' : None, # VHFPRM_Enum.Matrix,
        'Matrix' : None,    # VHFPRM_Enum.Matrix,
},
'TRANSFORM' : {
        'undefined' : None, # VHFPRM_Enum.Transform,
        'Transform' : None, # VHFPRM_Enum.Transform,
},
'TEXTURE' : {
        'undefined' : VHFPRM_Enum.Texture,
        'Texture' : VHFPRM_Enum.Texture,
},
'INT_TEXTURE' : {
        'undefined' : VHFPRM_Enum.TextureInt,
        'TextureInt' : VHFPRM_Enum.TextureInt,
},
'FLOAT_TEXTURE' : {
        'undefined' : VHFPRM_Enum.TextureFloat,
        'TextureFloat' : VHFPRM_Enum.TextureFloat,
        'List<TextureFloat>' : VHFPRM_Enum.TextureFloat,
},
'VECTOR_TEXTURE' : {
        'undefined' : VHFPRM_Enum.TextureVector,
        'TextureVector' : VHFPRM_Enum.TextureVector,
},
'MATRIX_TEXTURE' : {
        'undefined' : None,     # VHFPRM_Enum.TextureMatrix,
        'TextureMatrix' : None, # VHFPRM_Enum.TextureMatrix,
},
'TRANSFORM_TEXTURE' : {
        'undefined' : None,        # VHFPRM_Enum.TextureTransform,
        'TextureTransform' : None, # VHFPRM_Enum.TextureTransform,
},
'PLUGIN_LIST' : {
        'undefined': None, # VHFPRM_Enum.PluginList,
        'List<Object>' : None ,# VHFPRM_Enum.PluginList,
        'Object' : None, # VHFPRM_Enum.Plugin,
},
'STRING_LIST' : {
        'undefined': None, # VHFPRM_Enum.StringList,
        'List<String>' : None # VHFPRM_Enum.StringList,
},
'MATERIAL' : {
        'undefined' : None, # VHFPRM_Enum.Plugin,
        'Object' : None, # VHFPRM_Enum.Plugin,
},
'BRDF' : {
        'undefined' : None, # VHFPRM_Enum.Plugin,
        'Object' : None, # VHFPRM_Enum.Plugin,
},
'UVWGEN' : {
        'undefined' : None, # VHFPRM_Enum.Plugin,
        'Object' : None, # VHFPRM_Enum.Plugin,
},
'GEOMETRY' : {
        'undefined' : None, # VHFPRM_Enum.Plugin,
        'Object' : None, # VHFPRM_Enum.Plugin,
},
'PLUGIN' : {
        'undefined' : None, # VHFPRM_Enum.Plugin,
        'Object' : None, # VHFPRM_Enum.Plugin,
        'List<Object>' : None, # VHFPRM_Enum.PluginList,
},
'LIST' : {
        'undefined' : None,
        'Object' : None, # VHFPRM_Enum.Plugin,
        'List<Float>' : None,
        'List' : None,
        'List<TextureFloat>' : None,
        'List<Texture>' : None,
        'List<Integer>' : None,
        'List<Object>' : None, # VHFPRM_Enum.PluginList,
        'List<String>' : None, # VHFPRM_Enum.StringList,
},
'WIDGET_CURVE' : {
        'undefined' : VHFPRM_Enum.FloatRamp,
},
'WIDGET_RAMP' : {
        'undefined' : VHFPRM_Enum.ColorRamp,
        'boolean' : VHFPRM_Enum.ColorRamp,
},
}


VFHROP_SettingsTabs={
"SettingsCamera"           : "Camera",
"SettingsCameraDof"        : "Depth Of Field",
"SettingsMotionBlur"       : "Motion Blur",
"VRayStereoscopicSettings" : "Stereo",

"SettingsGI"               : "GI",
"SettingsDMCGI"            : "Brute Force",
"SettingsIrradianceMap"    : "Irradiance Map",
"SettingsLightCache"       : "Light Cache",

"SettingsDMCSampler"       : "DMC",
"SettingsImageSampler"     : "AA",

"SettingsOptions"             : "Options",
"SettingsOutput"              : "Output",
"SettingsColorMapping"        : "Color Mapping",
"SettingsRaycaster"           : "Raycaster",
"SettingsRegionsGenerator"    : "Regions",
"SettingsRTEngine"            : "RT",
"SettingsCaustics"            : "Caustics",
"SettingsDefaultDisplacement" : "Displacement",
}

VFHLights={
"LightAmbient" : 1,
"LightAmbientMax" : 1,
"LightDirect" : 1,
"LightDirectMax" : 1,
"LightDirectModo" : 1,
"LightDome" : 1,
"LightIES" : 1,
"LightIESMax" : 1,
"LightMesh" : 1,
"LightOmni" : 1,
"LightOmniMax" : 1,
"LightRectangle" : 1,
"LightSphere" : 1,
"LightSpot" : 1,
"LightSpotMax" : 1,
"MayaLightLirect" : 1,
"SunLight" : 1,
}


def addPluginParmTags(pt, vrayPlugin, jsonParm):
    parmtoken = jsonParm["attr"]

    assert( vrayPlugin )
    assert( getattr(vrayPlugin, parmtoken, None) )

    pluginattr_tags = {
    "vray_plugin" : vrayPlugin.getName(),
    "vray_pluginattr" : parmtoken,
    "vray_type" : vrayPlugin[ parmtoken ]["type"],
    }

    tags = pt.tags()
    tags.update(pluginattr_tags)
    pt.setTags( tags )
    if pt.type() == hou.parmTemplateType.Folder:
        for prmTmpl in pt.parmTemplates():
            tags = prmTmpl.tags()
            tags.update(pluginattr_tags)
            prmTmpl.setTags( tags )

    return pt


def addParmTags(pt, vrayPlugin, jsonParm):
    custom_tags={
    "cook_dependent":"1",
    }

    # handle "skip"
    skipAutoExport = jsonParm.get("skip", False)
    if skipAutoExport:
        custom_tags["vray_custom_handling"] = "1"
    # handle "options"
    options = map(lambda x: str(x).lower(), jsonParm.get("options", []))
    if options.count("linked_only"):
        custom_tags["vray_linked_only"] = "1"

    if options.count("export_as_radians"):
        custom_tags["vray_units"] = "radians"

    tags = pt.tags()
    tags.update(custom_tags)
    pt.setTags( tags )
    if pt.type() == hou.parmTemplateType.Folder:
        for prmTmpl in pt.parmTemplates():
            tags = prmTmpl.tags()
            tags.update(custom_tags)
            prmTmpl.setTags( tags )

    return pt


def getPRMBuilder(vrayPlugin, jsonParm):
    # returns creator function based on
    # jsonParm type mapping configured in JSONTYPE_TO_VFHPRM
    mappedTypes = JSONTYPE_TO_VFHPRM.get( jsonParm["type"], None )
    if not mappedTypes:
        return None

    vrayParm = getattr(vrayPlugin, jsonParm["attr"], None)
    vrayType = vrayParm["type"] if vrayParm else 'undefined'
    builderID = mappedTypes.get( vrayType, None )

    if vrayPlugin:
        if vrayPlugin.getName() == "LightMesh" and jsonParm["attr"] == "geometry":
            builderID = VHFPRM_Enum.Plugin
        elif vrayPlugin.getName() == "VRayClipper" and jsonParm["attr"] == "clip_mesh":
            builderID = VHFPRM_Enum.Plugin
        elif vrayPlugin.getName() == "VRayClipper" and jsonParm["attr"] == "exclusion_nodes":
            builderID = VHFPRM_Enum.PluginList
        elif vrayPlugin.getName() == "VRayStereoscopicSettings" and jsonParm["attr"] == "left_camera":
            builderID = VHFPRM_Enum.Plugin
        elif vrayPlugin.getName() == "VRayStereoscopicSettings" and jsonParm["attr"] == "right_camera":
            builderID = VHFPRM_Enum.Plugin
        elif VFHLights.get(vrayPlugin.getName(), None) and jsonParm["type"].lower().find("texture") > -1:
            builderID = VHFPRM_Enum.Plugin


    # TODO FIX : consider taking builderID from VRAYTYPE_TO_VFHPRM
    #       if builderID is None at this point
    return VFHPRM_Builder.get(builderID, None)


def createParmTemplate(vrayPlugin, jsonParm):
    builder = getPRMBuilder(vrayPlugin, jsonParm)
    if not builder:
        return None

    pt = builder(vrayPlugin, jsonParm)
    if not pt:
        return None

    adjustParmDisplayName(pt, vrayPlugin, jsonParm)

    vrayParm = getattr(vrayPlugin, jsonParm["attr"], None)
    # if we have corresponding plugin parameter add vray plugin attribute tags
    if vrayParm:
        addPluginParmTags(pt, vrayPlugin, jsonParm)

    addParmTags(pt, vrayPlugin, jsonParm)

    return pt


def getAsHouConditionalList(ptg, pt, cond, prefix):
    tmplist = []
    values = cond["values"]
    for i, value in enumerate(values):
        if isinstance(value, bool):
            value = int(value)

        parm = cond["parm"]
        parmowner = cond["parmowner"]
        if not parmowner:
            parmowner = prefix

        if parmowner:
            parm = "{}_{}".format( parmowner, parm )

        ptother = ptg.find(parm)
        if ptother:
            try:
                menuitems = ptother.menuItems()
                enumkeys = ptother.tags().get("vray_enumkeys", "").split(",")
                idx = enumkeys.index(str(value))
                value = menuitems[idx]
            except Exception as err:
                pass

        if isstr(value):
            value = "\"{}\"".format(value)

        conditional = "{} {} {}".format( parm, cond["op"], value)

        tmplist += (conditional, )

    return tmplist


def getAsHouConditional(ptg, pt, conditionalType, conditions, prefix):
    condlist = []
    for cond in conditions:
        tmplist = getAsHouConditionalList(ptg, pt, cond, prefix)

        opvalue = cond["opvalue"]
        if opvalue == "or":
            condlist += tmplist
        else :
            # opvalue == "and" and default 
            conditional = " ".join(tmplist)
            condlist += ( conditional, )

    condlist = map( lambda x: "{{ {} }}".format( x ), condlist )

    ptconditional = pt.conditionals().get( conditionalType, "")
    condlist = filter( lambda x: ptconditional.find(x) < 0, condlist )

    conditional = " ".join(condlist)
    return conditional


def addHouConditional(ptg, pt, conditionalType, conditions, prefix):
    conditional = getAsHouConditional( ptg, pt, conditionalType, conditions, prefix )
    if conditional:
        ptconditional = pt.conditionals().get(conditionalType, "")
        conditional = "{} {}".format(ptconditional, conditional).strip()
        pt.setConditional(conditionalType, conditional)


def convertCond(cond):
    assert(cond)

    op = cond["op"]
    if op == "==":
        op = "!="
    elif op == "!=":
        op = "=="
    elif op == ">":
        op = "<="
    elif op == ">=":
        op = "<"
    elif op == "<":
        op = ">="
    elif op == "<=":
        op = ">"
    else:
        op = "=="

    cond["op"] = op
    cond["values"] = [ cond["value"] ]

    return cond


def addHouConditional2(ptg, pt, conditionalType, conditions, prefix):
    condlist = [""]
    for orcond in conditions:
        tmplist = []
        for andcondmem in orcond:
            houcondlist = getAsHouConditionalList( ptg, pt, convertCond(andcondmem), prefix )
            conditional = " ".join(houcondlist)
            for cond in condlist:
                cond = "{} {}".format(cond, conditional).strip()
                tmplist.append(cond)

        condlist = tmplist

    condlist = filter( lambda x: bool(x), condlist )
    condlist = map( lambda x: "{{ {} }}".format( x ), condlist )

    ptconditional = pt.conditionals().get( conditionalType, "")
    condlist = filter( lambda x: ptconditional.find(x) < 0, condlist )

    conditional = " ".join(condlist)
    if conditional:
        ptconditional = pt.conditionals().get(conditionalType, "")
        conditional = "{} {}".format(ptconditional, conditional).strip()
        pt.setConditional(conditionalType, conditional)


def adjustParmConditionals(ptg, pt, jsonParm, parmConditions, prefix):
    cond = parmConditions.get(jsonParm["attr"], None)
    if cond:
        disableCond = cond.get("disable_when", None)
        if disableCond:
            addHouConditional( ptg, pt, hou.parmCondType.DisableWhen, disableCond, prefix )

        hideCond = cond.get("hide_when", None)
        if hideCond:
            addHouConditional( ptg, pt, hou.parmCondType.HideWhen, hideCond, prefix )

    ui = jsonParm.get("ui", None)
    if ui:
        enabledCond = ui.get("enabled", None)
        if enabledCond and hasattr(enabledCond, '__iter__'):
            addHouConditional2( ptg, pt, hou.parmCondType.DisableWhen, enabledCond, prefix )

    return pt


def adjustConditionals(ptg, vrayPlugin, jsonDesc):
    # if we are processing on of the plugins shown on the VRay ROP node
    # adjust plugin name
    # prefix all parameters
    prefix = None
    if VFHROP_SettingsTabs.has_key(jsonDesc["ID"]):
        prefix = jsonDesc["ID"]

    # get parameter conditions set in jsonDesc "Widget"
    parmConditions = plgutils.getJSONParmConditions(vrayPlugin, jsonDesc)
    for jsonParm in jsonDesc["Parameters"]:
        ptname = jsonParm["attr"]
        if prefix:
            ptname = "{}_{}".format(prefix, ptname)

        pt = ptg.find(ptname)
        if not pt:
            continue

        adjustParmConditionals(ptg, pt, jsonParm, parmConditions, prefix)
        ptg.replace(pt, pt)


def isValidHoudiniParm(jsonParm):
    available = jsonParm.get("available", None)
    if not available:
        return True

    return available.count("HOUDINI") > 0


def renameDuplicateParmTemplate(ptg, pt):
    assert( ptg.find(pt.name()) != None )

    i = 1
    while i < 100:
        parmName = "{}{}".format(pt.name(),str(i))
        i += 1
        if not ptg.find(parmName):
            pt.setName(parmName)
            break

    return pt


def renameParmTemplate(pt, prefix):
    if not prefix:
        return pt

    ptname = pt.name()
    if not ptname.startswith(prefix):
        pt.setName("{}_{}".format(prefix, ptname))

    return pt


def createParmTemplateGroup(vrayPlugin, jsonDesc):
    print("!!INFO!! Create ParmTemplateGroup for {}".format(jsonDesc["ID"]))

    ptg = hou.ParmTemplateGroup()

    # if we are processing on of the plugins shown on the VRay ROP node
    # adjust plugin name
    # prefix all parameters
    prefix = None
    if VFHROP_SettingsTabs.has_key(jsonDesc["ID"]):
        jsonDesc["Name"] = VFHROP_SettingsTabs[ jsonDesc["ID"] ]
        prefix = jsonDesc["ID"]

    currfolder_pt = tuple()

    # add plugin data prm as first parameter
    # every plugin has invisible parm that holds
    # additional plugin info in it and its spare data
    # pt = createParmTemplatePluginData(vrayPlugin, jsonDesc)
    # renameParmTemplate(pt, prefix)
    # ptg.addParmTemplate( pt)


    # add parameters
    for jsonParm in jsonDesc["Parameters"]:
        # handle "available" json attribute
        if not isValidHoudiniParm(jsonParm):
            print("!!INFO!! Skipping invalid Houdini parm {}".format(jsonParm["attr"]))
            continue

        vrayParm = getattr(vrayPlugin, jsonParm["attr"], None)
        if vrayParm:
            plgutils.updateUIGuides(vrayParm, jsonParm)

            # update desc from vrayPlugin parm if jsonParm["desc"] is empty
            if not jsonParm["desc"]:
                jsonParm["desc"] = vrayParm["description"]

        pt = createParmTemplate(vrayPlugin, jsonParm)
        if not pt:
            print("!!WARNING!! No Parameter Template created for json parameter {}::{} of type {}".format(
                                                                    jsonDesc["ID"],
                                                                    jsonParm["attr"],
                                                                    jsonParm["type"]
                                                                    ))
            continue

        ##### handle ui.tab and ui.rollout-just ignore this for now
        # ui = jsonParm.get("ui", None)
        # if ui:
        #     ptf = None
        #     tab_attr = ui.get("tab", None)
        #     if tab_attr:
        #         ptf = hou.FolderParmTemplate("folder", tab_attr)

        #     rollout_attr = ui.get("rollout", None)
        #     if rollout_attr:
        #         if rollout_attr.lower().find("endrollout") > -1:
        #             # currfolder_pt = tuple()
        #             pass
        #         else:
        #             ptf = hou.FolderParmTemplate("folder", rollout_attr)

        #     if ptf:
        #         ptg.addParmTemplate(ptf)
        #         currfolder_pt = ( ptf.label(), )

        # rename parameter if it belongs to one of the plugins shown on the VRay ROP node
        renameParmTemplate(pt, prefix)

        # parm with this name already exists
        # => we need to rename it
        if ptg.find(pt.name()):
            print("!!WARNING!! Found duplicate parameter with name {}::{} of type {}".format(
                                                                    jsonDesc["ID"],
                                                                    jsonParm["attr"],
                                                                    jsonParm["type"]
                                                                    ))
            renameDuplicateParmTemplate(ptg, pt)
            assert( ptg.find(pt.name()) == None )

        folder = ptg.findFolder(currfolder_pt)
        if folder:
            ptg.appendToFolder(folder, pt)
        else:
            ptg.addParmTemplate(pt)

    # handle enabled and visible conditions
    adjustConditionals(ptg, vrayPlugin, jsonDesc)

    return ptg
