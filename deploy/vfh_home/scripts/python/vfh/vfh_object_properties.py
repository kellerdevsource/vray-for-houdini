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
from vfh import vfh_ptg_utils

vfh_reload.reload(vfh_ptg_utils)

VRAY_FOLDER = "V-Ray"
DISPLACEMENT_FOLDER = (VRAY_FOLDER, "Displacement")
HAIR_FOLDER = (VRAY_FOLDER, "Hair")
OBJECT_PROPERTIES_FOLDER = (VRAY_FOLDER, "Object Properties")
PHYSICAL_CAMERA_FOLDER = (VRAY_FOLDER, "Physical Camera")

def _addVRayFolder(ptg):
    if ptg.findFolder(VRAY_FOLDER):
        return

    vrayFolder = hou.FolderParmTemplate("vray_folder", VRAY_FOLDER)
    vfh_ptg_utils.insertInFolderAfterLastTab(ptg, ptg, vrayFolder)

def _addVRayDisplacementFolder(ptg):
    vrayFolder = ptg.findFolder(VRAY_FOLDER)
    assert vrayFolder

    dispFolder = hou.FolderParmTemplate("vray_displ_folder_main", "Displacement")

    vfh_ptg_utils.insertInFolderAfterLastTab(ptg, vrayFolder, dispFolder)

def _addVRayHairFolder(ptg):
    vrayFolder = ptg.findFolder(VRAY_FOLDER)
    assert vrayFolder

    hairFolder = hou.FolderParmTemplate("vray_hair_folder_main", "Hair")

    vfh_ptg_utils.insertInFolderAfterLastTab(ptg, vrayFolder, hairFolder)

def _addVRayObjectPropertiesFolder(ptg):
    vrayFolder = ptg.findFolder(VRAY_FOLDER)
    assert vrayFolder

    obPropFolder = hou.FolderParmTemplate("vray_object_properties_folder_main", "Object Properties")

    vfh_ptg_utils.insertInFolderAfterLastTab(ptg, vrayFolder, obPropFolder)

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
        'parm_templates' : (vfh_ptg_utils.getParmTemplatesFromDS("GeomDisplacedMesh", prefix="GeomDisplacedMesh")),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray'
        },
        'conditionals' : {
            hou.parmCondType.HideWhen : "{ vray_displ_use == 0 } { vray_displ_type != 0 }"
        },
    }))

    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_displ_folder_GeomStaticSmoothedMesh", "Subdivision", **{
        'parm_templates' : (vfh_ptg_utils.getParmTemplatesFromDS("GeomStaticSmoothedMesh", prefix ="GeomStaticSmoothedMesh")),
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
    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_hair_folder_main", "Hair", **{
        'parm_templates' : (vfh_ptg_utils.getParmTemplatesFromDS("GeomMayaHair", prefix="GeomMayaHair")),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray'
        }
    }))

def _addVRayObjectPropertiesParmTemplates(ptg, vrayFolder):
    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_object_properties_folder_main", "Main", **{
        'parm_templates' : (vfh_ptg_utils.getParmTemplatesFromDS("vfh_object_properties")),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray'
        }
    }))

    ptg.appendToFolder(vrayFolder, hou.FolderParmTemplate("vray_object_properties_folder_wrapper", "Wrapper", **{
        'parm_templates' : (vfh_ptg_utils.getParmTemplatesFromDS("MtlWrapper", prefix="MtlWrapper")),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray'
        }
    }))

def addVRayDisplamentParams(node):
    ptg = node.parmTemplateGroup()

    vfh_ptg_utils.removeFolderIfEmpty(ptg, DISPLACEMENT_FOLDER, removeNotEmpty=True)
    vfh_ptg_utils.removeFolderIfEmpty(ptg, VRAY_FOLDER)

    _addVRayFolder(ptg)
    _addVRayDisplacementFolder(ptg)
    _addDisplacementControls(ptg, DISPLACEMENT_FOLDER)

    node.setParmTemplateGroup(ptg)

