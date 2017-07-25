#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

cmake_minimum_required(VERSION 3.7.0)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_CURRENT_SOURCE_DIR}/cmake/vfh_find")

option(WITH_VRAY_FOR_HOUDINI "Build V-Ray For Houdini plugin"             ON)
option(WITH_SOHO             "Build V-Ray For Houdini SOHO Python module" ON)

option(BUILD_TESTING         "Enable build testing"           OFF)
option(WITH_TESTS            "Run tests"                      OFF)
option(WITH_TEST_APP         "Build AppSDK test applications" OFF)

include(vfh_compiler)
include(vfh_macro)
include(vfh_houdini)
include(vfh_vrayappsdk)
include(vfh_vraysdk)
include(vfh_phoenix)

project(vray_for_houdini)

set_compiler_flags()

use_houdini_sdk()
use_vray_sdk()
use_vray_appsdk()

if(BUILD_TESTING)
	include(CTest)
endif()

if(WITH_VRAY_FOR_HOUDINI)
	if(WITH_SOHO)
		add_subdirectory(src/ipr/python)
		add_subdirectory(src/ipr)
	endif()
	add_subdirectory(src)
endif()

if(WITH_TESTS)
	add_subdirectory(test)
endif()

if(WITH_TEST_APP)
	add_subdirectory(test/api)
endif()

set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD ON)
