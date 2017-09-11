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

tubenode = node.createNode("tube")
node_parm = tubenode.parm("orient")
node_parm.set("z")
node_parm = tubenode.parm("rad1")
node_parm.set(0)
node_parm = tubenode.parm("tz")
node_parm.setExpression("-0.5 * ch(\"height\")")
node_parm = tubenode.parm("radscale")
node_parm.setExpression("ch(\"height\") * tan(0.5*deg(ch(\"../coneAngle\")))")

circlenode = node.createNode("circle")
node_parm = circlenode.parm("tz");
node_parm.setExpression("-ch(\"../%s/height\")" % tubenode.name())
node_parm = circlenode.parm("scale");
node_parm.setExpression("-ch(\"tz\") * tan(0.5*deg(ch(\"../coneAngle\") + ch(\"../penumbraAngle\")))")

mergenode = node.createNode("merge")
mergenode.setInput(0, tubenode, 0)
mergenode.setInput(1, circlenode, 0)

mergenode.setRenderFlag(True)
mergenode.setDisplayFlag(True)

node.layoutChildren()
node.setName("vraylightspot")