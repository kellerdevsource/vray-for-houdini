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
node_parm.setExpression("ch(\"./enabled\")")

boxnode = node.createNode("box")

circlenode = node.createNode("circle")
node_parm = circlenode.parm("orient")
node_parm.set("xy")
node_parm = circlenode.parm("tz")
node_parm.setExpression("-0.5 * ch(\"../%s/scale\")" % boxnode.name())
node_parm = circlenode.parm("scale")
node_parm.setExpression("0.4 * ch(\"../%s/scale\")" % boxnode.name())

mergenode = node.createNode("merge")
mergenode.setInput(0, boxnode, 0)
mergenode.setInput(1, circlenode, 0)

mergenode.setRenderFlag(True)
mergenode.setDisplayFlag(True)

node.layoutChildren()
node.setName("vraylighties")