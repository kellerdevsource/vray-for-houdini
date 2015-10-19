#
# Copyright (c) 2015, Chaos Software Ltd
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

def render():
    vrayNode = hou.node("/out/vray_renderer1")

    if not vrayNode:
        vrayNode = hou.node("/out").createNode("vray_renderer")

    if vrayNode:
        vrayNode.parm('execute').pressButton()
