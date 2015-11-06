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


def _getCreateRop():
    vrayNode = hou.node("/out/vray_renderer1")

    if not vrayNode:
        vrayNode = hou.node("/out").createNode("vray_renderer")

    return vrayNode


def render():
    vrayNode = _getCreateRop()
    vrayNode.parm('execute').pressButton()


def render_rt():
    vrayNode = _getCreateRop()
    vrayNode.parm('render_rt').pressButton()
