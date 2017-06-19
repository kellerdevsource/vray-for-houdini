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
	message_array("Using Phoenix SDK include path" Phoenix_INCLUDES)
	message_array("Using Phoenix SDK library path" Phoenix_LIBRARIES)

	set(PHOENIX_LOADERS "${CGR_PHOENIX_SHARED};${CGR_PHOENIX_SHARED_F3D};${CGR_PHOENIX_SHARED_VDB}")
	message_array("Using Phoenix loaders" PHOENIX_LOADERS)
endif()
