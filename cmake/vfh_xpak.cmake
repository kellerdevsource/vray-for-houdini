#
# Copyright (c) 2015-2018, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

set(CGREPO $ENV{CGREPO})
if(NOT CGREPO)
	message(FATAL_ERROR "CGREPO is not set!")
endif()

set(X_PAK "" CACHE PATH "")
if(NOT X_PAK)
	message(FATAL_ERROR "X_PAK is not set!")
endif()

if(APPLE)
	set(X_XPAKTOOL_EXECUTABLE ${CGREPO}/bintools/mavericks_x64/xpaktool)
elseif(WIN32)
	set(X_XPAKTOOL_EXECUTABLE ${CGREPO}/bintools/x64/xpaktool.exe)
else()
	set(X_XPAKTOOL_EXECUTABLE ${CGREPO}/bintools/linux_x64/xpaktool)
endif()

function(vfh_install_xpak)
	cmake_parse_arguments(ARG "" "PAK;VERSION;OVERRIDE_WORKDIR" "" ${ARGN})
	if(ARG_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "Unrecognized options \"${ARG_UNPARSED_ARGUMENTS}\" passed to vfh_install_xpak()!")
	endif()
	if(NOT ARG_PAK)
		message(FATAL_ERROR "PAK is a required argument")
	endif()
	if(NOT ARG_VERSION)
		message(FATAL_ERROR "VERSION is a required argument")
	endif()

	set(ENV{HOME} $ENV{USERPROFILE})
	set(CMD ${X_XPAKTOOL_EXECUTABLE} xinstall -pak ${ARG_PAK}/${ARG_VERSION} -workdir ${X_PAK})

	execute_process(COMMAND ${CMD} RESULT_VARIABLE errCode)

	if(NOT "${errCode}" STREQUAL "0")
		message(FATAL_ERROR "${CMD} failed with code ${errCode}!")
	endif()
endfunction()
