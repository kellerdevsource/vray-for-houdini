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

cnode = node.createNode("file")
node_parm = cnode.parm("file")
node_parm.set("pointlight.bgeo")

cnode.setRenderFlag(True)
cnode.setDisplayFlag(True)

node.layoutChildren()
node.setName("vraylightomni")