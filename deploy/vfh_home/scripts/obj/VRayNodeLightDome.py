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
node_parm.set("vfhlightdome.bgeo")

cnode = cnode.createOutputNode("xform")
node_parm = cnode.parm("scale");
node_parm.setExpression("ch(\"../dome_targetRadius\")")

cnode = cnode.createOutputNode("xform")
node_parm = cnode.parm("group")
node_parm.set("gcompass")
node_parm = cnode.parm("rOrd")
node_parm.setExpression("ch(\"../rOrd\")")
node_parm = cnode.parm("rx");
node_parm.setExpression("ch(\"../rx\")")
node_parm = cnode.parm("ry");
node_parm.setExpression("ch(\"../ry\")")
node_parm = cnode.parm("rz");
node_parm.setExpression("ch(\"../rz\")")

cnode = cnode.createOutputNode("xform")
node_parm = cnode.parm("xOrd")
node_parm.setExpression("ch(\"../xOrd\")")
node_parm = cnode.parm("rOrd")
node_parm.setExpression("ch(\"../rOrd\")")
node_parm = cnode.parm("rx");
node_parm.setExpression("ch(\"../rx\")")
node_parm = cnode.parm("ry");
node_parm.setExpression("ch(\"../ry\")")
node_parm = cnode.parm("rz");
node_parm.setExpression("ch(\"../rz\")")
node_parm = cnode.parm("tx");
node_parm.setExpression("ch(\"../tx\")")
node_parm = cnode.parm("ty");
node_parm.setExpression("ch(\"../ty\")")
node_parm = cnode.parm("tz");
node_parm.setExpression("ch(\"../tz\")")
node_parm = cnode.parm("px");
node_parm.setExpression("ch(\"../px\")")
node_parm = cnode.parm("py");
node_parm.setExpression("ch(\"../py\")")
node_parm = cnode.parm("pz");
node_parm.setExpression("ch(\"../pz\")")
node_parm = cnode.parm("invertxform")
node_parm.set(True)

cnode = cnode.createOutputNode("delete")
node_parm = cnode.parm("negate");
node_parm.set("keep")
node_parm = cnode.parm("group");
node_parm.setExpression("ifs(ch(\"../dome_spherical\"), \"gsphere\",\"ghemisphere\")")

cnode.setRenderFlag(True)
cnode.setDisplayFlag(True)

node.layoutChildren()
node.setName("vraylightdome")