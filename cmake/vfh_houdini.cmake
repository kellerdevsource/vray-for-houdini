#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

include(CheckIncludeFile)

set(HOUDINI_VERSION       "14.0" CACHE STRING "Houdini major version")
set(HOUDINI_VERSION_BUILD "248"  CACHE STRING "Houdini build version")
set(HOUDINI_INSTALL_ROOT  ""     CACHE PATH   "Houdini install path")

set(HOUDINI_HDK_PATH "")
if(SDK_PATH)
	set(HOUDINI_HDK_PATH "${SDK_PATH}/hdk/hdk${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}")
endif()

if (APPLE)
	if (HOUDINI_INSTALL_ROOT STREQUAL "")
		set(HOUDINI_INSTALL_ROOT "/Applications/Houdini ${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}" CACHE PATH "" FORCE)
	endif()

	set(HOUDINI_FRAMEWORK_ROOT "/Library/Frameworks/Houdini.framework/Versions/${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}")
	if(HOUDINI_HDK_PATH STREQUAL "")
		set(HOUDINI_HDK_PATH ${HOUDINI_FRAMEWORK_ROOT})
	endif()

	set(HOUDINI_INCLUDE_PATH "${HOUDINI_HDK_PATH}/Resources/toolkit/include")
	set(HOUDINI_LIB_PATH     "${HOUDINI_HDK_PATH}/Libraries")

	set(HOUDINI_BIN_PATH "${HOUDINI_INSTALL_ROOT}/Houdini FX.app/Contents/MacOS")
	set(HOUDINI_HOME_PATH "$ENV{HOME}/Library/Preferences/houdini/${HOUDINI_VERSION}")

elseif(WIN32)
	if (HOUDINI_INSTALL_ROOT STREQUAL "")
		set(HOUDINI_INSTALL_ROOT "C:/Program Files/Side Effects Software/Houdini ${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}" CACHE PATH "" FORCE)
	endif()

	if(HOUDINI_HDK_PATH STREQUAL "")
		set(HOUDINI_HDK_PATH ${HOUDINI_INSTALL_ROOT})
	endif()

	set(USER_HOME "$ENV{HOME}")
	if(USER_HOME STREQUAL "")
		set(USER_HOME "$ENV{USERPROFILE}/Documents")
	endif()
	file(TO_CMAKE_PATH "${USER_HOME}" USER_HOME)

	set(HOUDINI_INCLUDE_PATH "${HOUDINI_HDK_PATH}/toolkit/include")
	set(HOUDINI_LIB_PATH     "${HOUDINI_HDK_PATH}/custom/houdini/dsolib")

	set(HOUDINI_BIN_PATH  "${HOUDINI_INSTALL_ROOT}/bin")
	set(HOUDINI_HOME_PATH "${USER_HOME}/houdini${HOUDINI_VERSION}")

else()
	if (HOUDINI_INSTALL_ROOT STREQUAL "")
		set(HOUDINI_INSTALL_ROOT "/opt/hfs${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}" CACHE PATH "" FORCE)
	endif()

	if(HOUDINI_HDK_PATH STREQUAL "")
		set(HOUDINI_HDK_PATH ${HOUDINI_INSTALL_ROOT})
	endif()

	set(HOUDINI_INCLUDE_PATH "${HOUDINI_HDK_PATH}/toolkit/include")
	set(HOUDINI_LIB_PATH     "${HOUDINI_HDK_PATH}/dsolib")

	set(HOUDINI_BIN_PATH "${HOUDINI_INSTALL_ROOT}/bin")
	set(HOUDINI_HOME_PATH "$ENV{HOME}/houdini${HOUDINI_VERSION}")

endif()


# NOTE: exact list of compiler/linker arguments when building for Houdini can be queried with:
# "hcustom --cflags/ldflags" - Display compiler/linker flags
# common Houdini compiler flags
set(HOUDINI_DEFINES
	-DAMD64
	-DSIZEOF_VOID_P=8
	-DSESI_LITTLE_ENDIAN
	-DFBX_ENABLED=1
	-DOPENCL_ENABLED=1
	-DOPENVDB_ENABLED=1
	-DMAKING_DSO
	-DVERSION=${HOUDINI_VERSION}
	-DHOUDINI_DSO_VERSION=${HOUDINI_VERSION}
	-DUT_DSO_TAGINFO=${PLUGIN_TAGINFO}
)
set(HOUDINI_LIBS "")

