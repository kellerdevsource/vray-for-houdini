#
# Copyright (c) 2015-2017, Chaos Software Ltd
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


function(vfh_make_moc)
	cmake_parse_arguments(PAR "" "TARGET;FILE_IN;FILE_OUT_NAME" "DEFINITIONS" ${ARGN})

	if(PAR_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "vfh_make_moc() invalid arguments: ${PAR_UNPARSED_ARGUMENTS}")
	endif()

	# Unix Makefiles generator will not automagically create the output dir needed
	# by the custom command, so do it manually
	set(FILE_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
	file(MAKE_DIRECTORY ${FILE_OUT_DIR})
	if (WIN32)
		set(EXE_EXT ".exe")
	else()
		set(EXE_EXT "")
	endif()

	set(MOC_PATH ${QT_TOOLS_PATH}/moc${EXE_EXT})

	# Execute moc on file changes
	add_custom_command(
		OUTPUT ${FILE_OUT_DIR}/${PAR_FILE_OUT_NAME}
		COMMAND ${MOC_PATH} ${PAR_DEFINITIONS} -o${PAR_FILE_OUT_NAME} ${PAR_FILE_IN}
		DEPENDS	${PAR_FILE_IN}
		COMMENT	"Using ${MOC_PATH} to compile ${PAR_FILE_IN} to ${PAR_FILE_OUT_NAME}"
		WORKING_DIRECTORY ${FILE_OUT_DIR}
		VERBATIM
	)

	# Add dependency
	set_source_files_properties(${PAR_FILE_IN} PROPERTIES OBJECT_DEPENDS ${FILE_OUT_DIR}/${PAR_FILE_OUT_NAME})

	# For target to find output moc files
	target_include_directories(${PAR_TARGET} PRIVATE ${FILE_OUT_DIR})
endfunction()


function(vfh_find_library)
	cmake_parse_arguments(ARG "" "VAR" "NAMES;PATHS" ${ARGN})

	find_library(${ARG_VAR}
		NAMES
			${ARG_NAMES}
		PATHS
			${ARG_PATHS}
		NO_DEFAULT_PATH
		NO_CMAKE_ENVIRONMENT_PATH
		NO_CMAKE_PATH
		NO_SYSTEM_ENVIRONMENT_PATH
		NO_CMAKE_SYSTEM_PATH
	)

	set(${ARG_VAR} ${${ARG_VAR}} PARENT_SCOPE)

	if(NOT ${ARG_VAR})
		message(WARNING "Phoenix SDK part \"${ARG_NAMES}\" is not found under \"${ARG_PATHS}\"!")
	endif()
endfunction()


function(vfh_find_file)
	cmake_parse_arguments(ARG "" "VAR" "NAMES;PATHS" ${ARGN})

	find_file(${ARG_VAR}
		NAMES
			${ARG_NAMES}
		PATHS
			${ARG_PATHS}
		NO_DEFAULT_PATH
		NO_CMAKE_ENVIRONMENT_PATH
		NO_CMAKE_PATH
		NO_SYSTEM_ENVIRONMENT_PATH
		NO_CMAKE_SYSTEM_PATH
	)

	set(${ARG_VAR} ${${ARG_VAR}} PARENT_SCOPE)

	if(NOT ${ARG_VAR})
		message(WARNING "Phoenix SDK part \"${ARG_NAMES}\" is not found under \"${ARG_PATHS}\"!")
	endif()
endfunction()

# Generate launcher with all needed environment variables set.
# NOTE: used to generate .bat launchers and also used in c++ launcher
function(vfh_generate_launcher)
	cmake_parse_arguments(ARG "BATCH;RELEASE;CPP_LAUNCHER" "TEMPLATE_FILENAME;FILENAME;DESTINATION;BIN;TEMPLATE_DIR" "" ${ARGN})

	if(NOT ARG_BIN)
		if(ARG_CPP_LAUNCHER)
			# Set executable file
			if(ARG_BATCH)
				set(EXE_FILE hbatch)
			else()
				set(EXE_FILE houdini)
			endif()

			# Set working dir and executable extension
			if(WIN32)
				set(EXE_EXT ".exe")
				set(HFS_DIR ${HOUDINI_INSTALL_ROOT})
			else()
				set(EXE_EXT "")
				set(HFS_DIR ${HFS})
			endif()

			set(ARG_BIN ${HFS_DIR}/bin/${EXE_FILE}${EXE_EXT})
		else()
			if(WIN32)
				if(ARG_BATCH)
					set(ARG_BIN "\"%HFS%\\bin\\hbatch.exe\"")
				else()
					set(ARG_BIN "start \"V-Ray For Houdini\" /D \"%USERPROFILE%\\Desktop\" \"%HFS%\\bin\\houdini.exe\"")
				endif()
			elseif(APPLE)
			else()
				if(ARG_BATCH)
					set(ARG_BIN "\"\${HFS}/bin/hbatch\"")
				else()
					set(ARG_BIN "\"\${HFS}/bin/houdini\" -foreground")
				endif()
			endif()
		endif()
	endif()

	if(NOT ARG_TEMPLATE_FILENAME)
		if(WIN32)
			set(ARG_TEMPLATE_FILENAME "hfs.bat.in")
		elseif(APPLE)
			set(ARG_TEMPLATE_FILENAME "hfs_osx.sh.in")
		else()
			set(ARG_TEMPLATE_FILENAME "hfs_linux.sh.in")
		endif()
	endif()

	if(NOT ARG_DESTINATION)
		set(ARG_DESTINATION ${CMAKE_BINARY_DIR})
	endif()

	if(NOT ARG_FILENAME)
		if(ARG_RELEASE)
			if(ARG_BATCH)
				set(FILENAME_PREFIX hbatch)
			else()
				set(FILENAME_PREFIX hfs)
			endif()
			set(FILENAME_PREFIX ${FILENAME_PREFIX}${HOUDINI_VERSION}.${HOUDINI_VERSION_BUILD})
		else()
			if(ARG_BATCH)
				set(FILENAME_PREFIX vfh_hbatch)
			else()
				set(FILENAME_PREFIX vfh_hfs)
			endif()
		endif()

		if(WIN32)
			set(ARG_FILENAME ${FILENAME_PREFIX}.bat)
		else()
			set(ARG_FILENAME ${FILENAME_PREFIX}.sh)
		endif()
	endif()

	if(NOT ARG_TEMPLATE_DIR)
		set(ARG_TEMPLATE_DIR ${CMAKE_SOURCE_DIR}/deploy)
	endif()

	file(TO_NATIVE_PATH ${CMAKE_SOURCE_DIR} CMAKE_SOURCE_DIR)
	file(TO_NATIVE_PATH ${APPSDK_ROOT} APPSDK_ROOT)
	file(TO_NATIVE_PATH ${Phoenix_LIBRARIES} Phoenix_LIBRARIES)
	set(HFS_BIN ${ARG_BIN})

	set(TMP_FILEPATH ${CMAKE_BINARY_DIR}/tmp/${ARG_FILENAME})

	configure_file(${ARG_TEMPLATE_DIR}/${ARG_TEMPLATE_FILENAME}
	               ${TMP_FILEPATH}
	               @ONLY)

	if(ARG_CPP_LAUNCHER)
		# For cpp launcher we need to make the template now so we COPY instead of INSTALL
		file(COPY ${TMP_FILEPATH}
			DESTINATION
				${ARG_DESTINATION})
	else()
		file(INSTALL ${TMP_FILEPATH}
			DESTINATION
				${ARG_DESTINATION}
			FILE_PERMISSIONS
				OWNER_READ OWNER_WRITE OWNER_EXECUTE
				GROUP_READ GROUP_EXECUTE
				WORLD_READ WORLD_EXECUTE)
	endif()

	file(REMOVE ${TMP_FILEPATH})
endfunction()
