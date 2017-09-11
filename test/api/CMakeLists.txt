#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

project(appsdk_cpp)

find_package(Qt5Widgets)

include(vfh_compiler)
include(vfh_vrayappsdk)

file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")

use_vray_appsdk()
use_houdini_sdk()

add_executable(${PROJECT_NAME} "${SOURCES};${HEADERS}")

link_with_vray_appsdk(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} Qt5::Widgets)
target_link_libraries(${PROJECT_NAME} ${HDK_LIBS})
