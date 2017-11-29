#
# Copyright (c) 2015-2017, Chaos Software Ltd
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
	set(VRayOSL_FOUND TRUE)
	set(VRayOSL_INCLUDES "${OSL_ROOT}/include")
	set(VRayOSL_LIBRARIES "${OSL_ROOT}/lib")

else()
	set(VRayOSL_FOUND FALSE)
endif()