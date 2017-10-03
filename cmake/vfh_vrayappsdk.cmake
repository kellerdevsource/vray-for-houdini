#
# Copyright (c) 2015-2017, Chaos Software Ltd
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
		message(FATAL_ERROR "V-Ray AppSDK NOT found. Required version is ${APPSDK_VERSION}\n"
							"To specify AppSDK search path, use one of the following options:\n"
							"-DAPPSDK_PATH=<AppSDK root location>\n"
							"-DSDK_PATH=<VFH dependencies location>")
	endif()

	set(APPSDK_ROOT "${_appsdk_root}")
	message_array("Using V-Ray AppSDK" APPSDK_ROOT)

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
