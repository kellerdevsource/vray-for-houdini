#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#


macro(use_vray_appsdk)
	find_package(AppSDK)

	if(NOT AppSDK_FOUND)
		message(FATAL_ERROR "V-Ray AppSDK NOT found!\n"
							"Please specify a valid APPSDK_PATH or SDK_PATH and APPSDK_VERSION or APPSDK_VERSION!")
	endif()

	set(APPSDK_ROOT "${_appsdk_root}")
	message(STATUS "Using V-Ray AppSDK: ${APPSDK_ROOT}")

	add_definitions(-DVRAY_SDK_INTEROPERABILITY)

	include_directories(${AppSDK_INCLUDES})
	link_directories(${AppSDK_LIBRARIES})
endmacro()


macro(link_with_vray_appsdk _name)
	set(APPSDK_LIBS
		VRaySDKLibrary
	)
	target_link_libraries(${_name} ${APPSDK_LIBS})
endmacro()
