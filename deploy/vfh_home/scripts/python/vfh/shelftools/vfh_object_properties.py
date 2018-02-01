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

from vfh import vfh_reload
from vfh import vfh_attrs
from vfh import vfh_json

vfh_reload.reload(vfh_attrs)
vfh_reload.reload(vfh_json)

UI_FILEPATHS = {}

if not UI_FILEPATHS:
    dsDirPath = os.environ.get('VRAY_UI_DS_PATH', None)
    assert dsDirPath

    for root, dirs, files in os.walk(dsDirPath):
        for f in files:
            if f.endswith(".ds"):
                dsFileName = os.path.splitext(f)[0]
                UI_FILEPATHS[dsFileName] = os.path.join(root, f)

VRAY_FOLDER = "V-Ray"
DISPLACEMENT_FOLDER = (VRAY_FOLDER, "Displacement")
HAIR_FOLDER = (VRAY_FOLDER, "Hair")
OBJECT_PROPERTIES_FOLDER = (VRAY_FOLDER, "Object Properties")

def ptgParmTemplatesIt(ptg):
    for pt in ptg.parmTemplates():
        yield pt

        if pt.type() == hou.parmTemplateType.Folder and pt.isActualFolder():
            for _pt in ptgParmTemplatesIt(pt):
                yield _pt

def _getDsFilePathFromName(fileName):
    assert fileName in UI_FILEPATHS
    return UI_FILEPATHS[fileName]

def _getParmTemplatesFromDS(fileName, prefix=None):
    pluginDs = _getDsFilePathFromName(fileName)

    pluginPtg = hou.ParmTemplateGroup()

    # This will make attribute names prefixed with plugin ID name.
    pluginPrefixDef = "" if not prefix else "#define PREFIX \"%s_\"\n\n" % (prefix)

    dsContents = pluginPrefixDef + open(pluginDs, 'r').read()

    pluginPtg.setToDialogScript(dsContents)

    return pluginPtg.parmTemplates()

def _removeFolderIfEmpty(ptg, folderName, removeNotEmpty=False):
    folderParm = ptg.findFolder(folderName)
    if not folderParm:
        return
    if not folderParm.type() == hou.parmTemplateType.Folder:
        return
    if not folderParm.isActualFolder():
        return

    if not removeNotEmpty and len(folderParm.parmTemplates()):
        return

    ptg.remove(folderParm)

def _addVRayFolder(ptg):
    if ptg.findFolder(VRAY_FOLDER):
        return

    vrayFolder = hou.FolderParmTemplate("vray_folder", VRAY_FOLDER)
    vfh_attrs.insertInFolderAfterLastTab(ptg, ptg, vrayFolder)

def _addVRayDisplacementFolder(ptg):
    vrayFolder = ptg.findFolder(VRAY_FOLDER)
    assert vrayFolder

    dispFolder = hou.FolderParmTemplate("vray_displ_folder_main", "Displacement")
    dispFolder.setName("vray_displ_folder_main")

    vfh_attrs.insertInFolderAfterLastTab(ptg, vrayFolder, dispFolder)

def _addVRayHairFolder(ptg):
    vrayFolder = ptg.findFolder(VRAY_FOLDER)
    assert vrayFolder

    hairFolder = hou.FolderParmTemplate("vray_hair_folder_main", "Hair")
    hairFolder.setName("vray_hair_folder_main")

    vfh_attrs.insertInFolderAfterLastTab(ptg, vrayFolder, hairFolder)

def _addVRayObjectPropertiesFolder(ptg):
    vrayFolder = ptg.findFolder(VRAY_FOLDER)
    assert vrayFolder

    obPropFolder = hou.FolderParmTemplate("vray_object_properties_folder_main", "Object Properties")

    vfh_attrs.insertInFolderAfterLastTab(ptg, vrayFolder, obPropFolder)

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
        'menu_labels' : (["Displacement", "Subdivision", "From Material"]),
        'default_value' : 0,
        'conditionals' : {
            hou.parmCondType.DisableWhen : "{ vray_displ_use == 0 }",
        },
    }))

    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_displ_folder_GeomDisplacedMesh", "Displacement", **{
        'parm_templates' : (_getParmTemplatesFromDS("GeomDisplacedMesh", prefix="GeomDisplacedMesh")),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray'
        },
        'conditionals' : {
            hou.parmCondType.HideWhen : "{ vray_displ_use == 0 } { vray_displ_type != 0 }"
        },
    }))

    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_displ_folder_GeomStaticSmoothedMesh", "Subdivision", **{
        'parm_templates' : (_getParmTemplatesFromDS("GeomStaticSmoothedMesh", prefix ="GeomStaticSmoothedMesh")),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray'
        },
        'conditionals' : {
            hou.parmCondType.HideWhen : "{ vray_displ_use == 0 } { vray_displ_type != 1 }"
        },
    }))

    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_displ_folder_shopnet", "From Material", **{
        'parm_templates' : ([
            hou.StringParmTemplate("vray_displ_shoppath", "Material", 1, **{
                'string_type' : hou.stringParmType.NodeReference,
                'tags' : {
                    'spare_category': 'vray',
                    'opfilter': '!!VOP!!',
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

def _addVRayHairParmTemplates(ptg, vrayFolder):
    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_displ_folder_GeomMayaHair", "Hair", **{
        'parm_templates' : (_getParmTemplatesFromDS("GeomMayaHair", prefix="GeomMayaHair")),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray'
        }
    }))

def _addVRayObjectPropertiesParmTemplates(ptg, vrayFolder):
    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_object_properties_folder_main", "Main", **{
        'parm_templates' : (_getParmTemplatesFromDS("vfh_object_properties")),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray'
        }
    }))

    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_object_properties_folder_wrapper", "Wrapper", **{
        'parm_templates' : (_getParmTemplatesFromDS("MtlWrapper", prefix="MtlWrapper")),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray'
        }
    }))

def addVRayDisplamentParams(node):
    ptg = node.parmTemplateGroup()

    _removeFolderIfEmpty(ptg, DISPLACEMENT_FOLDER, removeNotEmpty=True)
    _removeFolderIfEmpty(ptg, VRAY_FOLDER)

    _addVRayFolder(ptg)
    _addVRayDisplacementFolder(ptg)
    _addDisplacementControls(ptg, DISPLACEMENT_FOLDER)

    node.setParmTemplateGroup(ptg)

def addVRayHairParams(node):
    ptg = node.parmTemplateGroup()

    _removeFolderIfEmpty(ptg, HAIR_FOLDER, removeNotEmpty=True)
    _removeFolderIfEmpty(ptg, VRAY_FOLDER)

    _addVRayFolder(ptg)
    _addVRayHairFolder(ptg)
    _addVRayHairParmTemplates(ptg, HAIR_FOLDER)

    node.setParmTemplateGroup(ptg)

def addVRayObjectProperties(node):
    ptg = node.parmTemplateGroup()

    _removeFolderIfEmpty(ptg, OBJECT_PROPERTIES_FOLDER, removeNotEmpty=True)
    _removeFolderIfEmpty(ptg, VRAY_FOLDER)

    _addVRayFolder(ptg)
    _addVRayObjectPropertiesFolder(ptg)
    _addVRayObjectPropertiesParmTemplates(ptg, OBJECT_PROPERTIES_FOLDER)

    node.setParmTemplateGroup(ptg)
