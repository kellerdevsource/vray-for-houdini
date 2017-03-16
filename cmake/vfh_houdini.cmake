#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

include(CheckIncludeFile)

set(HOUDINI_VERSION       "14.0" CACHE STRING "Houdini major version")
set(HOUDINI_VERSION_BUILD "248"  CACHE STRING "Houdini build version")
set(HOUDINI_INSTALL_ROOT  ""     CACHE PATH   "Houdini install path")

# Made using: cat sesitag.txt | sesitag -m
set(PLUGIN_TAGINFO "\"3262197cbf104d152da5089a671b9ff8394bdcd9d530d8aa27f5984e1714bfd251aa2487851869344346dba5159b681c2da1a710878dac641a5874f82bead6fb0cb006e8bedd1ad3f169d85849f95eb181\"")


macro(use_houdini_sdk)
	# Adjust install path
	if((NOT HOUDINI_INSTALL_ROOT) OR (NOT EXISTS ${HOUDINI_INSTALL_ROOT}))
		# no valid install path set, fall back to default install location
		if (APPLE)
			set(HOUDINI_INSTALL_ROOT "/Applications/Houdini ${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}" CACHE PATH "" FORCE)

		elseif(WIN32)
			set(HOUDINI_INSTALL_ROOT "C:/Program Files/Side Effects Software/Houdini ${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}" CACHE PATH "" FORCE)

		else()
			set(HOUDINI_INSTALL_ROOT "/opt/hfs${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}" CACHE PATH "" FORCE)

		endif()

	endif()

	find_package(HDK ${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD} REQUIRED)

	message(STATUS "Using Houdini ${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}: ${HOUDINI_INSTALL_ROOT}")
	message(STATUS "Using HDK include path: ${HDK_INCLUDES}")
	message(STATUS "Using HDK library path: ${HDK_LIBRARIES}")

	# Set bin and home path
	if (APPLE)
		#TODO : need to check those
		set(HOUDINI_BIN_PATH "${HOUDINI_INSTALL_ROOT}/Houdini FX.app/Contents/MacOS")
		set(HOUDINI_HOME_PATH "$ENV{HOME}/Library/Preferences/houdini/${HOUDINI_VERSION}")
		set(HOUDINI_FRAMEWORK_ROOT "/Library/Frameworks/Houdini.framework/Versions/${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD}")

	elseif(WIN32)
		set(USER_HOME "$ENV{HOME}")
		if(USER_HOME STREQUAL "")
			set(USER_HOME "$ENV{USERPROFILE}/Documents")
		endif()
		file(TO_CMAKE_PATH "${USER_HOME}" USER_HOME)

		set(HOUDINI_BIN_PATH  "${HOUDINI_INSTALL_ROOT}/bin")
		set(HOUDINI_HOME_PATH "${USER_HOME}/houdini${HOUDINI_VERSION}")

	else()
		set(HOUDINI_BIN_PATH "${HOUDINI_INSTALL_ROOT}/bin")
		set(HOUDINI_HOME_PATH "$ENV{HOME}/houdini${HOUDINI_VERSION}")

	endif()

	add_definitions(${HDK_DEFINITIONS})
	include_directories(${HDK_INCLUDES})
	link_directories(${HDK_LIBRARIES})
endmacro()


macro(houdini_plugin name sources)
	add_definitions(-DMAKING_DSO
					-DVERSION=${HOUDINI_VERSION}
					-DHOUDINI_DSO_VERSION=${HOUDINI_VERSION}
					-DUT_DSO_TAGINFO=${PLUGIN_TAGINFO}
	)

	set(libraryName ${name})
	add_library(${libraryName} SHARED ${sources})
	set_target_properties(${libraryName} PROPERTIES PREFIX "")
	target_link_libraries(${libraryName} ${HDK_LIBS})
	vfh_osx_flags(${libraryName})
endmacro()
