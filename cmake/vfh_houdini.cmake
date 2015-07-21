#
# Copyright (c) 2014, Chaos Software Ltd
#
# Build system based on CMake
#
# Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
#
# All rights reserved. These coded instructions, statements and
# computer programs contain unpublished information proprietary to
# Chaos Group Ltd, which is protected by the appropriate copyright
# laws and may not be disclosed to third parties or copied or
# duplicated, in whole or in part, without prior written consent of
# Chaos Group Ltd.
#

option(HOUDINI_DEFAULT_PATH "Use default Houdini installation path" ON)

set(HOUDINI_VERSION       "14.0" CACHE STRING "Houdini major version")
set(HOUDINI_VERSION_BUILD "248"  CACHE STRING "Houdini build version")

set(HOUDINI_DEFINES
	-DAMD64
	-DSIZEOF_VOID_P=8
	-DSESI_LITTLE_ENDIAN
	-D_USE_MATH_DEFINES
	-DMAKING_DSO
	-DVERSION=${HOUDINI_VERSION}
	-DHOUDINI_DSO_VERSION=${HOUDINI_VERSION}
	-DUT_DSO_TAGINFO=${PLUGIN_TAGINFO}
)

set(HOUDINI_LINK_FLAGS "")
set(HOUDINI_INSTALL_ROOT "" CACHE PATH "Houdini install path")

if (APPLE)
	set(HOUDINI_DEF_PATH "/Applications/Houdini ${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}")
	if (HOUDINI_DEFAULT_PATH)
		set(HOUDINI_INSTALL_ROOT "${HOUDINI_DEF_PATH}" CACHE PATH "" FORCE)
	endif()

	set(HOUDINI_FRAMEWORK_ROOT "/Library/Frameworks/Houdini.framework/Versions/${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}")

	set(HOUDINI_INCLUDE_PATH "${HOUDINI_FRAMEWORK_ROOT}/Resources/toolkit/include")
	set(HOUDINI_LIB_PATH     "${HOUDINI_FRAMEWORK_ROOT}/Libraries")

	set(HOUDINI_HOME_PATH "$ENV{HOME}/Library/Preferences/houdini/${HOUDINI_VERSION}")

	set(HOUDINI_BIN_PATH "${HOUDINI_INSTALL_ROOT}/Houdini FX.app/Contents/MacOS")
elseif(WIN32)
	set(HOUDINI_DEF_PATH "C:/Program Files/Side Effects Software/Houdini ${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}")
	if (HOUDINI_DEFAULT_PATH)
		set(HOUDINI_INSTALL_ROOT "${HOUDINI_DEF_PATH}" CACHE PATH "" FORCE)
	endif()

	set(HOUDINI_INCLUDE_PATH "${HOUDINI_INSTALL_ROOT}/toolkit/include")
	set(HOUDINI_LIB_PATH     "${HOUDINI_INSTALL_ROOT}/custom/houdini/dsolib")

	set(USER_HOME "$ENV{HOME}")
	if(USER_HOME STREQUAL "")
		set(USER_HOME "$ENV{USERPROFILE}/Documents")
	endif()

	set(HOUDINI_HOME_PATH "${USER_HOME}/houdini${HOUDINI_VERSION}")

	set(HOUDINI_BIN_PATH "${HOUDINI_INSTALL_ROOT}/bin")
else()
	set(HOUDINI_DEF_PATH "/opt/hfs${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}")
	if (HOUDINI_DEFAULT_PATH)
		set(HOUDINI_INSTALL_ROOT "${HOUDINI_DEF_PATH}" CACHE PATH "" FORCE)
	endif()

	set(HOUDINI_INCLUDE_PATH "${HOUDINI_INSTALL_ROOT}/toolkit/include")
	set(HOUDINI_LIB_PATH     "${HOUDINI_INSTALL_ROOT}/dsolib")

	set(HOUDINI_HOME_PATH "$ENV{HOME}/houdini${HOUDINI_VERSION}")

	set(HOUDINI_BIN_PATH "${HOUDINI_INSTALL_ROOT}/bin")
