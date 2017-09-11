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
node_parm.setExpression("if(ch(\"./enabled\"), ch(\"./intensity_multiplier\"), 0)")

circlenode = node.createNode("circle")

xformnode = circlenode.createOutputNode("xform")
node_parm = xformnode.parm("scale")
node_parm.set(3)

linenode = node.createNode("line")
node_parm = linenode.parm("diry")
node_parm.set(0)
node_parm = linenode.parm("dirz")
node_parm.set(-1)

cnode = linenode.createOutputNode("xform")
node_parm = cnode.parm("tx");
node_parm.setExpression("ch(\"../%s/scale\")" % xformnode.name())
node_parm = cnode.parm("scale");
node_parm.setExpression("ch(\"../%s/scale\") * 0.25" % xformnode.name())

copynode = cnode.createOutputNode("copy")
node_parm = copynode.parm("ncy");
node_parm.set(10)
node_parm = copynode.parm("rz");
node_parm.setExpression("360 / ch(\"ncy\")")

cnode = linenode.createOutputNode("xform")
node_parm = cnode.parm("scale");
node_parm.setExpression("ch(\"../%s/scale\") * 0.66" % xformnode.name())

mergenode = node.createNode("merge")
mergenode.setInput(0, circlenode, 0)
mergenode.setInput(1, xformnode, 0)
mergenode.setInput(2, copynode, 0)
mergenode.setInput(3, cnode, 0)

mergenode.setRenderFlag(True)
mergenode.setDisplayFlag(True)

node.layoutChildren()
node.setName("vraysunlight")