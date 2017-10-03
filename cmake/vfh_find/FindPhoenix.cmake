#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

include(vfh_macro)

set(PHXSDK_PATH "" CACHE PATH "Phoenix SDK root location")

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

if((NOT _phx_root) OR (NOT EXISTS ${_phx_root}))
	set(Phoenix_FOUND FALSE)
else()
	set(Phoenix_INCLUDES  "${_phx_root}/include")
	set(Phoenix_LIBRARIES "${_phx_root}/lib")
	set(Phoenix_LIBS)

	vfh_find_file(VAR CGR_PHOENIX_SHARED
		NAMES
			${CMAKE_SHARED_LIBRARY_PREFIX}aurloader${CMAKE_SHARED_LIBRARY_SUFFIX}
		PATHS
			${Phoenix_LIBRARIES}
	)

	vfh_find_file(VAR CGR_PHOENIX_SHARED_F3D
		NAMES
			${CMAKE_SHARED_LIBRARY_PREFIX}field3dio_phx${CMAKE_SHARED_LIBRARY_SUFFIX}
		PATHS
			${Phoenix_LIBRARIES}
	)

	vfh_find_file(VAR CGR_PHOENIX_SHARED_VDB
		NAMES
			${CMAKE_SHARED_LIBRARY_PREFIX}openvdbio_phx${CMAKE_SHARED_LIBRARY_SUFFIX}
		PATHS
			${Phoenix_LIBRARIES}
	)

	set(CGR_PHOENIX_LIB_NAMES
		aurloader
		aurloader_s
		aurramps_s
		guiwin_qt_s
		iutils_s
	)

	foreach(phx_static_lib_name IN ITEMS ${CGR_PHOENIX_LIB_NAMES})
		vfh_find_library(VAR CGR_PHOENIX_STATIC_LIB_${phx_static_lib_name}
			NAMES
				${phx_static_lib_name}
			PATHS
				${Phoenix_LIBRARIES}
		)
		if(CGR_PHOENIX_STATIC_LIB_${phx_static_lib_name})
			list(APPEND Phoenix_LIBS ${phx_static_lib_name})
		endif()
	endforeach()

	find_path(CGR_PHOENIX_HEADERS
		NAMES
			aura_ver.h
			aurinterface.h
			aurloader.h
			ramps.h
		PATHS
			${Phoenix_INCLUDES}
	)

	if(CGR_PHOENIX_SHARED AND
	   CGR_PHOENIX_SHARED_F3D AND
	   CGR_PHOENIX_SHARED_VDB AND
	   CGR_PHOENIX_HEADERS)
		set(Phoenix_FOUND TRUE)
	endif()
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

	set(Phoenix_INCLUDES  FALSE)
	set(Phoenix_LIBRARIES FALSE)
	set(Phoenix_LIBS FALSE)
endif()
