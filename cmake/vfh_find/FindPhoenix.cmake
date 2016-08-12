#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

string(TOLOWER "${CMAKE_HOST_SYSTEM_NAME}" _HOST_SYSTEM_NAME)

set(PHXSDK_PATH "" CACHE PATH "V-Ray Phoenix SDK root location")
set(PHXSDK_VERSION "" CACHE STRING "V-Ray Phoenix SDK version")

if(PHXSDK_PATH)
	set(_phx_root ${PHXSDK_PATH})
else()
	# no phxsdk root path is specified
	if(NOT PHXSDK_VERSION)
		message(FATAL_ERROR "V-Ray Phoenix SDK version NOT specified")
	endif()

	if(SDK_PATH)
		# if vfh sdk path is given use it to deduce phxsdk root location based on version and cache
		set(_phx_root "${SDK_PATH}/${_HOST_SYSTEM_NAME}/phxsdk/phxsdk${PHXSDK_VERSION}")
	else()
		# otherwise guess root location depanding on the host system and cache
		if(WIN32)
			# windows
			set(_phx_root "C:/Program Files/Chaos Group/Phoenix FD/Maya ${PHXSDK_VERSION} for x64/SDK")
		elseif(APPLE)
			# mac os
			set(_phx_root "")
		else()
			# linux
			set(_phx_root "/usr/ChaosGroup/PhoenixFD/Maya${PHXSDK_VERSION}-x64/SDK")
		endif()
	endif()
endif()

# check if phoenix path exists
if(NOT EXISTS ${_phx_root})
	message(FATAL_ERROR "V-Ray Phoenix SDK path NOT found: (\"${_phx_root}\")")
endif()


message(STATUS "Phoenix SDK path = ${_phx_root}")
message(STATUS "Phoenix SDK version = ${PHXSDK_VERSION}")

if(WIN32)
	set(CGR_PHOENIX_SHARED aurloader.dll)
	set(CGR_PHOENIX_SHARED_F3D field3dio_phx.dll)
	set(CGR_PHOENIX_SHARED_VDB openvdbio_phx.dll)
else()
	set(CGR_PHOENIX_SHARED libaurloader.so)
	set(CGR_PHOENIX_SHARED_F3D libfield3dio_phx.so)
	set(CGR_PHOENIX_SHARED_VDB libopenvdbio_phx.so)
endif()


set(Phoenix_FOUND TRUE)
set(Phoenix_INCLUDES "${_phx_root}/include")
set(Phoenix_LIBRARIES "${_phx_root}/lib")

# check for headers
foreach(loop_var IN ITEMS "aurinterface.h")

	if(NOT EXISTS "${Phoenix_INCLUDES}/${loop_var}" )
		message(STATUS "Invalid Phoenix SDK path - missing file ${Phoenix_INCLUDES}/${loop_var} ")
		set(Phoenix_FOUND FALSE)
		break()
	endif()

endforeach(loop_var)

# check for libs
foreach(loop_var IN ITEMS   "${CGR_PHOENIX_SHARED}"
							"${CGR_PHOENIX_SHARED_F3D}"
							"${CGR_PHOENIX_SHARED_VDB}")

	if(NOT EXISTS "${Phoenix_LIBRARIES}/${loop_var}" )
		message(STATUS "Invalid Phoenix SDK path - missing file ${Phoenix_LIBRARIES}/${loop_var} ")
		set(Phoenix_FOUND FALSE)
		break()
	endif()

endforeach(loop_var)

if(NOT Phoenix_FOUND)
	set(Phoenix_INCLUDES Phoenix_INCLUDES-NOTFOUND)
	set(Phoenix_LIBRARIES Phoenix_LIBRARIES-NOTFOUND)
endif()
