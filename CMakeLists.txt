#
# Copyright (c) 2015, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_CURRENT_SOURCE_DIR}/cmake/vfh_find")

project(vray_for_houdini)

include(CTest)

option(WITH_VRAY_FOR_HOUDINI "Build V-Ray For Houdini plugin" ON)
if(WITH_VRAY_FOR_HOUDINI)
	add_subdirectory(src)
endif()

option(WITH_TESTS "Run tests" OFF)
if(WITH_TESTS)
	add_subdirectory(test)
endif()

option(WITH_TEST_APP "Build AppSDK test applications" OFF)
if(WITH_TEST_APP)
	add_subdirectory(test/api)
endif()
