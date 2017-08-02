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

def add_physical_camera_attributes():
    if UI is None:
        return

    physCamDS = os.path.join(UI, "plugins", "view", "CameraPhysical.ds")
    if not os.path.exists(physCamDS):
        sys.stderr.write("CameraPhysical.ds is not found!\n")
        return

    for node in hou.selectedNodes():
        if node.type().name() not in {"cam"}:
            continue

        folderName = "V-Ray Physical Camera"

        group = node.parmTemplateGroup()
        if group.findFolder(folderName):
            continue

        sys.stdout.write("Adding \"Physical Camera\" attributes to \"%s\"...\n" % node.name())

        folder = hou.FolderParmTemplate("VRayPhysicalCamera", folderName)
        physCamGroup = hou.ParmTemplateGroup()
        physCamGroup.setToDialogScript(open(physCamDS, 'r').read())
        for parmTmpl in physCamGroup.parmTemplates():
            folder.addParmTemplate(parmTmpl)

        group.append(folder)
        node.setParmTemplateGroup(group)
