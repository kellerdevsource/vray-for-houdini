#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

set(VRAYSDK_PATH "" CACHE PATH "V-Ray SDK root location")
set(VRAYSDK_VERSION "" CACHE STRING "V-Ray SDK version")

if(VRAYSDK_PATH)
	# if VRAYSDK_PATH is specified then just take it as root location for V-Ray SDK
	set(_vraysdk_root ${VRAYSDK_PATH})
else()
	# no V-Ray SDK root path is passed to cmake
	if(SDK_PATH)
		set(_vraysdk_root "${SDK_PATH}/vraysdk")
		if(WIN32)
			set(_vraysdk_root "${_vraysdk_root}/${HDK_RUNTIME}")
		endif()
	else()
		# otherwise search for V-Ray for Maya default installation path
		set(_maya_versions "2017;2016;2015;2014")
		foreach(_maya_version ${_maya_versions})
			if(WIN32)
				# windows
				set(_vray_for_maya_root "C:/Program Files/Chaos Group/V-Ray/Maya ${_maya_version} for x64")
			elseif(APPLE)
				# mac os
				set(_vray_for_maya_root "/Applications/ChaosGroup/V-Ray/Maya${_maya_version}")
			else()
				# linux
				set(_vray_for_maya_root "/usr/ChaosGroup/V-Ray/Maya${_maya_version}-x64")
			endif()

			if(EXISTS ${_vray_for_maya_root})
				set(_vraysdk_root ${_vray_for_maya_root})
				message(STATUS "No path specified for V-Ray SDK. Fall back to default search path from V-Ray For Maya installation: ${_vraysdk_root}")
				break()
			endif()

		endforeach(_maya_version)
	endif()
endif()


message(STATUS "V-Ray SDK search path: ${_vraysdk_root}")


# check if path exists
if((NOT _vraysdk_root) OR (NOT EXISTS ${_vraysdk_root}))

	set(VRaySDK_FOUND FALSE)

else()

	set(VRaySDK_FOUND TRUE)
	set(VRaySDK_INCLUDES "${_vraysdk_root}/include")
	set(VRaySDK_LIBRARIES "${_vraysdk_root}/lib")

	set(_vraysdk_libdirs "")

	# search for os lib subfolder
	file(GLOB_RECURSE _vraysdk_files "${VRaySDK_LIBRARIES}/*tiff_s*")
	if(NOT _vraysdk_files)
		set(VRaySDK_FOUND FALSE)
	else()
		foreach(loop_var IN ITEMS ${_vraysdk_files})
			get_filename_component(_vrasdk_libdir ${loop_var} DIRECTORY)
			list(APPEND _vraysdk_libdirs ${_vrasdk_libdir})
		endforeach(loop_var)
	endif()

	# search for compiler lib subfolder
	file(GLOB_RECURSE _vraysdk_files "${VRaySDK_LIBRARIES}/*vutils_s*")
	if(NOT _vraysdk_files)
		set(VRaySDK_FOUND FALSE)
	else()
		foreach(loop_var IN ITEMS ${_vraysdk_files})
			get_filename_component(_vrasdk_libdir ${loop_var} DIRECTORY)
			list(APPEND _vraysdk_libdirs ${_vrasdk_libdir})
		endforeach(loop_var)
	endif()

	if(NOT _vraysdk_libdirs)
		set(VRaySDK_FOUND FALSE)
	else()
		list(APPEND VRaySDK_LIBRARIES ${_vraysdk_libdirs})
	endif()

	set(CGR_HAS_VRSCENE FALSE)
	find_path(__cgr_vrscene_h vrscene_preview.h PATHS ${VRaySDK_INCLUDES})
	find_library(__cgr_vrscene_lib vrscene_s PATHS ${VRaySDK_LIBRARIES})
	if (__cgr_vrscene_h AND __cgr_vrscene_lib)
		set(CGR_HAS_VRSCENE TRUE)
	endif()

	unset(__cgr_vrscene_h CACHE)
	unset(__cgr_vrscene_lib CACHE)

endif()


# check if all paths exist
if(VRaySDK_FOUND)
	foreach(loop_var IN ITEMS ${VRaySDK_INCLUDES})
		if(NOT EXISTS ${loop_var})
			set(VRaySDK_FOUND FALSE)
			break()
		endif()
	endforeach(loop_var)
	foreach(loop_var IN ITEMS ${VRaySDK_LIBRARIES})
		if(NOT EXISTS ${loop_var})
			set(VRaySDK_FOUND FALSE)
			break()
		endif()
	endforeach(loop_var)
endif()


if(NOT VRaySDK_FOUND)
	message(WARNING "V-Ray SDK NOT found!")

	set(VRaySDK_INCLUDES VRaySDK_INCLUDES-NOTFOUND)
	set(VRaySDK_LIBRARIES VRaySDK_LIBRARIES-NOTFOUND)
else()
	message_array("Using V-Ray SDK include path" VRaySDK_INCLUDES)
	message_array("Using V-Ray SDK library path" VRaySDK_LIBRARIES)
endif()
