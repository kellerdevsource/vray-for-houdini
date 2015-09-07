#
# Copyright (c) 2015, Chaos Software Ltd
#
# V-Ray For Houdini
#
# Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
#
# All rights reserved. These coded instructions, statements and
# computer programs contain unpublished information proprietary to
# Chaos Group Ltd, which is protected by the appropriate copyright
# laws and may not be disclosed to third parties or copied or
# duplicated, in whole or in part, without prior written consent of
# Chaos Group Ltd.
#

# Made using: cat sesitag.txt | sesitag -m
set(PLUGIN_TAGINFO "\"3262197cbf104d152da5089a671b9ff8394bdcd9d530d8aa27f5984e1714bfd251aa2487851869344346dba5159b681c2da1a710878dac641a5874f82bead6fb0cb006e8bedd1ad3f169d85849f95eb181\"")

include(CheckIncludeFile)

include(vfh_compiler)
include(vfh_macro)
include(vfh_houdini)
include(vfh_vrayappsdk)
include(vfh_vraysdk)

add_definitions(-DVRAY_FOR_HOUDINI)

include_directories(.)
include_directories(ui)
include_directories(op)
include_directories(sop)
include_directories(vop)
include_directories(vop/brdf)
include_directories(vop/material)
include_directories(vop/texture)
include_directories(vop/uvwgen)
include_directories(vop/meta)
include_directories(export)
include_directories(export/utils)
include_directories(utils)

file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")

# *.aur preview
CHECK_INCLUDE_FILE(FluidInterface.h HAS_PHOENIX)
if (HAS_PHOENIX)
	add_definitions(-DCGR_HAS_AUR)
endif()

# *.vrscene preview
CHECK_INCLUDE_FILE(vrayscene_preview_mesh.h HAS_VRSCENE)
if (HAS_VRSCENE)
	add_definitions(-DCGR_HAS_VRAYSCENE)
endif()

use_houdini_sdk()
use_vray_sdk()
use_vray_appsdk()

houdini_plugin(${PROJECT_NAME} "${SOURCES};${HEADERS}")

link_with_vray_sdk(${PROJECT_NAME})
link_with_vray_appsdk(${PROJECT_NAME})
link_with_boost(${PROJECT_NAME})

# Install shelf and icon locally
#
install(FILES       ${CMAKE_SOURCE_DIR}/deploy/vfh.shelf
		DESTINATION ${HOUDINI_HOME_PATH}/toolbar)

install(FILES       ${CMAKE_SOURCE_DIR}/deploy/ROP_vray.svg
		DESTINATION ${HOUDINI_HOME_PATH}/config/Icons)

# Install py modules locally
#
install(DIRECTORY   ${CMAKE_SOURCE_DIR}/deploy/scripts/python/vfh
		DESTINATION ${HOUDINI_HOME_PATH}/scripts/python)


