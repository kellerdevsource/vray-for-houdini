#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import hou
import os

from vfh import vfh_attrs
from vfh import vfh_json

HOUDINI_SOHO_DEVELOPER = os.environ.get("HOUDINI_SOHO_DEVELOPER", False)
if HOUDINI_SOHO_DEVELOPER:
    reload(vfh_attrs)
    reload(vfh_json)

UI = os.environ.get('VRAY_UI_DS_PATH', None)

VRAY_FOLDER = "V-Ray"
DISPLACEMENT_FOLDER = ("V-Ray", "Displacement")

def ptgParmTemplatesIt(ptg):
    for pt in ptg.parmTemplates():
        yield pt

        if pt.type() == hou.parmTemplateType.Folder and pt.isActualFolder():
            for _pt in ptgParmTemplatesIt(pt):
                yield _pt

def _getPluginParmTemplates(pluginName):
    pluginDs = os.path.join(UI, "plugins", "%s.ds" % pluginName)
    if not os.path.exists(pluginDs):
        return

    pluginPtg = hou.ParmTemplateGroup()

    # This will make attribute manes prefixed with plugin ID name.
    pluginPrefixDef = "#define PREFIX \"%s_\"\n\n" % (pluginName)
    dsContents = pluginPrefixDef + open(pluginDs, 'r').read()
    print dsContents

    pluginPtg.setToDialogScript(dsContents)

    return pluginPtg.parmTemplates()

def _removeVRayDisplacementAttributes(ptg, folderLabel):
    folder = ptg.findFolder(folderLabel)
    if folder:
        try:
            ptg.remove(folder.name())
        except hou.OperationFailed as e:
            print "Error removing folder \"%s\": %s" % (folder.name(), e.instanceMessage())
        except:
            print "Error removing folder \"%s\"" % (folder.name())

    return

    removeTmpl = []
    for pt in ptgParmTemplatesIt(ptg):
        attrName = pt.name()
        if attrName.startswith(("vray_displ_", "GeomDisplacedMesh_", "GeomStaticSmoothedMesh_")):
            removeTmpl.append(attrName)

    for attrName in removeTmpl:
        try:
            print ptg.find(attrName)
            ptg.remove(attrName)
        except hou.OperationFailed as e:
            print "Error removing \"%s\": %s" % (attrName, e.instanceMessage())
        except:
            print "Error removing \"%s\"" % (attrName)

def _addVRayFolder(ptg):
    vrayFolder = hou.FolderParmTemplate("vray_folder", VRAY_FOLDER)

    vfh_attrs.insertInFolderAfterLastTab(ptg, ptg, vrayFolder)

    vrayFolder = ptg.findFolder(VRAY_FOLDER)
    vrayFolder.setName("vray_folder")

def _addVRayDisplacementFolder(ptg):
    vrayFolder = ptg.findFolder(VRAY_FOLDER)
    assert vrayFolder

    dispFolder = hou.FolderParmTemplate("vray_displ_folder_main", "Displacement")
    dispFolder.setName("vray_displ_folder_main")

    vfh_attrs.insertInFolderAfterLastTab(ptg, vrayFolder, dispFolder)

def _addDisplacementControls(ptg, vrayFolder):
    ptg.appendToFolder(vrayFolder, hou.ToggleParmTemplate("vray_displ_use", "Use Displacement", **{
        'tags' : {
            'spare_category': 'vray'
        },
        'default_value' : True,
    }))

    ptg.appendToFolder(vrayFolder, hou.MenuParmTemplate("vray_displ_type", "Type", (['0', '1', '2']), **{
        'tags' : {
            'spare_category': 'vray'
        },
        'menu_labels' : (["Displacement", "Subdivision", "From SHOP"]),
        'default_value' : 0,
        'conditionals' : {
            hou.parmCondType.DisableWhen : "{ vray_displ_use == 0 }",
        },
    }))

    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_displ_folder_GeomDisplacedMesh", "Displacement", **{
        'parm_templates' : (_getPluginParmTemplates("GeomDisplacedMesh")),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray'
        },
        'conditionals' : {
            hou.parmCondType.HideWhen : "{ vray_displ_use == 0 } { vray_displ_type != 0 }"
        },
    }))

    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_displ_folder_GeomStaticSmoothedMesh", "Subdivision", **{
        'parm_templates' : (_getPluginParmTemplates("GeomStaticSmoothedMesh")),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray'
        },
        'conditionals' : {
            hou.parmCondType.HideWhen : "{ vray_displ_use == 0 } { vray_displ_type != 1 }"
        },
    }))

    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_displ_folder_shopnet", "From SHOP", **{
        'parm_templates' : ([
            hou.StringParmTemplate("vray_displ_shoppath", "SHOP", 1, **{
                'string_type' : hou.stringParmType.NodeReference,
                'tags' : {
                    'spare_category': 'vray',
                    'opfilter': '!!SHOP!!',
                    'oprelative': '.',
                }
            })
        ]),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray',
        },
        'conditionals' : {
            hou.parmCondType.HideWhen : "{ vray_displ_use == 0 } { vray_displ_type != 2 }",
        },
    }))

def addVRayDisplamentParamTemplate(ptg):
    if UI is None:
        return

    _removeVRayDisplacementAttributes(ptg, DISPLACEMENT_FOLDER)
    _removeVRayDisplacementAttributes(ptg, VRAY_FOLDER)

    _addVRayFolder(ptg)
    _addVRayDisplacementFolder(ptg)

    assert ptg.findFolder(DISPLACEMENT_FOLDER)

    _addDisplacementControls(ptg, DISPLACEMENT_FOLDER)

def addVRayDisplamentParams(node):
    ptg = node.parmTemplateGroup()

    addVRayDisplamentParamTemplate(ptg)

    node.setParmTemplateGroup(ptg)
