#
# Copyright (c) 2015, Chaos Software Ltd
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

from vfh import vfh_json
from vfh import vfh_attrs

def add_physical_camera_attributes():
    physCamPlugID = 'CameraPhysical'
    CameraPhysicalDesc = vfh_json.getPluginDesc(physCamPlugID)

    if not CameraPhysicalDesc:
        sys.stderr.write("CameraPhysical plugin description is not found!\n")

    else:
        physCamTabName = "V-Ray Physical Camera"

        for node in hou.selectedNodes():
            if node.type().name() == "cam":
                sys.stdout.write("Adding \"Physical Camera\" attributes to \"%s\"...\n" % node.name())

                ptg = node.parmTemplateGroup()

                if not ptg.findFolder(physCamTabName):
                    ptg.append(hou.FolderParmTemplate("vray.%s" % physCamPlugID, physCamTabName))

                vfh_attrs.addPluginParms(ptg, CameraPhysicalDesc, parmPrefix = physCamPlugID, parmFolder = physCamTabName)
                node.setParmTemplateGroup(ptg)
