#
# Copyright (c) 2015, Chaos Software Ltd
#
# V-Ray For Houdini
#
# Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
# 
# All rights reserved. These coded instructions, statements and
# computer programs contain unpublished information proprietary to
# Chaos Group Ltd, which is protected by the appropriate copyright
# laws and may not be disclosed to third parties or copied or
# duplicated, in whole or in part, without prior written consent of
# Chaos Group Ltd.
#


macro(link_with_boost _name)
	if(WIN32)
		set(BOOST_LIBS
			boost_system-vc110-mt-1_55
		)
	else()
		set(BOOST_LIBS
			boost_system
		)
	endif()
	target_link_libraries(${_name} ${BOOST_LIBS})
endmacro()


macro(cgr_install_runtime _target _path)
	if(WIN32)
		install(TARGETS ${_target} RUNTIME DESTINATION ${_path})
	else()
		install(TARGETS ${_target}         DESTINATION ${_path})
	endif()
endmacro()
