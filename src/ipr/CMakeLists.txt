#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

project(vray_for_houdini_ipr)

add_executable(${PROJECT_NAME} main.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME vfh_ipr)

cgr_install_runtime(${PROJECT_NAME} ${VFH_BIN_DIRPATH})
if (INSTALL_RELEASE)
	cgr_install_runtime(${PROJECT_NAME} ${INSTALL_RELEASE_PATH}/vfh_home/bin)
endif()
