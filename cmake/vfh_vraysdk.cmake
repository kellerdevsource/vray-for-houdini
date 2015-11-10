#
# Copyright (c) 2015, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

set(_maya_versons "2017;2016;2015;2014")
foreach(_maya_version ${_maya_versons})
	if(WIN32)
		set(_vray_for_maya_root "C:/Program Files/Chaos Group/V-Ray/Maya ${_maya_version} for x64")
	elseif(APPLE)
		set(_vray_for_maya_root "/Applications/ChaosGroup/V-Ray/Maya${_maya_version}")
	else()
		set(_vray_for_maya_root "/usr/ChaosGroup/V-Ray/Maya${_maya_version}-x64")
	endif()

	if(EXISTS ${_vray_for_maya_root})
		if(WIN32)
			set(_vray_os_subdir       "x64")
			set(_vray_compiler_subdir "vc101")
			if (${_maya_version} GREATER 2014)
				set(_vray_compiler_subdir "vc11")
			endif()
		elseif(APPLE)
			if(${_maya_version} GREATER 2015)
				set(_vray_os_subdir   "mavericks_x64")
			else()
				set(_vray_os_subdir   "mountain_lion_x64")
			endif()
			set(_vray_compiler_subdir "gcc-4.2")
		else()
			set(_vray_os_subdir       "linux_x64")
			set(_vray_compiler_subdir "gcc-4.4")
		endif()

		set(vray_for_maya_incpaths
			${_vray_for_maya_root}/include
		)
		set(vray_for_maya_libpaths
			${_vray_for_maya_root}/lib/${_vray_os_subdir}
			${_vray_for_maya_root}/lib/${_vray_os_subdir}/${_vray_compiler_subdir}
		)
		break()
	endif()
endforeach()

set(CGR_VRAYSDK_INCPATH "" CACHE STRING "V-Ray SDK include path")
set(CGR_VRAYSDK_LIBPATH "" CACHE STRING "V-Ray SDK library path")

set(VRAYSDK_INCPATH "" CACHE INTERNAL "")
set(VRAYSDK_LIBPATH "" CACHE INTERNAL "")

if(NOT CGR_VRAYSDK_INCPATH STREQUAL "")
	set(VRAYSDK_INCPATH ${CGR_VRAYSDK_INCPATH} CACHE PATH "" FORCE)
else()
	set(VRAYSDK_INCPATH ${vray_for_maya_incpaths} CACHE PATH "" FORCE)
endif()

if(NOT CGR_VRAYSDK_LIBPATH STREQUAL "")
	set(VRAYSDK_LIBPATH ${CGR_VRAYSDK_LIBPATH} CACHE PATH "" FORCE)
else()
	set(VRAYSDK_LIBPATH ${vray_for_maya_libpaths} CACHE PATH "" FORCE)
endif()

macro(use_vray_sdk)
	message(STATUS "Using V-Ray SDK include path: ${VRAYSDK_INCPATH}")
	message(STATUS "Using V-Ray SDK library path: ${VRAYSDK_LIBPATH}")

	if(WIN32)
		# Both V-Ray SDK and HDK defines some basic types,
		# tell V-Ray SDK not to define them
		add_definitions(
			-DVUTILS_NOT_DEFINE_INT8
			-DVUTILS_NOT_DEFINE_UINT8
		)
	endif()

	if(NOT EXISTS ${VRAYSDK_INCPATH})
		message(FATAL_ERROR "V-Ray SDK libraries / headers are not found!\n"
							"V-Ray SDK from V-Ray For Maya installation is utilized by default.\n"
							"Install V-Ray For Maya or point CGR_VRAYSDK_INCPATH and CGR_VRAYSDK_LIBPATH variables to the SDK location.")
	endif()

	include_directories(${VRAYSDK_INCPATH})
	link_directories(${VRAYSDK_LIBPATH})

	# Check if there is vrscene preview library
	find_path(CGR_HAS_VRSCENE vrscene_preview.h PATHS ${VRAYSDK_INCPATH})
	if (CGR_HAS_VRSCENE)
		add_definitions(-DCGR_HAS_VRAYSCENE)
	endif()
endmacro()


macro(link_with_vray_sdk _name)
	set(VRAY_SDK_LIBS
		alembic_s
		bmputils_s
		meshes_s
		meshinfosubdivider_s
		osdCPU_s
		openexr_s
		pimglib_s
		plugman_s
		putils_s
		vutils_s
	)
	list(APPEND VRAY_SDK_LIBS
		jpeg_s
		libpng_s
		tiff_s
	)
	if(WIN32)
		list(APPEND VRAY_SDK_LIBS
			zlib_s
			QtCore4
		)
	endif()
	if(CGR_HAS_VRSCENE)
		list(APPEND VRAY_SDK_LIBS
			treeparser_s
			vrscene_s
		)
	endif()
	target_link_libraries(${_name} ${VRAY_SDK_LIBS})
endmacro()


macro(link_with_vray _name)
	set(VRAY_LIBS
		cgauth
		vray
	)
	target_link_libraries(${_name} ${VRAY_LIBS})
endmacro()
