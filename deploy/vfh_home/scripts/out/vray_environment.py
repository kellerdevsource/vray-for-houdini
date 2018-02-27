# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Author: Jefferson D. Lim (galagast)
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import hou

# The node is passed in kwargs['node']
node = kwargs['node']

envNode = node.createNode("VRayNodeSettingsEnvironment")
envNode.setName("envSettings")

imageFileNode = envNode.createInputNode(0, "VRayNodeMetaImageFile")
imageFileNode.setName("envImageFile")
imageFileNode.parm("meta_image_uv_generator").set("1")

envNode.setInput(1, imageFileNode, 0)
envNode.setInput(2, imageFileNode, 0)
envNode.setInput(3, imageFileNode, 0)

node.layoutChildren()