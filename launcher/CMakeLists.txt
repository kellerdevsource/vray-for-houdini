#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

project(vhf_launcher)

vfh_generate_launcher(TEMPLATE_FILENAME "string_defines.h.in"
					  FILENAME "string_defines.h"
					  TEMPLATE_DIR ${CMAKE_SOURCE_DIR}/launcher
					  COPY_INSTEAD)

set(SOURCES
	launcher.cpp
)

set(HEADERS
	${CMAKE_BINARY_DIR}/string_defines.h
)

include_directories(${CMAKE_BINARY_DIR})

add_executable(${PROJECT_NAME} EXCLUDE_FROM_ALL "${SOURCES};${HEADERS}")

# This is to make sure other projects are built when 'vfh_launcher' is startup project
add_dependencies(${PROJECT_NAME} vray_for_houdini INSTALL)

