#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

find_package(Phoenix)

if(Phoenix_FOUND)
	message(STATUS "Using Phoenix SDK include path: ${Phoenix_INCLUDES}")
	message(STATUS "Using Phoenix SDK library path: ${Phoenix_LIBRARIES}")
	message(STATUS "Using Phoenix SDK loaders: [${CGR_PHOENIX_SHARED}, ${CGR_PHOENIX_SHARED_F3D}, ${CGR_PHOENIX_SHARED_VDB}]")
endif()
