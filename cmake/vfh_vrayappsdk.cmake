#
# Copyright (c) 2015, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

string(TOLOWER "${CMAKE_HOST_SYSTEM_NAME}" _HOST_SYSTEM_NAME)

set(APPSDK_VERSION "447" CACHE STRING "V-Ray AppSDK version")
set(APPSDK_PATH "$ENV{HOME}/src/appsdk_releases" CACHE PATH "V-Ray AppSDK location")
set(APPSDK_ROOT "${APPSDK_PATH}/${APPSDK_VERSION}/${_HOST_SYSTEM_NAME}" CACHE PATH "V-Ray AppSDK root" FORCE)
option(APPSDK_QT "AppSDK is Qt based" OFF)


macro(use_vray_appsdk)
	message(STATUS "Using V-Ray AppSDK: ${APPSDK_ROOT}")

	if(NOT EXISTS ${APPSDK_ROOT})
		message(FATAL_ERROR "V-Ray AppSDK root (\"${APPSDK_ROOT}\") doesn't exist!")
	endif()

	message(STATUS "Using V-Ray AppSDK Qt: ${APPSDK_QT}")

	if (APPSDK_QT)
		add_definitions(-DVRAY_APPSDK_QT)
	else()
		remove_definitions(-DVRAY_APPSDK_QT)
	endif()

	add_definitions(-DVRAY_SDK_INTEROPERABILITY)

	include_directories(${APPSDK_ROOT}/cpp/include)
	link_directories(${APPSDK_ROOT}/bin)
	link_directories(${APPSDK_ROOT}/lib)
	link_directories(${APPSDK_PATH}/devel)
endmacro()


macro(link_with_vray_appsdk _name)
	set(APPSDK_LIBS
		VRaySDKLibrary
	)
	target_link_libraries(${_name} ${APPSDK_LIBS})
endmacro()
