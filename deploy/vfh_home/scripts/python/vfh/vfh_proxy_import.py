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

import time
import threading

from vfh import vfh_json
from vfh import vfh_attrs

def add_proxy_import_to_selected_sop():
    nodeToSelect = None
    enterInNode = False

    for node in hou.selectedNodes():
        if node.type().name() == 'geo':
            created = node.createNode('VRayNodeVRayProxy')
            if not nodeToSelect:
                nodeToSelect = created
                enterInNode = True
            else:
                # nodeToSelect is assigned
                enterInNode = False

    if enterInNode:
        nodeToSelect.setSelected(True)

    if not nodeToSelect:
        sys.stderr.write('No selected SOP nodes to import proxy into!')
        sys.stderr.flush()
