import os
import hou


class VRayProxyDialog:
	def __init__(self):
		self.__ui = None


	def initUI(self):
		uipath = os.path.join( os.path.dirname(__file__), "VRayProxy_ui.ui" )
		self.__ui = hou.ui.createDialog( uipath )

		self.__onExportModeChange()
		self.__onExportAnimationChange()
		self.__onExportPCLsChange()
		self.__onVoxelPerMeshChange()
		self.__onAutoCreateProxiesChange()

		self.__ui.addCallback("exportMode.val", self.__onExportModeChange)
		self.__ui.addCallback("exportAnimation.val", self.__onExportAnimationChange)
		self.__ui.addCallback("exportVelocity.val", self.__onExportVelocityChange)
		self.__ui.addCallback("animPlayback.val", self.__onAnimPlaybackRangeChange)
		self.__ui.addCallback("exportPCL.val", self.__onExportPCLsChange)
		self.__ui.addCallback("voxelPerMesh.val", self.__onVoxelPerMeshChange)
		self.__ui.addCallback("autoCreateProxies.val", self.__onAutoCreateProxiesChange)
		self.__ui.addCallback("createProxy.val", self.__onCreateProxy)


	def show(self, val):
		if not self.__ui:
			self.initUI()

		filename = ""
		selected = filter(lambda item: (item.type().description() == 'Geometry'), hou.selectedNodes())
		selected = map( lambda item: item.name(), selected )
		if len(selected):
			filename = "%s.vrmesh" % selected[0]

		self.__ui.setValue("selection.val", ", ".join(selected))
		self.__ui.setValue("filename.val", filename)
		self.__ui.setValue("show.val", val)


	def __onExportModeChange(self):
		exportMode = bool( self.__ui.value("exportMode.val") )
		self.__ui.enableValue("filename.val", exportMode)
		self.__ui.enableValue("includeTransform.val", not exportMode)
		self.__ui.enableValue("previewLastSelected.val", exportMode)


	def __onExportAnimationChange(self):
		exportAnimation = bool( self.__ui.value("exportAnimation.val") )
		self.__ui.enableValue("exportAnimationGR.val", exportAnimation)

		self.__onExportVelocityChange()
		self.__onAnimPlaybackRangeChange()


	def __onExportVelocityChange(self):
		exportAnimation = bool( self.__ui.value("exportAnimation.val") )
		exportVelocity = bool( self.__ui.value("exportVelocity.val") )
		self.__ui.enableValue("exportVelocityGR.val", (exportAnimation and exportVelocity))


	def __onAnimPlaybackRangeChange(self):
		exportAnimation = bool( self.__ui.value("exportAnimation.val") )
		animPlayback = self.__ui.value("animPlayback.val")
		self.__ui.enableValue("animPlaybackGR.val", exportAnimation and (animPlayback == 2))


	def __onExportPCLsChange(self):
		exportPCL = bool( self.__ui.value("exportPCL.val") )
		self.__ui.enableValue("pointSize.val", exportPCL)


	def __onVoxelPerMeshChange(self):
		voxelPerMesh = bool( self.__ui.value("voxelPerMesh.val") )
		self.__ui.enableValue("maxFacesPerVoxel.val", not voxelPerMesh)


	def __onAutoCreateProxiesChange(self):
		autoCreateProxies = bool( self.__ui.value("autoCreateProxies.val") )
		self.__ui.enableValue("autoCreateProxiesGR.val", autoCreateProxies)


	def __onCreateProxy(self):
		# @ brief: builds cmdline args for vrayproxy cmd based on UI options and calls it
		# varyproxy cmd usage:
		# vrayproxy -m exportMode -d filepath [-n filename] [-t] [-f] [-l]
		# 			[-a animStart animEnd] [-v velocityStart velocityEnd]
		# 			[-P pointSize] [-F maxPreviewFaces] [-H maxPreviewStrands]
		# 			[-T previewType] [-c] [-i] [-x maxFacesPerVoxel]
		# 			[-A -N newNodeName [-b]]

		# -m exportMode
		# 	exportMode = 0 - export each object in separate file
		# 	exportMode = 1 - export all selected objects in single file
		# -d filepath
		# 	filepath- ath to the folder where the .vrmesh file(s) will be saved
		# -n filename
		# 	the name of the .vrmesh file if exporting in a single file
		# -t
		# 	export geometry in world space
		# -f
		# 	overwrite files with same file name
		# -l
		# 	use last selected object as preview geometry
		# -a animStart animEnd
		# 	export frame range, animStart = strat frame, animEnd = end frame
		# -v velocityStart velocityEnd
		# 	export vertex velocity, velocityStart = start sample time in [0,1) range
		# 							velocityEnd = end sample time in (0,1] range
		# -P pointSize
		# 	point cloud information will be computed and stored with each voxel in the file
		# 	pointSize -  average area covered by one point
		# -F maxPreviewFaces
		# 	max number of faces for preview geometry voxel
		# -H maxPreviewStrands
		# 	max number of strands for preview geometry voxel
		# -T previewType
		# 	specifies how to do the simplification
		# 	previewType = 0 - SIMPLIFY_CLUSTERING
		# 	previewType = 1 - SIMPLIFY_COMBINED
		# 	previewType = 2 - SIMPLIFY_FACE_SAMPLING
		# -c
		# 	export point color sets
		# -i
		# 	ignore hidden and templated geometry
		# -x maxFacesPerVoxel
		# 	maxFacesPerVoxel - specifies max number of faces per voxel
		# -A
		# 	create VRayProxy node for each exported .vrmesh file
		# -N newNodeName
		# 	specifies name prefix for newly creaded VRayProxy nodes
		# -b
		# 	make backup file with selection

		exportMode = self.__ui.value("exportMode.val")
		filepath = hou.expandString( self.__ui.value("filepath.val") );

		filename = self.__ui.value("filename.val");
		facesInPreview = self.__ui.value("facesInPreview.val")
		strandsInPreview = self.__ui.value("strandsInPreview.val")
		previewMode = self.__ui.value("previewMode.val")

		cmd_args = "-d \"%s\" -n \"%s\" -m %d -F %d -H %d -T %d" % (filepath, filename, exportMode, facesInPreview, strandsInPreview, previewMode)

		includeTransform = bool( self.__ui.value("includeTransform.val") )
		if (exportMode == 1) or includeTransform:
			cmd_args += " -t"

		overwriteFiles = bool( self.__ui.value("overwriteFiles.val") )
		if overwriteFiles:
			cmd_args += " -f"

		previewLastSelected = bool( self.__ui.value("previewLastSelected.val") )
		if (exportMode == 1) and previewLastSelected:
			cmd_args += " -l"

		exportAnimation = bool( self.__ui.value("exportAnimation.val") )
		if exportAnimation:
			# animPlayback is one of {0 = Use playback range, 1 = Use animation range, 2 = User specified range }
			animPlayback = self.__ui.value("animPlayback.val")
			animStart = 0
			animEnd = 0

			if animPlayback == 0:
				animStart = int( hou.expandString('$RFSTART') )
				animEnd = int( hou.expandString('$RFEND') )

			elif animPlayback == 1:
				animStart = int( hou.expandString('$FSTART') )
				animEnd = int( hou.expandString('$FEND') )

			elif animPlayback == 2:
				animStart = self.__ui.value("animStart.val")
				animEnd = self.__ui.value("animEnd.val")

			cmd_args += " -a %d %d" % (animStart, animEnd)

			exportVelocity = bool( self.__ui.value("exportVelocity.val") )
			if exportVelocity:
				velocityStart = self.__ui.value("velocityStart.val")
				velocityEnd = self.__ui.value("velocityEnd.val")

				cmd_args += " -v %f %f" % (velocityStart, velocityEnd)

		exportPCL = bool( self.__ui.value("exportPCL.val") )
		if exportPCL:
			pointSize = self.__ui.value("pointSize.val")
			cmd_args += " -P %f" % pointSize

		exportVertexColors = bool( self.__ui.value("exportColors.val") )
		if exportVertexColors:
			cmd_args += " -c"

		ignoreHidden = bool( self.__ui.value("ignoreHidden.val") )
		if ignoreHidden:
			cmd_args += " -i"

		voxelPerMesh = bool( self.__ui.value("voxelPerMesh.val") )
		if not voxelPerMesh:
			maxFacesPerVoxel = self.__ui.value("maxFacesPerVoxel.val")
			cmd_args += " -x %d" % (maxFacesPerVoxel)

		autoCreateProxies = bool( self.__ui.value("autoCreateProxies.val") )
		if autoCreateProxies:
			newNodeName = self.__ui.value("newNodeName.val")
			cmd_args += " -A -N %s" % (newNodeName)

			backup = bool( self.__ui.value("backup.val") )
			if backup:
				cmd_args += " -b"

		print "Call vrayproxy cmd", cmd_args

		res = hou.hscript( "vrayproxy %s" % cmd_args )
		print "vrayproxy cmd result", res[0]
		self.show(0)

