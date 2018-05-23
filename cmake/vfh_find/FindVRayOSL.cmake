#
# Copyright (c) 2015-2018, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

if (SDK_PATH)
	set(OSL_ROOT "${SDK_PATH}/osl/")
endif()

if(OSL_ROOT AND (EXISTS ${OSL_ROOT}))
	set(OSL_LIB_NAME vrayosl)
	if (WIN32)
		set(OSL_LIB_NAME "${OSL_LIB_NAME}.lib")
	else()
		set(OSL_LIB_NAME "lib${OSL_LIB_NAME}.a")
	endif()

	if (EXISTS ${OSL_ROOT}/lib/${OSL_LIB_NAME})
		set(VRayOSL_FOUND TRUE)
		set(VRayOSL_INCLUDES "${OSL_ROOT}/include")
		set(VRayOSL_LIBRARIES "${OSL_ROOT}/lib")
	else()
		set(VRayOSL_FOUND FALSE)
	endif()
else()
	set(VRayOSL_FOUND FALSE)
endif()