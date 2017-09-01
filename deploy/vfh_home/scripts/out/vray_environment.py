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

imageTexNode = envNode.createInputNode(0, "VRayNodeTexBitmap")	# bg_tex
envNode.setInput(1, imageTexNode, 0)							# gi_tex
envNode.setInput(2, imageTexNode, 0)							# reflect_tex
envNode.setInput(3, imageTexNode, 0)							# refract_tex

uvwGenEnvNode = imageTexNode.createInputNode(5, "VRayNodeUVWGenEnvironment")
imageFileNode = imageTexNode.createInputNode(6, "VRayNodeBitmapBuffer")

envNode.setName("environment_settings")
uvwGenEnvNode.setName("environment_mapping")
imageTexNode.setName("imagetexture")
imageFileNode.setName("imagefile")

node.layoutChildren()