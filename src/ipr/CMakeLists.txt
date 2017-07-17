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
	vfh_ipr_server.cpp
)

set(HEADERS
	vfh_ipr_checker_types.h
	vfh_ipr_server.h
)


list(APPEND SOURCES moc_vfh_ipr_server.cpp)
add_executable(${PROJECT_NAME} "${SOURCES};${HEADERS}")
vfh_make_moc(TARGET ${PROJECT_NAME}
	FILE_IN       ${CMAKE_CURRENT_SOURCE_DIR}/vfh_ipr_server.h
	FILE_OUT_NAME moc_vfh_ipr_server.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME vfh_ipr)

cgr_install_runtime(${PROJECT_NAME} ${VFH_BIN_DIRPATH})

target_link_libraries(${PROJECT_NAME} Qt5Network Qt5Core Qt5Gui Qt5Widgets)
