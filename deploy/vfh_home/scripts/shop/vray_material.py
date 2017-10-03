#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

# Python script invoked by the SHOP manager in Houdini when a "vray_material" context SHOP is being created.
# For more info refer to:
#  https://www.sidefx.com/docs/houdini13.0/hom/assetscripts#node_initialization_scripts
#
# This script is used to add a default brdf material and material output
#

import hou

# The node is passed in kwargs['node']
shop = kwargs['node']
output = shop.createNode("vray_material_output")
brdf = output.createInputNode(0, "VRayNodeBRDFVRayMtl")
shop.layoutChildren()