if(WIN32)
	# Houdini compiler flags for Windows
	list(APPEND HOUDINI_DEFINES
		-DI386
		-DWIN32
		-DSWAP_BITFIELDS
		-D_WIN32_WINNT=0x0502
		-DWINVER=0x0502
		-DNOMINMAX
		-DSTRICT
		-DWIN32_LEAN_AND_MEAN
		-D_USE_MATH_DEFINES
		-D_CRT_SECURE_NO_DEPRECATE
		-D_CRT_NONSTDC_NO_DEPRECATE
		-D_SCL_SECURE_NO_WARNINGS
		-DBOOST_ALL_NO_LIB
		-DEIGEN_MALLOC_ALREADY_ALIGNED=0
		-DOPENVDB_3_ABI_COMPATIBLE
	)

	file(GLOB HOUDINI_LIBS_A "${HOUDINI_LIB_PATH}/*.a")
	file(GLOB HOUDINI_LIBS_LIB "${HOUDINI_LIB_PATH}/*.lib")

	list(APPEND HOUDINI_LIBS
		${HOUDINI_LIBS_A}
		${HOUDINI_LIBS_LIB}
	)

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
		shell32
		user32
		uuid
		winspool
		ws2_32
	)

	list(APPEND HOUDINI_LIBS ${SYSTEM_LIBS})

else()
	list(APPEND HOUDINI_DEFINES
		-D_GNU_SOURCE
		-DENABLE_THREADS
		-DENABLE_UI_THREADS
		-DUSE_PTHREADS
		-DGCC3
		-DGCC4
		-D_REENTRANT
		-D_FILE_OFFSET_BITS=64
	)

	set(HOUDINI_LIBS
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
		# Houdini compiler flags for Apple
		list(APPEND HOUDINI_DEFINES
			-DNEED_SPECIALIZATION_STORAGE
			-DMBSD
			-DMBSD_COCOA
			-DMBSD_INTEL
		)

		list(APPEND HOUDINI_LIBS
			z
			dl
			tbb
			tbbmalloc
			pthread
			QtCore
			QtGui
			"-framework Cocoa"
		)

	else()
		# Houdini compiler flags for Linux
		list(APPEND HOUDINI_DEFINES
			-DLINUX
		)

		list(APPEND HOUDINI_LIBS
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
	find_library(HDK_LIB_GEO
		NAMES openvdb_sesi HoudiniGEO
		PATHS ${HOUDINI_LIB_PATH}
	)

	message(STATUS "Using Houdini ${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}: ${HOUDINI_INSTALL_ROOT}")
	message(STATUS "Using HDK include path: ${HOUDINI_INCLUDE_PATH}")
	message(STATUS "Using HDK library path: ${HOUDINI_LIB_PATH}")

	if(NOT HDK_LIB_GEO)
		message(FATAL_ERROR "Houdini SDK is not found! Check HOUDINI_VERSION / HOUDINI_VERSION_BUILD variables!")
	endif()

	if(WIN32)
		find_library(HDK_LIB_HALF
			NAMES Half
			PATHS ${HOUDINI_LIB_PATH}
		)
		if(NOT HDK_LIB_HALF)
			message(FATAL_ERROR "Houdini SDK missing Half.lib - it is requiered on win32!")
		endif()
		list(APPEND HDK_LIB_GEO ${HDK_LIB_HALF})
	endif()

	add_definitions(${HOUDINI_DEFINES})
	include_directories(${HOUDINI_INCLUDE_PATH})
	link_directories(${HOUDINI_LIB_PATH})
endmacro()


macro(houdini_plugin name sources)
	set(libraryName ${name})
	add_library(${libraryName} SHARED ${sources})
	set_target_properties(${libraryName} PROPERTIES PREFIX "")
	target_link_libraries(${libraryName} ${HOUDINI_LIBS})
	vfh_osx_flags(${libraryName})
endmacro()
