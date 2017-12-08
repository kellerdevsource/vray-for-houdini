#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

project(vray_for_houdini_python_ipr)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/ipr)
include_directories(${CMAKE_SOURCE_DIR}/src/export)
include_directories(${CMAKE_SOURCE_DIR}/src/export/utils)
include_directories(${CMAKE_SOURCE_DIR}/src/export/export)
include_directories(${CMAKE_SOURCE_DIR}/src/lib)
include_directories(${CMAKE_SOURCE_DIR}/src/utils)

set(SOURCES
	vfh_ipr.cpp
	vfh_ipr_viewer.cpp
	vfh_ipr_imdisplay_viewer.cpp
)

set(HEADERS
	vfh_ipr_viewer.h
	vfh_ipr_imdisplay_viewer.h
)

list(APPEND SOURCES moc_vfh_ipr_imdisplay_viewer.cpp)
houdini_plugin(${PROJECT_NAME} "${SOURCES};${HEADERS}")
vfh_make_moc(TARGET ${PROJECT_NAME}
	FILE_IN       ${CMAKE_CURRENT_SOURCE_DIR}/vfh_ipr_imdisplay_viewer.h
	FILE_OUT_NAME moc_vfh_ipr_imdisplay_viewer.cpp
)

target_link_libraries(${PROJECT_NAME}
	vray_for_houdini
	vray_for_houdini_exporter
	Qt5Network
)

link_with_vray_sdk(${PROJECT_NAME})
link_with_vray(${PROJECT_NAME})
link_with_vray_appsdk(${PROJECT_NAME})
link_with_boost(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME _vfh_ipr)
if (WIN32)
	set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pyd")
elseif(APPLE)
	target_link_libraries(${PROJECT_NAME} python2.7)
endif()

cgr_install_runtime(${PROJECT_NAME} ${VFH_PYTHON_DIRPATH})
if (INSTALL_RELEASE)
	cgr_install_runtime(${PROJECT_NAME} ${INSTALL_RELEASE_PATH}/vfh_home/soho/python2.7)
endif()
