# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import hou

node = kwargs['node']

node.parm("dimmer").setExpression('if(ch("./enabled"), ch("./intensity"), 0)')

tubenode = node.createNode("tube")
tubenode.parm("orient").set("z")
tubenode.parm("rad1").set(0)
tubenode.parm("tz").setExpression("-ch(\"height\") / 2.0")
tubenode.parm("radscale").setExpression("ch(\"height\") * tan(ch(\"../coneAngle\") / 2.0)")

circlenode = node.createNode("circle")
circlenode.parm("tz").setExpression('-ch("../%s/height")' % tubenode.name())
circlenode.parm("scale").setExpression('-ch("tz") * tan((ch("../coneAngle") + ch("../penumbraAngle")) / 2.0)')

mergenode = node.createNode("merge")
mergenode.setInput(0, tubenode, 0)
mergenode.setInput(1, circlenode, 0)
mergenode.setRenderFlag(True)
mergenode.setDisplayFlag(True)

node.layoutChildren()
