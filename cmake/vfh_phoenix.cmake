#
# Copyright (c) 2015-2017, Chaos Software Ltd
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

	message_array("Using Phoenix libraries" Phoenix_LIBS)
endif()

macro(use_phx_sdk)
	if (Phoenix_FOUND)
		message_array("Using PHXSDK include path" Phoenix_INCLUDES)
		message_array("Using PHXSDK library path" Phoenix_LIBRARIES)

		add_definitions(-DCGR_HAS_AUR)
		include_directories(${Phoenix_INCLUDES})
		link_directories(${Phoenix_LIBRARIES})
	endif()
endmacro()