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


def _createVRayRop():
    vrayNode = hou.node("/out").createNode("vray_renderer")

    return vrayNode


def _getVRayRop():
    vrayROP = hou.node(hou.getenv("curVRayROP"))

    vray_node_types = hou.nodeType(hou.ropNodeTypeCategory(), "vray_renderer")
    vray_nodes = vray_node_types.instances()
    sel_vray_nodes = [ i for i in vray_nodes if i.isSelected() ]

    if vray_nodes:
        vray_rops = sel_vray_nodes if sel_vray_nodes else vray_nodes

        # Use first available
        vrayROP = vray_rops[0]
        hou.putenv("curVRayROP", vrayROP.path())
    else:
        if not vrayROP:
            vrayROP = _createVRayRop()
            hou.putenv("curVRayROP", vrayROP.path())
        else:
            vrayROP = hou.node(hou.getenv("curVRayROP"))

    return vrayROP


def render():
    vrayNode = _getVRayRop()
    vrayNode.parm('execute').pressButton()


def render_rt():
    vrayNode = _getVRayRop()
    vrayNode.parm('render_rt').pressButton()
