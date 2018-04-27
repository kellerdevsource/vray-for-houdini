#
# Copyright (c) 2015-2018, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)
endif()

macro(vfh_osx_flags _project_name)
	if(APPLE)
		if(INSTALL_RELEASE)
			set(VFH_RPATH "@loader_path/../../appsdk/bin")
		else()
			set(VFH_RPATH "${SDK_PATH}/appsdk/bin")
		endif()

		# This sets search paths for modules like libvray.dylib
		# (no need for install_name_tool tweaks)
		set_target_properties(${_project_name}
			PROPERTIES
				INSTALL_RPATH "${VFH_RPATH}")

		add_custom_command(TARGET ${_project_name} POST_BUILD
			COMMAND install_name_tool -change ../../lib/mavericks_x64/gcc-4.2-cpp/libvrayoslquery.dylib @rpath/libvrayoslquery.dylib $<TARGET_FILE:${_project_name}>
			COMMAND install_name_tool -change ../../lib/mavericks_x64/gcc-4.2-cpp/libvrayoslexec.dylib @rpath/libvrayoslexec.dylib $<TARGET_FILE:${_project_name}>
			COMMAND install_name_tool -change ../../lib/mavericks_x64/gcc-4.2-cpp/libvrayoslcomp.dylib @rpath/libvrayoslcomp.dylib $<TARGET_FILE:${_project_name}>
			COMMAND install_name_tool -change ./lib/mavericks_x64/gcc-4.2-cpp/libvrayopenimageio.dylib @rpath/libvrayopenimageio.dylib $<TARGET_FILE:${_project_name}>
			COMMAND install_name_tool -change @rpath/Python @rpath/libpython2.7.dylib $<TARGET_FILE:${_project_name}>
		)
	endif()
endmacro()

macro(set_compiler_flags)
	if(WIN32)
		# Houdini specific
		set(CMAKE_CXX_FLAGS "/EHsc /GT /bigobj")

		# Houdini specific warnings
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4355 /w14996")

		# Enable multi core compilation
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

		set(CMAKE_CXX_FLAGS_DEBUG "/MD /Od /Zi /DNDEBUG /DVFH_DEBUG /DVASSERT_ENABLED")

		add_definitions(-DOPENEXR_DLL)
	else()
		set(CMAKE_CXX_FLAGS "-fPIC -std=c++11")
		set(CMAKE_CXX_FLAGS_DEBUG "-g -DNDEBUG -DVFH_DEBUG")
		set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

		if (APPLE)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
			add_definitions(-DBOOST_NO_CXX11_RVALUE_REFERENCES)
		endif()

		add_definitions(-D__OPTIMIZE__)

		# Force color compiler output
		if(CMAKE_GENERATOR STREQUAL "Ninja")
			if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
				CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
				set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
			elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 5.0.0)
				set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
			endif()
		endif()

		# disable warnings from AppSDK
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pragmas")

		# Houdini specific
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-parentheses -Wno-sign-compare -Wno-reorder -Wno-uninitialized -Wno-unused-parameter -Wno-deprecated -fno-strict-aliasing")

		# Houdini SDK / V-Ray SDK
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch -Wno-narrowing -Wno-int-to-pointer-cast")

		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DVASSERT_ENABLED")

		# Add time lib
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lrt")

		if (NOT APPLE)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-literal-suffix -Wno-unused-local-typedefs -Wno-placement-new")

			if(WITH_STATIC_LIBC)
				set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
			endif()
		endif()
	endif()

	message(STATUS "Using flags: ${CMAKE_CXX_FLAGS}")
	message(STATUS "  Release: ${CMAKE_CXX_FLAGS_RELEASE}")
	message(STATUS "  Debug:   ${CMAKE_CXX_FLAGS_DEBUG}")
endmacro()

if (UNIX AND NOT APPLE)
	set(CMAKE_CXX_CREATE_SHARED_LIBRARY "<CMAKE_CXX_COMPILER> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> <SONAME_FLAG><TARGET_SONAME> -o <TARGET> -Wl,--start-group <OBJECTS> <LINK_LIBRARIES> -Wl,--end-group")
endif()
