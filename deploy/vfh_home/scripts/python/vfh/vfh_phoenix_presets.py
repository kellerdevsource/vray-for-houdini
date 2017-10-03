#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import hou
import sys


def _setup_phx_material_node(phxCacheNode, materialNode):
	shaderNode = None
	if not materialNode:
		# create new material node and clear non output nodes
		materialNode = hou.node('/shop').createNode('vray_material')

	# find output material node
	outputNode = None
	for n in materialNode.children():
		if n.type().name() == 'vray_material_output':
			outputNode = n
		else:
			n.destroy()

	if not outputNode:
		outputNode = materialNode.createNode('vray_material_output')

	# find the phxShaderNode
	for n in materialNode.children():
		if n.type().name() == 'VRayNodePhxShaderSim':
			shaderNode = n
			break

	if not shaderNode:
		shaderNode = materialNode.createNode('VRayNodePhxShaderSim')

	outputNode.setNamedInput('Simulation', shaderNode, 0)
	return shaderNode


def _setup_phx_cache_node(sopNode):
	cacheNode = None
	for node in sopNode.children():
		if node.type().name() == 'VRayNodePhxShaderCache':
			cacheNode = node
			break

	if not cacheNode:
		cacheNode = sopNode.createNode('VRayNodePhxShaderCache')

	cacheNode.setDisplayFlag(True)
	cacheNode.setRenderFlag(True)

	return cacheNode


def _set_phx_presets(preset, shaderNode, cacheNode):
	vals = {}
	if preset == 'FumeFX':
		# fire
		vals['emult'] = 0.4
		vals['etmult'] = 1.0
		vals['blackbody'] = 0.0
		vals['fire_opacity_mode'] = 2 # Use Own Opacity

		# smoke
		vals['darg'] = 6 # Constant Color
		vals['noscatter'] = 3 # Approximate + Shadows
		vals['transfown'] = 0.8
		vals['transfext'] = 0.5
		vals['smoketransp'] = 0.5
	elif preset == 'HoudiniVolume':
		# fire
		vals['emult'] = 0.1
		vals['etmult'] = 1.0
		vals['blackbody'] = 0.0
		vals['fire_opacity_mode'] = 1 # Fully Visible

		# smoke
		vals['noscatter'] = 3 # Approximate + Shadows
		vals['smoketransp'] = 0.2
	elif preset == 'HoudiniLiquid':
		vals['renderMode'] = 4 # Mesh
	elif preset == 'MayaFluids':
		# fire
		vals['emult'] = 31.0
		vals['blackbody'] = 0.0
		vals['fire_opacity_mode'] = 0 # Use Smoke Opacity

		# smoke
		vals['darg'] = 6 # Constant color
		vals['noscatter'] = 3 # Approximate + Shadows
		vals['transfext'] = 0.6
		vals['targ'] = 2 # Smoke

	for key in vals:
		shaderNode.parm(key).set(vals[key])

	shaderNode.parm('selectedSopPath').set(cacheNode.path())
	shaderNode.parm('setPresetParam').set(preset)
	shaderNode.parm('setPresetParam').pressButton()


def apply_preset(preset):
	selectedNodes = hou.selectedNodes()

	if len(selectedNodes) == 0:
		geo = hou.node('/obj').createNode('geo')
		for n in geo.children():
			n.destroy()
		selectedNodes = [geo]

	doSelected = len(selectedNodes) == 1

	for node in selectedNodes:
		node.setSelected(False)
		if node.type().name() == 'geo':
			matPath = node.parm('shop_materialpath').evalAsString()
			phxShaderNode = _setup_phx_material_node(node, hou.node(matPath))
			node.parm('shop_materialpath').set(phxShaderNode.parent().path())
			matPath = node.parm('shop_materialpath').evalAsString()
			cacheNode = _setup_phx_cache_node(node)
			_set_phx_presets(preset, phxShaderNode, cacheNode)
			if doSelected:
				cacheNode.setSelected(True)