def addVRayHairParams(node):
    ptg = node.parmTemplateGroup()

    vfh_ptg_utils.removeFolderIfEmpty(ptg, HAIR_FOLDER, removeNotEmpty=True)
    vfh_ptg_utils.removeFolderIfEmpty(ptg, VRAY_FOLDER)

    _addVRayFolder(ptg)
    _addVRayHairFolder(ptg)
    _addVRayHairParmTemplates(ptg, HAIR_FOLDER)

    node.setParmTemplateGroup(ptg)

def addVRayObjectProperties(node):
    ptg = node.parmTemplateGroup()

    vfh_ptg_utils.removeFolderIfEmpty(ptg, OBJECT_PROPERTIES_FOLDER, removeNotEmpty=True)
    vfh_ptg_utils.removeFolderIfEmpty(ptg, VRAY_FOLDER)

    _addVRayFolder(ptg)
    _addVRayObjectPropertiesFolder(ptg)
    _addVRayObjectPropertiesParmTemplates(ptg, OBJECT_PROPERTIES_FOLDER)

    node.setParmTemplateGroup(ptg)

def addVRayCameraPhysical(node):
    def _getCurrectSettings(node):
        """
        Collects current CameraPhysical attribute values.
        """
        parms = dict()

        for pt in node.parmTemplateGroup().entries():
            attrName = pt.name()
            if attrName.startswith("CameraPhysical_"):
                try:
                    p = node.parm(attrName)
                    parms[attrName] = p.eval()
                except:
                    pass

        return parms

    def _removeCameraPhysicalAttributes(node, folderLabel):
        """
        Removes existing CameraPhysical attributes.
        """
        ptg = node.parmTemplateGroup()

        folder = ptg.findFolder(folderLabel)
        if folder:
            ptg.remove(folder.name())

        # Removing the folder doesn't remove invisible parameters
        for pt in ptg.entries():
            attrName = pt.name()
            if attrName.startswith("CameraPhysical"):
                ptg.remove(attrName)

        node.setParmTemplateGroup(ptg)

    def _addVRayCameraPhysical(ptg):
        vrayFolder = ptg.findFolder(VRAY_FOLDER)
        assert vrayFolder

        vfh_ptg_utils.insertInFolderAfterLastTab(
            ptg,
            vrayFolder,
            hou.FolderParmTemplate(
                "VfhCameraPhysicalTab",
                "Physical Camera",
                **{
                    'parm_templates' : (
                        vfh_ptg_utils.getParmTemplatesFromDS("CameraPhysical", prefix="CameraPhysical")
                    ),
                    'tags' : {
                        'spare_category': 'vray'
                    }
                }
            )
        )

    currentSettings = _getCurrectSettings(node)

    # Remove old attributes
    _removeCameraPhysicalAttributes(node, "V-Ray Physical Camera")

    ptg = node.parmTemplateGroup()

    vfh_ptg_utils.removeFolderIfEmpty(ptg, PHYSICAL_CAMERA_FOLDER, removeNotEmpty=True)
    vfh_ptg_utils.removeFolderIfEmpty(ptg, VRAY_FOLDER)

    _addVRayFolder(ptg)
    _addVRayCameraPhysical(ptg)

    node.setParmTemplateGroup(ptg)

    for parm in node.parms():
        attrName = parm.name()

        if attrName in {"CameraPhysical_f_number"}:
            parm.setExpression("ch(\"./fstop\")")
        if attrName in {"CameraPhysical_focus_distance"}:
            parm.setExpression("ch(\"./focus\")")
        if attrName in {"CameraPhysical_horizontal_offset"}:
            parm.setExpression("-ch(\"./winx\")")
        if attrName in {"CameraPhysical_vertical_offset"}:
            parm.setExpression("-ch(\"./winy\")")
        if attrName in {"CameraPhysical_focal_length"}:
            parm.setExpression("ch(\"./focal\")")
        if attrName in {"CameraPhysical_film_width"}:
            parm.setExpression("ch(\"./aperture\")")

        parmValue = currentSettings.get(attrName, None)
        if parmValue is not None:
            parm.set(parmValue)
