import hou

def render():
    vrayNode = hou.node("/out/vray_renderer1")

    if not vrayNode:
        vrayNode = hou.node("/out").createNode("vray_renderer")

    if vrayNode:
        vrayNode.parm('execute').pressButton()
