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
import sys

UI = os.environ.get('VRAY_UI_DS_PATH', None)

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

def add_physical_camera_attributes():
    if UI is None:
        return

    physCamDS = os.path.join(UI, "plugins", "CameraPhysical.ds")
    if not os.path.exists(physCamDS):
        sys.stderr.write("CameraPhysical.ds is not found!\n")
        return

    for node in hou.selectedNodes():
        if node.type().name() not in {"cam"}:
            continue

        folderName = "VfhCameraPhysical"
        folderLabel = "V-Ray Physical Camera"

        currentSettings = _getCurrectSettings(node)

        _removeCameraPhysicalAttributes(node, folderLabel)

        group = node.parmTemplateGroup()
        folder = hou.FolderParmTemplate(folderName, folderLabel)

        physCamGroup = hou.ParmTemplateGroup()
        physCamGroup.setToDialogScript(open(physCamDS, 'r').read())
        for parmTmpl in physCamGroup.parmTemplates():
            folder.addParmTemplate(parmTmpl)

        group.append(folder)
        node.setParmTemplateGroup(group)

        for parm in node.parms():
            try:
                attrName = parm.name()

                parmValue = currentSettings.get(attrName, None)
                if parmValue is not None:
                    parm.set(parmValue)
                elif attrName in {"CameraPhysical_f_number"}:
                    parm.setExpression("ch(\"./fstop\")")
                elif attrName in {"CameraPhysical_focus_distance"}:
                    parm.setExpression("ch(\"./focus\")")
                elif attrName in {"CameraPhysical_horizontal_offset"}:
                    parm.setExpression("-ch(\"./winx\")")
                elif attrName in {"CameraPhysical_vertical_offset"}:
                    parm.setExpression("-ch(\"./winy\")")
            except:
                pass
