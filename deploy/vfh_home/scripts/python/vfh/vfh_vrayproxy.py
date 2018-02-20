#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import os
import hou

class VRayProxyDialog:
	def __init__(self):
		self.__ui = None

	def initUI(self):
		uipath = os.path.join(os.path.dirname(__file__), "vfh_vrayproxy_export.ui")
		self.__ui = hou.ui.createDialog( uipath )

		self.__onExportAnimationChange()
		self.__onExportPCLsChange()
		self.__onVoxelPerMeshChange()

		self.__ui.addCallback("exp_animation.val", self.__onExportAnimationChange)
		self.__ui.addCallback("exp_velocity.val", self.__onExportVelocityChange)
		self.__ui.addCallback("animplayback.val", self.__onAnimPlaybackRangeChange)
		self.__ui.addCallback("exp_pcls.val", self.__onExportPCLsChange)
		self.__ui.addCallback("voxelpermesh.val", self.__onVoxelPerMeshChange)
		self.__ui.addCallback("createproxy.val", self.__onCreateProxy)

	def show(self, val):
		if not self.__ui:
			self.initUI()

		selected = filter(lambda item: (item.type().name() == 'geo'), hou.selectedNodes())
		selected = map( lambda item: item.path(), selected )

		self.__ui.setValue("objects.val", ", ".join(selected))
		self.__ui.setValue("show.val", val)

	def __onExportAnimationChange(self):
		exportAnimation = bool( self.__ui.value("exp_animation.val") )
		self.__ui.enableValue("exp_animation_group.val", exportAnimation)

		self.__onExportVelocityChange()
		self.__onAnimPlaybackRangeChange()

	def __onExportVelocityChange(self):
		exportAnimation = bool( self.__ui.value("exp_animation.val") )
		exportVelocity = bool( self.__ui.value("exp_velocity.val") )
		self.__ui.enableValue("exp_velocity_group.val", (exportAnimation and exportVelocity))

	def __onAnimPlaybackRangeChange(self):
		exportAnimation = bool( self.__ui.value("exp_animation.val") )
		animPlayback = self.__ui.value("animplayback.val")
		self.__ui.enableValue("animplayback_group.val", exportAnimation and (animPlayback == 2))

	def __onExportPCLsChange(self):
		exportPCL = bool( self.__ui.value("exp_pcls.val") )
		self.__ui.enableValue("pointsize.val", exportPCL)

	def __onVoxelPerMeshChange(self):
		voxelPerMesh = bool( self.__ui.value("voxelpermesh.val") )
		self.__ui.enableValue("max_facespervoxel.val", not voxelPerMesh)

	def __onCreateProxy(self):
		# @brief builds cmdline args for vrayproxy cmd based on UI options and calls it
		# @note see vfh_home/help/command.help file or type 'help vrayproxy' in textport
		#       for detailed info on vrayproxy usage and command line options.
		cmd_args = "-n {}".format( hou.expandString( self.__ui.value("filepath.val") ) )

		if bool( self.__ui.value("mkpath.val") ):
			cmd_args += " -c"

		if bool( self.__ui.value("overwrite.val") ):
			cmd_args += " -f"

		if bool( self.__ui.value("exp_separately.val") ):
			cmd_args += " -m"

		if bool( self.__ui.value("save_hidden.val") ):
			cmd_args += " -i"

		if bool( self.__ui.value("lastaspreview.val") ):
			cmd_args += " -l"

		if bool( self.__ui.value("xformtype.val") ):
			cmd_args += " -t"

		if bool( self.__ui.value("exp_animation.val") ):
			# animplayback is one of {0 = Use playback range, 1 = Use animation range, 2 = User specified range }
			animPlayback = self.__ui.value("animplayback.val")
			animStart = 0
			animEnd = 0

			if animPlayback == 0:
				animStart = int( hou.expandString('$RFSTART') )
				animEnd = int( hou.expandString('$RFEND') )

			elif animPlayback == 1:
				animStart = int( hou.expandString('$FSTART') )
				animEnd = int( hou.expandString('$FEND') )

			elif animPlayback == 2:
				animStart = self.__ui.value("animstart.val")
				animEnd = self.__ui.value("animend.val")

			cmd_args += " -a {} {}".format(animStart, animEnd)

			exportVelocity = bool( self.__ui.value("exp_velocity.val") )
			if exportVelocity:
				velocityStart = self.__ui.value("velocitystart.val")
				velocityEnd = self.__ui.value("velocityend.val")

				cmd_args += " -v {} {}".format(velocityStart, velocityEnd)

		cmd_args += " -T {} -F {} -H {}".format(
			self.__ui.value("simplificationtype.val"),
			self.__ui.value("max_previewfaces.val"),
			self.__ui.value("max_previewstrands.val")
			)

		if not bool( self.__ui.value("voxelpermesh.val") ):
			maxFacesPerVoxel = self.__ui.value("max_facespervoxel.val")
			cmd_args += " -X {}".format(maxFacesPerVoxel)

		if bool( self.__ui.value("exp_pcls.val") ):
			pointSize = self.__ui.value("pointsize.val")
			cmd_args += " -P {}".format(pointSize)

		cmd_args += " {}".format( self.__ui.value("objects.val") )

		print("Calling: vrayproxy {}".format(cmd_args))
		res = hou.hscript( "vrayproxy {}".format(cmd_args) )
		print("\n".join(res))

		self.show(0)

def add_proxy_import_to_selected_sop():
	nodeToSelect = None
	enterInNode = False

	for node in hou.selectedNodes():
		if node.type().name() == 'geo':
			created = node.createNode('VRayNodeVRayProxy')
			if not nodeToSelect:
				nodeToSelect = created
				enterInNode = True
			else:
				# nodeToSelect is assigned
				enterInNode = False

	if enterInNode:
		nodeToSelect.setSelected(True)

	if not nodeToSelect:
		sys.stderr.write('No selected SOP nodes to import proxy into!')
		sys.stderr.flush()