# Create program launcher with all needed environment variables
#
option(USE_LAUNCHER "Create launcher" ON)
if(USE_LAUNCHER)
	if(WIN32)
		set(HFS_RUN_FILE_IN  "${CMAKE_SOURCE_DIR}/misc/hfs.bat.in")
		set(HFS_RUN_FILE_OUT "${CMAKE_BINARY_DIR}/misc/hfs.bat")

		configure_file(${HFS_RUN_FILE_IN} ${HFS_RUN_FILE_OUT} @ONLY)

		message(STATUS "Generating Houdini run file (${HFS_RUN_FILE_DST})")
		install(FILES       ${HFS_RUN_FILE_OUT}
				DESTINATION "$ENV{USERPROFILE}/Desktop")
	else()
		set(HFS_ENV_FILE_DST "$ENV{HOME}/bin/hfs.env")
		set(HFS_RUN_FILE_DST "$ENV{HOME}/bin/hfs")

		if(APPLE)
			set(HFS_ENV_FILE_IN "${CMAKE_SOURCE_DIR}/misc/hfs_osx.env.in")
		else()
			set(HFS_ENV_FILE_IN "${CMAKE_SOURCE_DIR}/misc/hfs_linux.env.in")
		endif()
		set(HFS_RUN_FILE_IN "${CMAKE_SOURCE_DIR}/misc/hfs.sh.in")

		set(HFS_ENV_FILE_OUT "${CMAKE_BINARY_DIR}/misc/hfs.env")
		set(HFS_RUN_FILE_OUT "${CMAKE_BINARY_DIR}/misc/hfs")

		configure_file(${HFS_ENV_FILE_IN} ${HFS_ENV_FILE_OUT} @ONLY)
		configure_file(${HFS_RUN_FILE_IN} ${HFS_RUN_FILE_OUT} @ONLY)

		message(STATUS "Generating Houdini run file (${HFS_RUN_FILE_DST})")
		install(FILES       ${HFS_ENV_FILE_OUT}
				DESTINATION "$ENV{HOME}/bin")
		install(FILES       ${HFS_RUN_FILE_OUT}
				DESTINATION "$ENV{HOME}/bin"
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
	endif()
endif()


# Install to local share
#
option(INSTALL_RELEASE "Install release build for distribution" OFF)
if(INSTALL_RELEASE)
	set(INSTALL_RELEASE_PATH "" CACHE PATH "Release install path")

	if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Release")
		message(WARNING "INSTALL_RELEASE is ON, but CMAKE_BUILD_TYPE is not \"Release\" (${CMAKE_BUILD_TYPE})!")
	endif()

	if("${INSTALL_RELEASE_PATH}" STREQUAL "")
		message(FATAL_ERROR "INSTALL_RELEASE is ON, but INSTALL_RELEASE_PATH is not set!")
	else()

		# Copy plugin
		cgr_install_runtime(${PROJECT_NAME} ${INSTALL_RELEASE_PATH}/dso)

		# Copy UI & scripts
		install(FILES       ${CMAKE_SOURCE_DIR}/deploy/vfh.shelf
				DESTINATION ${INSTALL_RELEASE_PATH}/deploy)

		install(FILES       ${CMAKE_SOURCE_DIR}/deploy/ROP_vray.svg
				DESTINATION ${INSTALL_RELEASE_PATH}/deploy)

		install(DIRECTORY   ${CMAKE_SOURCE_DIR}/deploy/scripts/python/vfh
				DESTINATION ${INSTALL_RELEASE_PATH}/deploy/scripts/python)

		# Generate & copy startup file
		if(WIN32)
			configure_file(${CMAKE_SOURCE_DIR}/deploy/hfs.bat.in
						   ${CMAKE_BINARY_DIR}/deploy/hfs.bat
						   @ONLY)

			install(FILES       ${CMAKE_BINARY_DIR}/deploy/hfs.bat
					DESTINATION ${INSTALL_RELEASE_PATH}
					RENAME      "hfs${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}.bat")

		elseif(APPLE)
			configure_file(${CMAKE_SOURCE_DIR}/deploy/hfs_osx.sh.in
						   ${CMAKE_BINARY_DIR}/deploy/hfs_osx.sh
						   @ONLY)

			install(FILES       ${CMAKE_BINARY_DIR}/deploy/hfs_osx.sh
					DESTINATION ${INSTALL_RELEASE_PATH}
					PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE
					RENAME      "hfs${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}.sh")

		else()
			configure_file(${CMAKE_SOURCE_DIR}/deploy/hfs_linux.sh.in
						   ${CMAKE_BINARY_DIR}/deploy/hfs_linux.sh
						   @ONLY)

			install(FILES       ${CMAKE_BINARY_DIR}/deploy/hfs_linux.sh
					DESTINATION ${INSTALL_RELEASE_PATH}
					PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE
					RENAME      "hfs${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}.sh")
		endif()

		# Copy AppSDK
		file(GLOB APPSDK_BIN         "${APPSDK_ROOT}/bin/*.*")
		file(GLOB APPSDK_VRAYPLUGINS "${APPSDK_ROOT}/bin/plugins/*.*")
		file(GLOB APPSDK_VRLSERVICE  "${APPSDK_ROOT}/bin/vrlservice/*.*")

		file(INSTALL    ${APPSDK_BIN}
			DESTINATION ${INSTALL_RELEASE_PATH}/appsdk/bin
			USE_SOURCE_PERMISSIONS
		)

		file(INSTALL    ${APPSDK_VRAYPLUGINS}
			DESTINATION ${INSTALL_RELEASE_PATH}/appsdk/bin/plugins
			USE_SOURCE_PERMISSIONS
		)

		file(INSTALL    ${APPSDK_VRLSERVICE}
			DESTINATION ${INSTALL_RELEASE_PATH}/appsdk/bin/vrlservice
			USE_SOURCE_PERMISSIONS
		)

		# JSON plugin descriptions
		install(DIRECTORY   ${CMAKE_SOURCE_DIR}/deploy/plugins_desc
				DESTINATION ${INSTALL_RELEASE_PATH}
				PATTERN ".git" EXCLUDE
		)
	endif()
endif()
