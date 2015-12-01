# Copyright (c) 2015, Chaos Software Ltd
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

cnode = node.createNode("file")
node_parm = cnode.parm("file")
node_parm.set("pointlight.bgeo")

cnode.setRenderFlag(True)
cnode.setDisplayFlag(True)

node.layoutChildren()