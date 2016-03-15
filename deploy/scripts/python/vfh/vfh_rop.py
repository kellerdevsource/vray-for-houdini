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

def _validVrayRop(node):
    if node != None:
        result = node.type().name() == 'vray_renderer'
    else:
        result = False
    
    return result


def _createVrayRop():
    vrayRopNode = hou.node('/out').createNode('vray_renderer')
    
    return vrayRopNode


def _getVrayRop():
    vrayRopPath = getattr(hou.session, 'curVrayRopPath', "")
    
    # safety incase vrayRopPath gets overwritten with a different value type
    if type(vrayRopPath) is not str:
        vrayRopPath = ""
    
    vrayRopType = hou.nodeType('Driver/vray_renderer')
    vrayRopNodes = vrayRopType.instances()
    vrayRopSelection = [i for i in vrayRopNodes if i.isSelected()]
    
    if len(vrayRopSelection) > 0:
        vrayRop = vrayRopSelection[0]
    else:
        vrayRop = hou.node(vrayRopPath)
        if not _validVrayRop(vrayRop):
            if len(vrayRopNodes) > 0:
                vrayRop = vrayRopNodes[0]
            else:
                vrayRop = _createVrayRop()
                
    hou.session.curVrayRopPath = vrayRop.path()
    
    return vrayRop


def render():
    vrayRopNode = _getVrayRop()
    vrayRopNode.parm('execute').pressButton()


def render_rt():
    vrayRopNode = _getVrayRop()
    vrayRopNode.parm('render_rt').pressButton()