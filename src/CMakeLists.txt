#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

set(SDK_PATH "" CACHE STRING "SDK root qpath")


include(CheckIncludeFile)

include(vfh_compiler)
include(vfh_macro)
include(vfh_houdini)
include(vfh_vrayappsdk)
include(vfh_vraysdk)
include(vfh_phoenix)

add_definitions(-DVRAY_FOR_HOUDINI)
add_definitions(-DVRAYAPPSDK_VFB_HANDLE)

include_directories(.)
include_directories(io)
include_directories(ui)
include_directories(op)
include_directories(sop)
include_directories(vop)
include_directories(rop)
include_directories(vop/brdf)
include_directories(vop/material)
include_directories(vop/texture)
include_directories(vop/uvwgen)
include_directories(vop/meta)
include_directories(export)
include_directories(export/utils)
include_directories(export/export)
include_directories(utils)

file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")

if (WIN32)
	macro(GroupSources curdir)
		file(GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${curdir} ${PROJECT_SOURCE_DIR}/${curdir}/*)
		foreach(child ${children})
			if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${curdir}/${child})
				GroupSources(${curdir}/${child})
			else()
				string(REPLACE "/" "\\" groupname ${curdir})
				source_group(${groupname} FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
			endif()
		endforeach()
	endmacro()
	GroupSources(src)
endif()

# *.aur preview
if (Phoenix_FOUND)
	# this will enable compilation for the code that integrates phx
	add_definitions(-DCGR_HAS_AUR)
	include_directories(${Phoenix_INCLUDES})
	link_directories(${Phoenix_LIBRARIES})
endif()

use_houdini_sdk()
use_vray_sdk()
use_vray_appsdk()

houdini_plugin(${PROJECT_NAME} "${SOURCES};${HEADERS}")

link_with_vray_sdk(${PROJECT_NAME})
link_with_vray(${PROJECT_NAME})
link_with_vray_appsdk(${PROJECT_NAME})
link_with_boost(${PROJECT_NAME})


if (Phoenix_FOUND)
	target_link_libraries(${PROJECT_NAME}
		aurloader_s
		aurloader
		guiwin_qt_s
		iutils_s
		aurramps_s
	)
endif()

set(CGR_SRC_HASH "" CACHE STRING "Sources hash (for Teamcity)")

set(SRC_GIT_HASH ${CGR_SRC_HASH})
if("${CGR_SRC_HASH}" STREQUAL "")
	cgr_get_git_hash(${CMAKE_SOURCE_DIR} SRC_GIT_HASH)
endif()

message(STATUS "Source revision: ${SRC_GIT_HASH}")
add_definitions(-DCGR_GIT_HASH=${SRC_GIT_HASH})


# Install
#
option(INSTALL_LOCAL "Install files locally" ON)
option(INSTALL_RELEASE "Install release build for distribution" OFF)


function(vfh_gen_launcher _location _vfh_appsdk_path _vfh_config_path _vfh_deploy_path _launcher)
	file(TO_NATIVE_PATH ${_vfh_appsdk_path} _vfh_appsdk_path)
	file(TO_NATIVE_PATH ${_vfh_config_path} _vfh_config_path)
	file(TO_NATIVE_PATH ${_vfh_deploy_path} _vfh_deploy_path)

	if(WIN32)
		set(_mylauncher hfs_launcher.bat)
		configure_file(${CMAKE_SOURCE_DIR}/deploy/hfs.bat.in
					   ${CMAKE_BINARY_DIR}/tmp/${_mylauncher}
					   @ONLY)
	elseif(APPLE)
		set(_mylauncher hfs_launcher.sh)
		configure_file(${CMAKE_SOURCE_DIR}/deploy/hfs_osx.sh.in
					   ${CMAKE_BINARY_DIR}/tmp/${_mylauncher}
					   @ONLY)
	else()
		set(_mylauncher hfs_launcher.sh)
		configure_file(${CMAKE_SOURCE_DIR}/deploy/hfs_linux.sh.in
					   ${CMAKE_BINARY_DIR}/tmp/${_mylauncher}
					   @ONLY)
	endif()

	file(INSTALL ${CMAKE_BINARY_DIR}/tmp/${_mylauncher}
		 DESTINATION ${_location}
		 FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		)

	set(${_launcher} ${_location}/${_mylauncher} PARENT_SCOPE)
	file(REMOVE ${CMAKE_BINARY_DIR}/tmp/${_mylauncher})
endfunction(vfh_gen_launcher)


function(vfh_install_common _location)
	# Copy Phoenix cache file loader library
	if (Phoenix_FOUND)
		install(FILES       ${Phoenix_LIBRARIES}/${CGR_PHOENIX_SHARED}
							${Phoenix_LIBRARIES}/${CGR_PHOENIX_SHARED_VDB}
							${Phoenix_LIBRARIES}/${CGR_PHOENIX_SHARED_F3D}
				DESTINATION ${_location}/vfh_home/vfh_lib
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
	endif()

	# Copy plugin
	cgr_install_runtime(${PROJECT_NAME} ${_location}/vfh_home/dso)

	# Copy config
	install(DIRECTORY   ${CMAKE_SOURCE_DIR}/deploy/vfh_home
			DESTINATION ${_location})

endfunction(vfh_install_common)


if(INSTALL_LOCAL)
	set(DEPLOY_LOCATION ${CMAKE_BINARY_DIR}/deploy)

	vfh_install_common(${DEPLOY_LOCATION})

	# Generate launcher file
	vfh_gen_launcher(${DEPLOY_LOCATION}           # where
					 ${APPSDK_ROOT}               # app sdk location
					 ${CMAKE_SOURCE_DIR}/deploy   # vfh config location
					 ${DEPLOY_LOCATION}           # vfh dso location
					 _launcher                    # output launcher
					)

	# Create shortcuts
	file(TO_NATIVE_PATH "${_launcher}" SHORTCUT_TARGET)
	if(WIN32)
		file(WRITE "$ENV{USERPROFILE}/Desktop/hfs.bat" ${SHORTCUT_TARGET})
	else()
		file(WRITE "$ENV{HOME}/bin/hfs.sh" ${SHORTCUT_TARGET})
	endif()
endif()


if(INSTALL_RELEASE)

	if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Release")
		message(WARNING "INSTALL_RELEASE is ON, but CMAKE_BUILD_TYPE is not \"Release\" (${CMAKE_BUILD_TYPE})!")
	endif()

	set(INSTALL_RELEASE_ROOT "" CACHE PATH "Release install path")
	set(INSTALL_RELEASE_SUBDIR "vfh-${SRC_GIT_HASH}${INSTALL_RELEASE_SUFFIX}")

	if("${INSTALL_RELEASE_ROOT}" STREQUAL "")
		message(FATAL_ERROR "INSTALL_RELEASE is ON, but INSTALL_RELEASE_ROOT is not set!")
	endif()

	# Set install path
	set(INSTALL_RELEASE_PATH ${INSTALL_RELEASE_ROOT}/${INSTALL_RELEASE_SUBDIR})

	vfh_install_common(${INSTALL_RELEASE_PATH})

	# Generate launcher file
	if(WIN32)
		vfh_gen_launcher(${INSTALL_RELEASE_PATH}          # where
						 "%LAUNCHER_LOCATION%/appsdk"     # app sdk location
						 "%LAUNCHER_LOCATION%"            # vfh config location
						 "%LAUNCHER_LOCATION%"            # vfh dso location
						 _launcher                        # output launcher
						)
	else()
		vfh_gen_launcher(${INSTALL_RELEASE_PATH}          # where
						 "\${LAUNCHER_LOCATION}/appsdk"   # app sdk location
						 "\${LAUNCHER_LOCATION}"          # vfh config location
						 "\${LAUNCHER_LOCATION}"          # vfh dso location
						 _launcher                        # output launcher
						)
	endif()
	get_filename_component(_launcher_ext ${_launcher} EXT)
	file(RENAME ${_launcher} ${INSTALL_RELEASE_PATH}/hfs${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}${_launcher_ext})

	# Copy ui
	install(DIRECTORY   ${CMAKE_SOURCE_DIR}/deploy/ui
			DESTINATION ${INSTALL_RELEASE_PATH})

	# Copy AppSDK
	install(DIRECTORY   ${APPSDK_ROOT}/bin
			DESTINATION ${INSTALL_RELEASE_PATH}/appsdk)

	# Clone JSON plugin descriptions
	set(VFH_CLONE_PLUGINS_DESC ${CMAKE_BINARY_DIR}/vfh_clone_plugins_desc.cmake)
	configure_file(${CMAKE_SOURCE_DIR}/cmake/vfh_clone_plugins_desc.cmake.in ${VFH_CLONE_PLUGINS_DESC})
	install(SCRIPT ${VFH_CLONE_PLUGINS_DESC})

	# Archive the installation directory
	set(VFH_PACK ${CMAKE_BINARY_DIR}/vfh_pack.cmake)
	configure_file(${CMAKE_SOURCE_DIR}/cmake/vfh_pack.cmake.in ${VFH_PACK})
	install(SCRIPT ${VFH_PACK})
endif()
