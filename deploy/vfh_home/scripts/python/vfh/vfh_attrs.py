#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import sys
import re
import hou

import vfh.vfh_json as vfh_json


def BoolParmTemplate(parmName, parmLabel, parmDesc):
    parmArgs = {}
    if 'default' in parmDesc:
        parmArgs['default_value'] = parmDesc['default']

    return hou.ToggleParmTemplate(parmName, parmLabel, **parmArgs)


def IntParmTemplate(parmName, parmLabel, parmDesc):
    parmArgs = {}
    parmArgs['naming_scheme'] = hou.parmNamingScheme.Base1
    parmArgs['num_components'] = 1

    if 'default' in parmDesc:
        parmArgs['default_value'] = [parmDesc['default']]

    if 'ui' in parmDesc:
        parmArgs['min_is_strict'] = False
        parmArgs['max_is_strict'] = False

        ui_desc = parmDesc['ui']
        # Use soft bounds and allow manual override
        if 'soft_min' in ui_desc:
            parmArgs['min'] = ui_desc['soft_min']
        if 'soft_max' in ui_desc:
            parmArgs['max'] = ui_desc['soft_max']

    return hou.IntParmTemplate(parmName, parmLabel, **parmArgs)


def FloatParmTemplate(parmName, parmLabel, parmDesc):
    parmArgs = {}
    parmArgs['naming_scheme'] = hou.parmNamingScheme.Base1
    parmArgs['num_components'] = 1

    if 'default' in parmDesc:
        parmArgs['default_value'] = [parmDesc['default']]

    if 'ui' in parmDesc:
        parmArgs['min_is_strict'] = False
        parmArgs['max_is_strict'] = False

        ui_desc = parmDesc['ui']
        # Use soft bounds and allow manual override
        if 'soft_min' in ui_desc:
            parmArgs['min'] = ui_desc['soft_min']
        if 'soft_max' in ui_desc:
            parmArgs['max'] = ui_desc['soft_max']

    return hou.FloatParmTemplate(parmName, parmLabel, **parmArgs)


def EnumParmTemplate(parmName, parmLabel, parmDesc):
    parmArgs = {}
    parmArgs['menu_items']  = [item[0] for item in parmDesc['items']]
    parmArgs['menu_labels'] = [item[1] for item in parmDesc['items']]
    parmArgs['default_value'] = next((i for i, x in enumerate(parmArgs['menu_items']) if x == parmDesc['default']), 0)
    return hou.MenuParmTemplate(parmName, parmLabel, **parmArgs)


def VectorParmTemplate(parmName, parmLabel, parmDesc):
    v = parmDesc['default']
    parmArgs = {}
    parmArgs['naming_scheme'] = hou.parmNamingScheme.XYZW
    parmArgs['default_value'] = v
    parmArgs['num_components'] = len(v)
    return hou.FloatParmTemplate(parmName, parmLabel, **parmArgs)


def ColorParmTemplate(parmName, parmLabel, parmDesc):
    v = parmDesc['default']
    parmArgs = {}
    parmArgs['naming_scheme'] = hou.parmNamingScheme.RGBA
    parmArgs['default_value']  = v
    parmArgs['num_components'] = len(v)
    return hou.FloatParmTemplate(parmName, parmLabel, **parmArgs)


def AColorParmTemplate(parmName, parmLabel, parmDesc):
    v = parmDesc['default']
    parmArgs = {}
    parmArgs['naming_scheme'] = hou.parmNamingScheme.RGBA
    parmArgs['default_value']  = v
    parmArgs['num_components'] = len(v)
    return hou.FloatParmTemplate(parmName, parmLabel, **parmArgs)


