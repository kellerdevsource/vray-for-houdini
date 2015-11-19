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

fnode = node.createNode("file")
node_parm = fnode.parm("file")
node_parm.set("vfhlightdome.bgeo")
node_parm = fnode.parm("loadtype")
node_parm.set('delayed')
node_parm = fnode.parm("viewportlod")
node_parm.set('full')

tnode = fnode.createOutputNode("xform")
node_parm = tnode.parm("scale");
node_parm.setExpression("ch(\"../dome_targetRadius\")")

rnode = tnode.createOutputNode("null")
rnode.setName("OUT")
rnode.setRenderFlag(True)
rnode.setDisplayFlag(True)

node.layoutChildren()