#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#


import hou

from vfh import vfh_json
from vfh import vfh_attrs


def addVRayDisplamentParamTemplate(ptg):
    if not ptg.findFolder("V-Ray"):
        vfh_attrs.insertInFolderAfterLastTab(ptg, ptg, hou.FolderParmTemplate("vray", "V-Ray"))

    if not ptg.findFolder(("V-Ray", "Displacement")):
        vfh_attrs.insertInFolderAfterLastTab(ptg, ptg.findFolder('V-Ray'), hou.FolderParmTemplate("vray", "Displacement"))

    folder = ("V-Ray", "Displacement")
    # enable displacement
    if not ptg.find("vray_use_displ"):
        params = { }
        params["tags"] = {'spare_category': 'vray'}
        params["default_value"] = False
        params["script_callback_language"] = hou.scriptLanguage.Python
        ptg.appendToFolder(folder, hou.ToggleParmTemplate("vray_use_displ", "Use Displacement", **params))

    # displacement type
    if not ptg.find("vray_displ_type"):
        params = { }
        params["tags"] = {'spare_category': 'vray'}
        params["default_value"] = 0
        params["menu_items"]=(['0', '1', '2'])
        params["menu_labels"]=(['From Shop Net', 'Displaced', 'Smoothed'])
        params["conditionals"]={hou.parmCondType.DisableWhen: "{ vray_use_displ == 0 }"}
        params["script_callback_language"] = hou.scriptLanguage.Python
        ptg.appendToFolder(folder, hou.MenuParmTemplate("vray_displ_type", "Displacement Type", **params))

    # params for vray_displ_type = 'shopnet'
    if not ptg.findFolder("shopnet"):
        params = { }
        params["tags"] = {'spare_category': 'vray'}
        params["folder_type"] = hou.folderType.Simple
        params["conditionals"]={hou.parmCondType.HideWhen: "{ vray_displ_type != 0 }"}
        ptg.appendToFolder(folder, hou.FolderParmTemplate("vray", "shopnet",**params))

    shopnetFolder = ("V-Ray", "Displacement", "shopnet")
    if not ptg.find("vray_displshoppath"):
        params = { }
        params["string_type"] = hou.stringParmType.NodeReference
        params["tags"] = {'spare_category': 'vray', 'opfilter': '!!SHOP!!', 'oprelative': '.'}
        params["script_callback_language"] = hou.scriptLanguage.Python
        params["conditionals"]={hou.parmCondType.HideWhen: "{ vray_displ_type != 0 }", hou.parmCondType.DisableWhen: "{ vray_use_displ == 0 }"}
        ptg.appendToFolder(shopnetFolder, hou.StringParmTemplate("vray_displshoppath", "Shop path", 1, **params))

        # params for vray_displ_type = 'GeomDisplacedMesh'
    if not ptg.findFolder("gdm"):
        params = { }
        params["tags"] = {'spare_category': 'vray'}
        params["folder_type"] = hou.folderType.Simple
        params["conditionals"]={hou.parmCondType.HideWhen: "{ vray_displ_type != 1 }"}
        ptg.appendToFolder(folder, hou.FolderParmTemplate("vray", "gdm",**params))

    gdmFolder = ("V-Ray", "Displacement", "gdm")
    displDesc = vfh_json.getPluginDesc('GeomDisplacedMesh')
    paramDescList = filter(lambda parmDesc: parmDesc['attr'] != 'displacement_tex_float', displDesc['PluginParams'])
    for parmDesc in paramDescList:
        vfh_attrs.addPluginParm(ptg, parmDesc, parmPrefix = 'GeomDisplacedMesh', parmFolder = gdmFolder)

    # params for vray_displ_type = 'GeomStaticSmoothedMesh'
    if not ptg.findFolder("gssm"):
        params = { }
        params["tags"] = {'spare_category': 'vray'}
        params["folder_type"] = hou.folderType.Simple
        params["conditionals"]={hou.parmCondType.HideWhen: "{ vray_displ_type != 2}"}
        ptg.appendToFolder(folder, hou.FolderParmTemplate("vray", "gssm",**params))

    gssmFolder = ("V-Ray", "Displacement", "gssm")

    subdivDesc = vfh_json.getPluginDesc('GeomStaticSmoothedMesh')
    paramDescList = filter(lambda parmDesc: parmDesc['attr'] != 'displacement_tex_float', subdivDesc['PluginParams'])
    for parmDesc in paramDescList:
        vfh_attrs.addPluginParm(ptg, parmDesc, parmPrefix = 'GeomStaticSmoothedMesh', parmFolder = gssmFolder)


def addVRayDisplamentParams(node):
    ptg = node.parmTemplateGroup()
    addVRayDisplamentParamTemplate(ptg)
    node.setParmTemplateGroup(ptg)