def StringParmTemplate(parmName, parmLabel, parmDesc):
    string_type = hou.stringParmType.Regular
    file_type   = hou.fileType.Any

    if 'subtype' in parmDesc:
        str_subtype = parmDesc['subtype']
        if str_subtype == 'FILE_PATH':
            string_type = hou.stringParmType.FileReference
            file_type   = hou.fileType.Any
        elif str_subtype == 'DIR_PATH':
            string_type = hou.stringParmType.FileReference
            file_type   = hou.fileType.Directory

    parmArgs = {}
    parmArgs['string_type'] = string_type
    parmArgs['file_type']   = file_type
    parmArgs['num_components'] = 1
    return hou.StringParmTemplate(parmName, parmLabel, **parmArgs)


def TextureParmTemplate(parmName, parmLabel, parmDesc):
    parmArgs = {}
    parmArgs["string_type"] = hou.stringParmType.NodeReference
    parmArgs["tags"] = {'spare_category': 'vray', 'opfilter': '!!VOP!!', 'oprelative': '.'}
    parmArgs["script_callback_language"] = hou.scriptLanguage.Python
    parmArgs['num_components'] = 1
    return hou.StringParmTemplate(parmName, parmLabel, **parmArgs)


def FloatTextureParmTemplate(parmName, parmLabel, parmDesc):
    return TextureParmTemplate(parmName, parmLabel, parmDesc)


# When there is no name specified for the attribute we could "guess" the name
# from the attribute like: 'dist_near' will become "Dist Near"
#
def parmNameToParmLabel(parmName):
    attr_name = parmName.replace("_", " ")
    attr_name = re.sub(r"\B([A-Z])", r" \1", attr_name)
    return attr_name.title()


CustomParmTemplates = {
    'BOOL'          : BoolParmTemplate,
    'INT'           : IntParmTemplate,
    'FLOAT'         : FloatParmTemplate,
    'ENUM'          : EnumParmTemplate,
    'VECTOR'        : VectorParmTemplate,
    'COLOR'         : ColorParmTemplate,
    'ACOLOR'        : AColorParmTemplate,
    'STRING'        : StringParmTemplate,
    'TEXTURE'       : TextureParmTemplate,
    'FLOAT_TEXTURE' : FloatTextureParmTemplate
}


def getFirstNonTabParmTemplate(ptf):
    return next((pt for pt in ptf.parmTemplates() if pt.type() != hou.parmTemplateType.Folder or pt.folderType() != hou.folderType.Tabs), None)


def insertInFolderAfterLastTab(ptg, ptf, pt):
    p = getFirstNonTabParmTemplate(ptf)
    if p:
        ptg.insertBefore(p, pt)
        return

    if isinstance(ptf, hou.ParmTemplateGroup):
        ptg.append(pt)
        return

    if isinstance(ptf, hou.FolderParmTemplate):
        ptg.appendToFolder(ptf, pt)


def addPluginParm(ptg, parmDesc, parmPrefix = None, parmFolder = None):
    parmName = "%s_%s" % (parmPrefix, parmDesc['attr']) if parmPrefix else parmDesc['attr']
    parmTemplate = ptg.find(parmName)
    if not parmTemplate:
        parmType = parmDesc['type']
        if parmType in CustomParmTemplates:
            if not parmFolder and not ptg.findFolder('V-Ray'):
                parmFolder = 'V-Ray'
                insertInFolderAfterLastTab(ptg, ptg, hou.FolderParmTemplate("vray", "V-Ray"))

            parmLabel = parmDesc.get('name', parmNameToParmLabel(parmDesc['attr']))
            MyTemplate = CustomParmTemplates[parmType]
            parmTemplate = MyTemplate(parmName, parmLabel, parmDesc)
            ptg.appendToFolder(parmFolder, parmTemplate)
        else:
            sys.stderr.write("Unimplemented ParmTemplate for plugin parm type %s!\n" % parmType)

    return parmTemplate


def addPluginParms(ptg, pluginDesc, parmPrefix = None, parmFolder = None):
    for parmDesc in pluginDesc['PluginParams']:
        addPluginParm(ptg, parmDesc, parmPrefix = parmPrefix, parmFolder = parmFolder)


