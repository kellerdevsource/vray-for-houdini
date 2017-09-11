#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

# NOTE: Testing can be enabled/disabled through -DBUILD_TESTING=ON/OFF
# Add -WITH_TESTS=ON/OFF to enable/disable integration tests

if(BUILD_TESTING)
	message(STATUS "Tests enabled")

	add_test(NAME vfhpytests COMMAND python -m unittest discover WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

	# Set VFH environment variables
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
				APPEND PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${APPSDK_ROOT}/bin:${HOUDINI_INSTALL_ROOT}/dsolib:$ENV{LD_LIBRARY_PATH}")
	set_property( TEST vfhpytests
				APPEND PROPERTY ENVIRONMENT "DYLD_LIBRARY_PATH=${APPSDK_ROOT}/bin:$ENV{DYLD_LIBRARY_PATH}")
	set_property( TEST vfhpytests
				APPEND PROPERTY ENVIRONMENT "PATH=${APPSDK_ROOT}/bin:${HOUDINI_INSTALL_ROOT}/bin:$ENV{PATH}")
	set_property( TEST vfhpytests
				APPEND PROPERTY ENVIRONMENT "PYTHONPATH=${APPSDK_ROOT}/python27/bin:$PYTHONPATH")
endif()
