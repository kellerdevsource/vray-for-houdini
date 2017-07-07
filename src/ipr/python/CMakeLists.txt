#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

project(vray_for_houdini_python_ipr)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/export)
include_directories(${CMAKE_SOURCE_DIR}/src/export/utils)
include_directories(${CMAKE_SOURCE_DIR}/src/export/export)
include_directories(${CMAKE_SOURCE_DIR}/src/utils)

set(SOURCES
	vfh_ipr.cpp
)

set(HEADERS
)

use_houdini_sdk()
use_vray_sdk()
use_vray_appsdk()

houdini_plugin(${PROJECT_NAME} "${SOURCES};${HEADERS}")

target_link_libraries(${PROJECT_NAME} vray_for_houdini_exporter)

link_with_vray_sdk(${PROJECT_NAME})
link_with_vray(${PROJECT_NAME})
link_with_vray_appsdk(${PROJECT_NAME})
link_with_boost(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME _vfh_ipr)
if (WIN32)
	set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pyd")
endif()

cgr_install_runtime(${PROJECT_NAME} ${VFH_PYTHON_DIRPATH})
