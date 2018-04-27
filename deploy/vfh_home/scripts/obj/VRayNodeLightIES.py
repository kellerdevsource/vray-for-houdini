# Copyright (c) 2015-2018, Chaos Software Ltd
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
circlenode.parm("orient").set(2)
circlenode.parm("ty").setExpression("-0.5 * ch(\"../%s/scale\")" % boxnode.name())
circlenode.parm("scale").setExpression("0.4 * ch(\"../%s/scale\")" % boxnode.name())

mergenode = node.createNode("merge")
mergenode.setInput(0, boxnode, 0)
mergenode.setInput(1, circlenode, 0)

mergenode.setRenderFlag(True)
mergenode.setDisplayFlag(True)

node.layoutChildren()
