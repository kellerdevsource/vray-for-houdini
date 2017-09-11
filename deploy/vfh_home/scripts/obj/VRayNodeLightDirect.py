# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import hou

# The node is passed in kwargs['node']
node = kwargs['node']

node_parm = node.parm("dimmer")
node_parm.setExpression("if(ch(\"./enabled\"), ch(\"./intensity\"), 0)")

linenode = node.createNode("line")
node_parm = linenode.parm("diry")
node_parm.set(0)
node_parm = linenode.parm("dirz")
node_parm.set(-1)

gridnode = node.createNode("grid")
node_parm = gridnode.parm("orient");
node_parm.set("xy")
node_parm = gridnode.parm("sizex");
node_parm.set(2)
node_parm = gridnode.parm("sizey");
node_parm.setExpression("ch(\"sizex\")")
node_parm = gridnode.parm("rows");
node_parm.set(3)
node_parm = gridnode.parm("cols");
node_parm.setExpression("ch(\"rows\")")

copynode = node.createNode("copy")
node_parm = copynode.parm("nml")
node_parm.set(False)
copynode.setInput(0, linenode, 0)
copynode.setInput(1, gridnode, 0)

mergenode = node.createNode("merge")
mergenode.setInput(0, gridnode, 0)
mergenode.setInput(1, copynode, 0)

mergenode.setRenderFlag(True)
mergenode.setDisplayFlag(True)

node.layoutChildren()

node.setName("vraylightdirect")