#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

include(FindGit)


macro(link_with_boost _name)
	if(WIN32)
		if(HOUDINI_VERSION VERSION_LESS 15.5)
			set(BOOST_LIBS boost_system-vc110-mt-1_55)
		else()
			set(BOOST_LIBS boost_system-vc140-mt-1_55)
		endif()
	else()
		set(BOOST_LIBS boost_system)
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


macro(cgr_get_git_hash _dir _out_var)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
		WORKING_DIRECTORY ${_dir}
		OUTPUT_VARIABLE ${_out_var}
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
endmacro()


function(message_array _label _array)
	message(STATUS "${_label}:")
	foreach(_item ${${_array}})
		message(STATUS "  ${_item}")
	endforeach()
endfunction()
