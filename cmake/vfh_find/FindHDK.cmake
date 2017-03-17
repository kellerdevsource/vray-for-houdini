#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

find_package(PackageHandleStandardArgs)


# Houdini 15 and above defines version in SYS/SYS_Version.h
set(__hdk_version_filepath "toolkit/include/SYS/SYS_Version.h")

find_path(__hdk_path ${__hdk_version_filepath}
	${SDK_PATH}/hdk/hdk${HDK_FIND_VERSION} ${HOUDINI_INSTALL_ROOT}
	NO_DEFAULT_PATH
)

set(HDK_PATH ${__hdk_path})
unset(__hdk_path CACHE)

if(APPLE)
	set(HDK_INCLUDES "${HDK_PATH}/../Resources/toolkit/include")
	set(HDK_LIBRARIES "${HDK_PATH}/../Libraries")

elseif(WIN32)
	set(HDK_INCLUDES "${HDK_PATH}/toolkit/include")
	set(HDK_LIBRARIES "${HDK_PATH}/custom/houdini/dsolib")

else()
	set(HDK_INCLUDES "${HDK_PATH}/toolkit/include")
	set(HDK_LIBRARIES "${HDK_PATH}/dsolib")

endif()


# Find out the current version by parsing HDK version from HDK version file
set(HDK_VERSION_FILE ${HDK_PATH}/${__hdk_version_filepath})
if(EXISTS ${HDK_VERSION_FILE})

	file(STRINGS "${HDK_VERSION_FILE}" __hdk_major_version REGEX "^#define[\t ]+SYS_VERSION_MAJOR_INT[\t ]+.*")
	file(STRINGS "${HDK_VERSION_FILE}" __hdk_minor_version REGEX "^#define[\t ]+SYS_VERSION_MINOR_INT[\t ]+.*")
	file(STRINGS "${HDK_VERSION_FILE}" __hdk_build_version REGEX "^#define[\t ]+SYS_VERSION_BUILD_INT[\t ]+.*")
	file(STRINGS "${HDK_VERSION_FILE}" __hdk_patch_version REGEX "^#define[\t ]+SYS_VERSION_PATCH_INT[\t ]+.*")

	string(REGEX REPLACE "^.*SYS_VERSION_MAJOR_INT[\t ]+([0-9]*).*$" "\\1" HDK_MAJOR_VERSION "${__hdk_major_version}")
	string(REGEX REPLACE "^.*SYS_VERSION_MINOR_INT[\t ]+([0-9]*).*$" "\\1" HDK_MINOR_VERSION "${__hdk_minor_version}")
	string(REGEX REPLACE "^.*SYS_VERSION_BUILD_INT[\t ]+([0-9]*).*$" "\\1" HDK_BUILD_VERSION "${__hdk_build_version}")
	string(REGEX REPLACE "^.*SYS_VERSION_PATCH_INT[\t ]+([0-9]*).*$" "\\1" HDK_PATCH_VERSION "${__hdk_patch_version}")

	# Clean up cache
	unset(__hdk_major_version)
	unset(__hdk_minor_version)
	unset(__hdk_build_version)
	unset(__hdk_patch_version)

	set(HDK_VERSION "${HDK_MAJOR_VERSION}.${HDK_MINOR_VERSION}.${HDK_BUILD_VERSION}.${HDK_PATCH_VERSION}")

endif()

find_package_handle_standard_args(HDK
	REQUIRED_VARS HDK_PATH HDK_INCLUDES HDK_LIBRARIES HDK_VERSION
	VERSION_VAR   HDK_VERSION
)

# Version check
if(NOT ("${HDK_FIND_VERSION_MAJOR}.${HDK_FIND_VERSION_MINOR}"
		VERSION_EQUAL
		"${HDK_MAJOR_VERSION}.${HDK_MINOR_VERSION}"))
	set(HDK_FOUND FALSE)
endif()


if(NOT HDK_FOUND AND HDK_FIND_REQUIRED)
	message(FATAL_ERROR "Found HDK ${HDK_VERSION}, uncompatible with required version ${HDK_FIND_VERSION}")
endif()


# Find required libs
find_library(__hdk_libgeo
	NAMES openvdb_sesi HoudiniGEO
	PATHS ${HDK_LIBRARIES}
	NO_DEFAULT_PATH
)

set(HDK_LIB_GEO ${__hdk_libgeo})
unset(__hdk_libgeo CACHE)

