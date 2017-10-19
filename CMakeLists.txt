#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

cmake_minimum_required(VERSION 3.7.0)

list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_SOURCE_DIR}/cmake
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/vfh_find
)

include(vfh_macro)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(WITH_VRAY_FOR_HOUDINI "Build V-Ray For Houdini plugin"             ON)
option(WITH_SOHO             "Build V-Ray For Houdini SOHO Python module" ON)

option(BUILD_TESTING         "Enable build testing"           OFF)
option(WITH_TESTS            "Run tests"                      OFF)
option(WITH_TEST_APP         "Build AppSDK test applications" OFF)

option(WITH_LAUNCHER         "Build development launcher"     OFF)

option(INSTALL_RELEASE "Install release build for distribution" OFF)
if(INSTALL_RELEASE AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND "${CMAKE_GENERATOR}" STREQUAL "Ninja")
	message(WARNING "INSTALL_RELEASE is ON, but CMAKE_BUILD_TYPE is not \"Release\" (${CMAKE_BUILD_TYPE})!")
endif()

set(CGR_SRC_HASH "" CACHE STRING "Sources hash")

set(SRC_GIT_HASH ${CGR_SRC_HASH})
if("${CGR_SRC_HASH}" STREQUAL "")
	cgr_get_git_hash(${CMAKE_SOURCE_DIR} SRC_GIT_HASH)
endif()

message(STATUS "Source revision: ${SRC_GIT_HASH}")

set(INSTALL_RELEASE_ROOT "" CACHE PATH "Release install path")
set(INSTALL_RELEASE_SUBDIR "vfh-${SRC_GIT_HASH}${INSTALL_RELEASE_SUFFIX}")

if(INSTALL_RELEASE AND "${INSTALL_RELEASE_ROOT}" STREQUAL "")
	message(FATAL_ERROR "INSTALL_RELEASE is ON, but INSTALL_RELEASE_ROOT is not set!")
endif()

# Set install path
set(INSTALL_RELEASE_PATH ${INSTALL_RELEASE_ROOT}/${INSTALL_RELEASE_SUBDIR})

include(vfh_compiler)

project(vray_for_houdini)

include(vfh_houdini)
include(vfh_vrayappsdk)
include(vfh_vraysdk)
include(vfh_phoenix)

use_houdini_sdk()
use_vray_sdk()
use_vray_appsdk()
use_phx_sdk()

set_compiler_flags()

if(BUILD_TESTING)
	include(CTest)
endif()

if(WITH_VRAY_FOR_HOUDINI)
	if(WITH_SOHO)
		add_subdirectory(src/ipr/python)
		add_subdirectory(src/ipr)
	endif()
	add_subdirectory(src)
	if(WITH_LAUNCHER)
		add_subdirectory(launcher)
		set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "vfh_launcher")
	endif()
endif()

if(WITH_TESTS)
	add_subdirectory(test)
endif()

if(WITH_TEST_APP)
	add_subdirectory(test/api)
endif()

set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD ON)
