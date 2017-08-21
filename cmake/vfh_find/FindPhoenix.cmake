#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

set(PHXSDK_PATH "" CACHE PATH "Phoenix SDK root location")
set(PHXSDK_VERSION "" CACHE STRING "Phoenix SDK version")

if(PHXSDK_PATH)
	# if PHXSDK_PATH is specified then just take it as root location for Phoenix sdk
	set(_phx_root ${PHXSDK_PATH})
else()
	# no Phoenix sdk root path is passed to cmake
	if(SDK_PATH)
		set(_phx_root "${SDK_PATH}/phxsdk/qt${HOUDINI_QT_VERSION}")
		if(WIN32)
			set(_phx_root "${_phx_root}/${HDK_RUNTIME}")
		endif()
	else()
		# otherwise search for Phoenix for Maya default installation path
		set(_maya_versions "2017;2016;2015;2014")
		foreach(_maya_version ${_maya_versions})
			if(WIN32)
				# windows
				set(_phx_for_maya_root "C:/Program Files/Chaos Group/Phoenix FD/Maya ${_maya_version} for x64/SDK")
			elseif(APPLE)
				# mac os
				set(_phx_for_maya_root "")
			else()
				# linux
				set(_phx_for_maya_root "/usr/ChaosGroup/PhoenixFD/Maya${_maya_version}-x64/SDK")
			endif()

			if(EXISTS ${_phx_for_maya_root})
				set(_phx_root ${_phx_for_maya_root})
				message(STATUS "No path specified for Phoenix SDK. Fall back to default search path from Phoenix for Maya installation: ${_phx_for_maya_root}")
				break()
			endif()

		endforeach(_maya_version)
	endif()
endif()


message(STATUS "Phoenix SDK search path: ${_phx_root}")


# check if path exists
if((NOT _phx_root) OR (NOT EXISTS ${_phx_root}))

	set(Phoenix_FOUND FALSE)

else()

	if(WIN32)
		set(CGR_PHOENIX_SHARED      aurloader.dll)
		set(CGR_PHOENIX_SHARED_F3D  field3dio_phx.dll)
		set(CGR_PHOENIX_SHARED_VDB  openvdbio_phx.dll)
		set(CGR_PHOENIX_STATIC_LIBS guiwin_qt_s.lib
									iutils_s.lib
									aurramps_s.lib
									aurloader_s.lib
									gui_utils_s.lib)
	else()
		set(CGR_PHOENIX_SHARED      libaurloader.so)
		set(CGR_PHOENIX_SHARED_F3D  libfield3dio_phx.so)
		set(CGR_PHOENIX_SHARED_VDB  libopenvdbio_phx.so)
		set(CGR_PHOENIX_STATIC_LIBS libguiwin_qt_s.a
									libiutils_s.a
									libaurramps_s.a
									libaurloader_s.a
									libgui_utils_s.a)
	endif()


	set(Phoenix_FOUND TRUE)
	set(Phoenix_INCLUDES "${_phx_root}/include")
	set(Phoenix_LIBRARIES "${_phx_root}/lib")

	# check for headers
	foreach(loop_var IN ITEMS   "aurinterface.h"
								"aurloader.h"
								"aura_ver.h"
								"ramps.h")

		if(NOT EXISTS "${Phoenix_INCLUDES}/${loop_var}" )
			message(STATUS "Invalid Phoenix SDK path - missing file ${Phoenix_INCLUDES}/${loop_var} ")
			set(Phoenix_FOUND FALSE)
			break()
		endif()

	endforeach(loop_var)

	# check for libs
	foreach(loop_var IN ITEMS   "${CGR_PHOENIX_SHARED}"
								"${CGR_PHOENIX_SHARED_F3D}"
								"${CGR_PHOENIX_SHARED_VDB}"
								 ${CGR_PHOENIX_STATIC_LIBS})

		if(NOT EXISTS "${Phoenix_LIBRARIES}/${loop_var}" )
			message(STATUS "Invalid Phoenix SDK path - missing file ${Phoenix_LIBRARIES}/${loop_var} ")
			set(Phoenix_FOUND FALSE)
			break()
		endif()

	endforeach(loop_var)

endif()


# check if all paths exist
if(Phoenix_FOUND)
	foreach(loop_var IN ITEMS ${Phoenix_INCLUDES})
		if(NOT EXISTS ${loop_var})
			set(Phoenix_FOUND FALSE)
			break()
		endif()
	endforeach(loop_var)
	foreach(loop_var IN ITEMS ${Phoenix_LIBRARIES})
		if(NOT EXISTS ${loop_var})
			set(Phoenix_FOUND FALSE)
			break()
		endif()
	endforeach(loop_var)
endif()


if(NOT Phoenix_FOUND)
	message(WARNING "Phoenix SDK NOT found!")

	set(Phoenix_INCLUDES Phoenix_INCLUDES-NOTFOUND)
	set(Phoenix_LIBRARIES Phoenix_LIBRARIES-NOTFOUND)
endif()
