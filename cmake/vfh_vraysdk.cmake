#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

find_package(VRaySDK)

macro(use_vray_sdk)
	if(NOT VRaySDK_FOUND)
		message(FATAL_ERROR "V-Ray SDK NOT found!\n"
							"To specify V-Ray SDK search path, use one of the following options:\n"
							"-DVRAYSDK_PATH=<V-Ray SDK root location>\n"
							"-DSDK_PATH=<VFH dependencies location> and VRAYSDK_VERSION\n"
							"or install V-Ray For Maya"
							)
	endif()

	if(WIN32)
		# Both V-Ray SDK and HDK defines some basic types,
		# tell V-Ray SDK not to define them
		add_definitions(
			-DVUTILS_NOT_DEFINE_INT8
			-DVUTILS_NOT_DEFINE_UINT8
		)
	endif()

	include_directories(${VRaySDK_INCLUDES})
	link_directories(${VRaySDK_LIBRARIES})

	# Check if there is vrscene preview library
	if (CGR_HAS_VRSCENE)
		add_definitions(-DCGR_HAS_VRAYSCENE)
	endif()

	if(WIN32)
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DVASSERT_ENABLED")
	endif()
endmacro()


macro(link_with_vray_sdk _name)
	set(VRAY_SDK_LIBS
		alembic_s
		bmputils_s
		meshes_s
		meshinfosubdivider_s
		rayserver_s
		osdCPU_s
		openexr_s
		pimglib_s
		plugman_s
		putils_s
		vutils_s
		jpeg_s
		libpng_s
		tiff_s
	)

	if(WIN32)
		if(HDK_MAJOR_VERSION VERSION_GREATER 15.0)
			list(APPEND VRAY_SDK_LIBS ${HDK_QT_LIBS})
		else()
			list(APPEND VRAY_SDK_LIBS QtCore4)
		endif()

		list(APPEND VRAY_SDK_LIBS zlib_s)
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
		vray
	)
	target_link_libraries(${_name} ${VRAY_LIBS})
endmacro()