if(WIN32)
	find_library(__hdk_libhalf
		NAMES Half
		PATHS ${HDK_LIBRARIES}
		NO_DEFAULT_PATH
	)

	list(APPEND HDK_LIB_GEO ${__hdk_libhalf})
	unset(__hdk_libhalf CACHE)
endif()

foreach(loop_var IN ITEMS ${HDK_LIB_GEO})
	if(NOT EXISTS ${loop_var})
		set(HDK_FOUND FALSE)
		break()
	endif()
endforeach(loop_var)


if(NOT HDK_FOUND AND HDK_FIND_REQUIRED)
	message(FATAL_ERROR "Found HDK ${HDK_VERSION}, but one or more required libraries are missing from:"
						"${HDK_LIBRARIES}")
endif()


if(HDK_FOUND)
	# NOTE: exact list of compiler/linker arguments when building for Houdini can be queried with:
	# "hcustom --cflags/ldflags" - Display compiler/linker flags
	# common Houdini compiler flags
	set(HDK_DEFINITIONS
		-DAMD64
		-DSIZEOF_VOID_P=8
		-DSESI_LITTLE_ENDIAN
		-DFBX_ENABLED=1
		-DOPENCL_ENABLED=1
		-DOPENVDB_ENABLED=1
	)
	set(HDK_LIBS "")

	if(WIN32)
		list(APPEND HDK_DEFINITIONS
			-DI386
			-DWIN32
			-DSWAP_BITFIELDS
			-D_WIN32_WINNT=0x0502
			-DWINVER=0x0502
			-DNOMINMAX
			-DSTRICT
			-DWIN32_LEAN_AND_MEAN
			-D_USE_MATH_DEFINES
			-D_CRT_SECURE_NO_DEPRECATE
			-D_CRT_NONSTDC_NO_DEPRECATE
			-D_SCL_SECURE_NO_WARNINGS
			-DBOOST_ALL_NO_LIB
			-DEIGEN_MALLOC_ALREADY_ALIGNED=0
		)

		if(${HDK_MAJOR_VERSION} VERSION_EQUAL "16")
			# NOTE: openvdb_sesi is version 3.3.0 but HDK is using 4.0.0 API
			list(APPEND HDK_DEFINITIONS
				-DOPENVDB_3_ABI_COMPATIBLE
			)
		endif()

		file(GLOB HDK_LIBS_A "${HDK_LIBRARIES}/*.a")
		file(GLOB HDK_LIBS_LIB "${HDK_LIBRARIES}/*.lib")

		list(APPEND HDK_LIBS
			${HDK_LIBS_A}
			${HDK_LIBS_LIB}
		)

		set(SYSTEM_LIBS
			advapi32
			comctl32
			comdlg32
			gdi32
			kernel32
			msvcprt
			msvcrt
			odbc32
			odbccp32
			oldnames
			ole32
			oleaut32
			shell32
			user32
			uuid
			winspool
			ws2_32
		)

		list(APPEND HDK_LIBS ${SYSTEM_LIBS})

	else()
		list(APPEND HDK_DEFINITIONS
			-D_GNU_SOURCE
			-DENABLE_THREADS
			-DENABLE_UI_THREADS
			-DUSE_PTHREADS
			-DGCC3
			-DGCC4
			-D_REENTRANT
			-D_FILE_OFFSET_BITS=64
		)

		set(HDK_LIBS
			HoudiniUI
			HoudiniOPZ
			HoudiniOP3
			HoudiniOP2
			HoudiniOP1
			HoudiniSIM
			HoudiniGEO
			HoudiniPRM
			HoudiniUT
		)

		if(APPLE)
			# Houdini compiler flags for Apple
			list(APPEND HDK_DEFINITIONS
				-DNEED_SPECIALIZATION_STORAGE
				-DMBSD
				-DMBSD_COCOA
				-DMBSD_INTEL
			)

			list(APPEND HDK_LIBS
				z
				dl
				tbb
				tbbmalloc
				pthread
				QtCore
				QtGui
				"-framework Cocoa"
			)

		else()
			# Houdini compiler flags for Linux
			list(APPEND HDK_DEFINITIONS
				-DLINUX
			)

			list(APPEND HDK_LIBS
				GLU
				GL
				X11
				Xext
				Xi
				dl
				pthread
			)
		endif()

	endif()

endif(HDK_FOUND)
