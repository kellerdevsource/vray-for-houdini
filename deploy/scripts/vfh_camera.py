import hou
import os
import sys

from . import vfh_json
from . import vfh_attrs

def add_physical_camera_attributes():
    CameraPhysicalDesc = vfh_json.getPluginDesc('CameraPhysical')

    if not CameraPhysicalDesc:
        sys.stderr.write("CameraPhysical plugin description is not found!\n")

    else:
        cameraTabName = "V-Ray Physical Camera"

        for node in hou.selectedNodes():
            if node.type().name() == "cam":
                sys.stdout.write("Adding \"Physical Camera\" attributes to \"%s\"...\n" % node.name())

                ptg = node.parmTemplateGroup()

                vrayFolder = ptg.findFolder(cameraTabName)
                if not vrayFolder:
                    vrayFolder = hou.FolderParmTemplate("vray.CameraPhysical", cameraTabName)

                    vfh_attrs.add_attributes(vrayFolder, CameraPhysicalDesc, prefix='CameraPhysical')

                    ptg.append(vrayFolder)

                    node.setParmTemplateGroup(ptg)
