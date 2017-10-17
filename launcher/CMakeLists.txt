#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

project(vfh_launcher)

vfh_generate_launcher(TEMPLATE_FILENAME "string_defines.h.in"
					  FILENAME "string_defines.h"
					  TEMPLATE_DIR ${CMAKE_SOURCE_DIR}/launcher
					  CPP_LAUNCHER)

set(SOURCES
	launcher.cpp
)

set(HEADERS
	${CMAKE_BINARY_DIR}/string_defines.h
)

include_directories(${CMAKE_BINARY_DIR})

# This project must be excluded from all since INSTALL depends on ALL and it will create circular dependency
add_executable(${PROJECT_NAME} EXCLUDE_FROM_ALL "${SOURCES};${HEADERS}")

# This is to make sure other projects are built when 'vfh_launcher' is startup project
add_dependencies(${PROJECT_NAME} vray_for_houdini)

if(WITH_SOHO)
	add_dependencies(${PROJECT_NAME} vray_for_houdini_ipr vray_for_houdini_python_ipr)
endif()

if(MSVC AND NOT ${CMAKE_GENERATOR} STREQUAL "Ninja")
	# Only Visual Studio generator produces INSTALL target
	add_dependencies(${PROJECT_NAME} INSTALL)
endif()
