#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

project(vray_for_houdini_ipr)

set(SOURCES
	main.cpp
	${CMAKE_SOURCE_DIR}/src/utils/vfh_log.cpp
)

set(HEADERS
	${CMAKE_SOURCE_DIR}/src/utils/vfh_log.h
)

use_vray_sdk()

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/export)
include_directories(${CMAKE_SOURCE_DIR}/src/utils)

add_executable(${PROJECT_NAME} "${SOURCES};${HEADERS}")

link_with_vray_sdk(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME vfh_ipr)

cgr_install_runtime(${PROJECT_NAME} ${VFH_BIN_DIRPATH})
if (INSTALL_RELEASE)
	cgr_install_runtime(${PROJECT_NAME} ${INSTALL_RELEASE_PATH}/vfh_home/bin)
endif()

target_link_libraries(${PROJECT_NAME} Qt5Network Qt5Core Qt5Gui Qt5Widgets)
