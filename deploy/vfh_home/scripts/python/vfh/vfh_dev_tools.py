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
from vfh import vfh_ptg_utils

from pprint import pprint

vfh_reload.reload(vfh_attrs)
vfh_reload.reload(vfh_ptg_utils)

def _syncUiIPR(nodeDef):
    ptg = nodeDef.parmTemplateGroup()

    VRAY_IPR_FOLDER = "V-Ray IPR"

    vfh_ptg_utils.removeFolderIfEmpty(ptg, VRAY_IPR_FOLDER, removeNotEmpty=True, removeNotExact=True)

    ptg.append(hou.FolderParmTemplate("vray_ipr_main", VRAY_IPR_FOLDER, **{
        'parm_templates' : (vfh_ptg_utils.getParmTemplatesFromDS("vfh_rop_ipr")),
        'folder_type' : hou.folderType.Simple,
        'tags' : {
            'spare_category': 'vray'
        }
    }))

    nodeDef.setParmTemplateGroup(ptg)

def syncUiIPR():
    iprNode = hou.node("/out/vray_ipr")
    iprNodeDef = iprNode.type().definition()

    _syncUiIPR(iprNodeDef)
