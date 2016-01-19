#
# Copyright (c) 2015, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

# CTest module invokes "enable_testing()" command automatically when the BUILD_TESTING option is ON
# add -DBUILD_TESTING=ON/OFF to enble/disable testing

if(BUILD_TESTING)
# ... CMake code to create tests goes here...

	message( STATUS "VFH tests enabled")

	add_test(NAME vfhpytests COMMAND python -m unittest discover WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

	# set VFH environment variables
	# TODO: need a better way to set VFH test environment
	set_property( TEST vfhpytests
				APPEND PROPERTY ENVIRONMENT "HOUDINI13_VOLUME_COMPATIBILITY=1")
	set_property( TEST vfhpytests
				APPEND PROPERTY ENVIRONMENT "HOUDINI_DSO_ERROR=1")
	set_property( TEST vfhpytests
				APPEND PROPERTY ENVIRONMENT "HFS=${HOUDINI_INSTALL_ROOT}")
	set_property( TEST vfhpytests
				APPEND PROPERTY ENVIRONMENT "VRAY_PLUGIN_DESC_PATH=${CMAKE_SOURCE_DIR}/deploy/plugins_desc")
	set_property( TEST vfhpytests
				APPEND PROPERTY ENVIRONMENT "VRAY_SDK=${APPSDK_ROOT}")
	set_property( TEST vfhpytests
				APPEND PROPERTY ENVIRONMENT "VRAY_PATH=${APPSDK_ROOT}/bin")
	set_property( TEST vfhpytests
				APPEND PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${HOUDINI_INSTALL_ROOT}/dsolib:${APPSDK_ROOT}/bin:${HOUDINI_HOME_PATH}/vfh/lib:$ENV{LD_LIBRARY_PATH}")
	set_property( TEST vfhpytests
				APPEND PROPERTY ENVIRONMENT "PATH=${HOUDINI_INSTALL_ROOT}/bin:${APPSDK_ROOT}/bin:$ENV{PATH}")

endif()