endif()

# Local install plugin path
set(HOUDINI_PLUGIN_PATH "${HOUDINI_HOME_PATH}/dso")

if(WIN32)
	list(APPEND HOUDINI_DEFINES
		-DI386
		-DWIN32
		-DSWAP_BITFIELDS
		-D_WIN32_WINNT=0x0501
		-DWINVER=0x0501
		-DNOMINMAX
		-DSTRICT
		-DWIN32_LEAN_AND_MEAN
		-D_USE_MATH_DEFINES
		-D_CRT_SECURE_NO_DEPRECATE
		-D_CRT_NONSTDC_NO_DEPRECATE
		-D_SCL_SECURE_NO_WARNINGS
		-DBOOST_ALL_NO_LIB
		-DFBX_ENABLED=1
		-DOPENCL_ENABLED=1
		-DOPENVDB_ENABLED=1
	)

	file(GLOB HOUDINI_LINK_FLAGS "${HOUDINI_LIB_PATH}/*.a")

	set(SYSTEM_LIBS
		advapi32
		comctl32
		comdlg32
		gdi32
		kernel32
		msvcprt
		msvcrt
		odbc32
		odbccp32
		oldnames
		ole32
		oleaut32
		rayserver_s
		shell32
		user32
		uuid
		winspool
		ws2_32
	)

	list(APPEND HOUDINI_LINK_FLAGS ${SYSTEM_LIBS})

	list(APPEND HOUDINI_LINK_FLAGS
		QtCore4
		QtGui4
	)

else()
	list(APPEND HOUDINI_DEFINES
		-DUSE_PTHREADS
		-DENABLE_THREADS
		-DENABLE_UI_THREADS
		-D_GNU_SOURCE
		-DGCC4
	)

	set(HOUDINI_LINK_FLAGS
		HoudiniUI
		HoudiniOPZ
		HoudiniOP3
		HoudiniOP2
		HoudiniOP1
		HoudiniSIM
		HoudiniGEO
		HoudiniPRM
		HoudiniUT
	)

	if(APPLE)
		list(APPEND HOUDINI_DEFINES
			-D_REENTRANT
			-DNEED_SPECIALIZATION_STORAGE
			-DMBSD
			-DMBSD_COCOA
			-DMBSD_INTEL
			-DFBX_ENABLED=1
			-DOPENCL_ENABLED=1
			-DOPENVDB_ENABLED=1
		)

		list(APPEND HOUDINI_LINK_FLAGS
			z
			dl
			tbb
			pthread
			QtCore
			QtGui
			"-framework Cocoa"
		)

	else()
		list(APPEND HOUDINI_DEFINES
			-DLINUX
			-DFBX_ENABLED=1
			-DOPENCL_ENABLED=1
			-DOPENVDB_ENABLED=1
		)

		list(APPEND HOUDINI_LINK_FLAGS
			GLU
			GL
			X11
			Xext
			Xi
			dl
			pthread
		)
	endif()
endif()


macro(use_houdini_sdk)
	add_definitions(${HOUDINI_DEFINES})
	include_directories(${HOUDINI_INCLUDE_PATH})
	link_directories(${HOUDINI_LIB_PATH})
endmacro()


macro(houdini_plugin name sources)
	set(libraryName ${name})
	add_library(${libraryName} SHARED ${sources})
	set_target_properties(${libraryName} PROPERTIES PREFIX "")
	target_link_libraries(${libraryName} ${HOUDINI_LINK_FLAGS})
	install(TARGETS ${libraryName} DESTINATION ${HOUDINI_PLUGIN_PATH})
endmacro()

message(STATUS "Using Houdini ${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}: ${HOUDINI_INSTALL_ROOT}")
