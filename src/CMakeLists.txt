#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

set(SDK_PATH "" CACHE STRING "SDK root qpath")

include_directories(.)
include_directories(io)
include_directories(op)
include_directories(gu)
include_directories(sop)
include_directories(vop)
include_directories(rop)
include_directories(lib)
include_directories(vop/brdf)
include_directories(vop/material)
include_directories(vop/texture)
include_directories(vop/uvwgen)
include_directories(vop/meta)
include_directories(export)
include_directories(export/utils)
include_directories(export/export)
include_directories(utils)

# Qt has JSON modules only in > 5.x version and we need to support 4.x also.
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/rapidjson/include)

set(EXPORTER_SOURCES
	export/export/vfh_export_geom.cpp
	export/export/vfh_export_hair.cpp
	export/export/vfh_export_mesh.cpp
	export/export/vfh_export_primitive.cpp
	export/export/vfh_export_vrayproxy.cpp
	export/export/vfh_export_hdk_shaders.cpp
	export/export/vfh_export_op.cpp
	export/export/vfh_material_override.cpp
	export/utils/vfh_geoutils.cpp
	export/utils/vfh_tex_utils.cpp
	export/utils/vfh_utils_vrmesh.cpp
	export/vfh_export_context.cpp
	export/vfh_export_light.cpp
	export/vfh_export_material.cpp
	export/vfh_export_object.cpp
	export/vfh_export_view.cpp
	export/vfh_exporter.cpp
	export/vfh_plugin_attrs.cpp
	export/vfh_plugin_exporter.cpp
	utils/vfh_error.cpp
	utils/vfh_hashes.cpp
	utils/vfh_hou_utils.cpp
	utils/vfh_log.cpp
	utils/vfh_lru_cache.hpp
	utils/vfh_op_utils.cpp
	utils/vfh_plugin_info.cpp
	utils/vfh_prm_def.cpp
	utils/vfh_prm_defaults.cpp
	utils/vfh_prm_globals.cpp
	utils/vfh_prm_json.cpp
	utils/vfh_prm_templates.cpp
	lib/vfh_vray_instances.cpp
	op/op_node_base.cpp
)

set(EXPORTER_HEADERS
	export/export/vfh_export_geom.h
	export/export/vfh_export_hair.h
	export/export/vfh_export_mesh.h
	export/export/vfh_export_primitive.h
	export/export/vfh_export_vrayproxy.h
	export/export/vfh_material_override.h
	export/utils/vfh_geoutils.h
	export/utils/vfh_tex_utils.h
	export/utils/vfh_utils_vrmesh.h
	export/vfh_export_context.h
	export/vfh_export_view.h
	export/vfh_exporter.h
	export/vfh_plugin_attrs.h
	export/vfh_plugin_exporter.h
	export/vfh_vray.h
	utils/vfh_attr_utils.h
	utils/vfh_error.h
	utils/vfh_hashes.h
	utils/vfh_hou_utils.h
	utils/vfh_log.h
	utils/vfh_op_utils.h
	utils/vfh_plugin_info.h
	utils/vfh_primitives.h
	utils/vfh_prm_def.h
	utils/vfh_prm_defaults.h
	utils/vfh_prm_globals.h
	utils/vfh_prm_json.h
	utils/vfh_prm_templates.h
	lib/vfh_vray_instances.h
	op/op_node_base.h
)

set(SOURCES
	cmd/vfh_cmd_register.cpp
	cmd/vfh_cmd_vrayproxy.cpp
	gu/gu_volumegridref.cpp
	gu/gu_vrayproxyref.cpp
	gu/gu_vraysceneref.cpp
	gu/gu_geomplaneref.cpp
	io/io_vrmesh.cpp
	obj/obj_node_base.cpp
	rop/rop_vrayproxyrop.cpp
	rop/vfh_rop.cpp
	sop/sop_GeomPlane.cpp
	sop/sop_PhoenixCache.cpp
	sop/sop_vrayproxy.cpp
	sop/sop_vrayscene.cpp
	sop/vfh_vrayproxyutils.cpp
	vfh_register.cpp
	vop/brdf/vop_BRDFLayered.cpp
	vop/brdf/vop_BRDFScanned.cpp
	vop/brdf/vop_BRDFVRayMtl.cpp
	vop/env/vop_SettingsEnvironment.cpp
	vop/material/vop_GeomDisplacedMesh.cpp
	vop/material/vop_GeomStaticSmoothedMesh.cpp
	vop/material/vop_MaterialOutput.cpp
	vop/material/vop_MtlMulti.cpp
	vop/material/vop_PhoenixSim.cpp
	vop/meta/vop_meta_image_file.cpp
	vop/rc/vop_rc_container.cpp
	vop/texture/vop_TexFalloff.cpp
	vop/texture/vop_TexGradRamp.cpp
	vop/texture/vop_TexLayeredMax.cpp
	vop/texture/vop_TexRemap.cpp
	vop/texture/vop_texture_output.cpp
	vop/vop_context.cpp
	vop/vop_node_base.cpp
)

set(HEADERS
	cmd/vfh_cmd_register.h
	cmd/vfh_cmd_vrayproxy.h
	gu/gu_volumegridref.h
	gu/gu_vrayproxyref.h
	gu/gu_vraysceneref.h
	gu/gu_geomplaneref.h
	io/io_vrmesh.h
	obj/obj_node_base.h
	obj/obj_node_def.h
	rop/rop_vrayproxyrop.h
	rop/vfh_rop.h
	sop/sop_GeomPlane.h
	sop/sop_node_base.h
	sop/sop_node_def.h
	sop/sop_PhoenixCache.h
	sop/sop_vrayproxy.h
	sop/sop_vrayscene.h
	sop/vfh_vrayproxyutils.h
	vfh_class_utils.h
	vfh_defines.h
	vfh_includes.h
	vfh_typedefs.h
	vop/brdf/vop_brdf_def.h
	vop/brdf/vop_BRDFLayered.h
	vop/brdf/vop_BRDFScanned.h
	vop/brdf/vop_BRDFVRayMtl.h
	vop/env/vop_env_def.h
	vop/env/vop_SettingsEnvironment.h
	vop/material/vop_GeomDisplacedMesh.h
	vop/material/vop_GeomStaticSmoothedMesh.h
	vop/material/vop_MaterialOutput.h
	vop/material/vop_mtl_def.h
	vop/material/vop_MtlMulti.h
	vop/material/vop_PhoenixSim.h
	vop/meta/vop_meta_def.h
	vop/meta/vop_meta_image_file.h
	vop/rc/vop_rc_container.h
	vop/rc/vop_rc_def.h
	vop/texture/vop_TexFalloff.h
	vop/texture/vop_TexGradRamp.h
	vop/texture/vop_TexLayeredMax.h
	vop/texture/vop_TexRemap.h
	vop/texture/vop_texture_def.h
	vop/texture/vop_texture_output.h
	vop/uvwgen/vop_uvwgen_def.h
	vop/vop_context.h
	vop/vop_node_base.h
)

if (WIN32)
	function(GroupSources curdir)
		file(GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${curdir} ${PROJECT_SOURCE_DIR}/${curdir}/*)
		foreach(child ${children})
			if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${curdir}/${child})
				GroupSources(${curdir}/${child})
			else()
				string(REPLACE "/" "\\" groupname ${curdir})
				string(REPLACE "src\\" "" groupname ${groupname})
				source_group(${groupname} FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
			endif()
		endforeach()
	endfunction()
	GroupSources(src)
endif()

# *.aur preview
if (Phoenix_FOUND)
	# this will enable compilation for the code that integrates phx
	add_definitions(-DCGR_HAS_AUR)
	include_directories(${Phoenix_INCLUDES})
	link_directories(${Phoenix_LIBRARIES})
endif()

houdini_library(vray_for_houdini_exporter "${EXPORTER_SOURCES};${EXPORTER_HEADERS}")
houdini_plugin(${PROJECT_NAME} "${SOURCES};${HEADERS}")

target_link_libraries(${PROJECT_NAME} vray_for_houdini_exporter)

link_with_vray_sdk(${PROJECT_NAME})
link_with_vray(${PROJECT_NAME})
link_with_vray_appsdk(${PROJECT_NAME})
link_with_boost(${PROJECT_NAME})

if (Phoenix_FOUND)
	target_link_libraries(${PROJECT_NAME} ${Phoenix_LIBS})
endif()

add_definitions(-DCGR_GIT_HASH=${SRC_GIT_HASH})

function(vfh_install_common _location)
	# Copy Phoenix cache file loader library
	if (Phoenix_FOUND)
		install(
			FILES
				${CGR_PHOENIX_SHARED}
				${CGR_PHOENIX_SHARED_VDB}
				${CGR_PHOENIX_SHARED_F3D}
			DESTINATION
				${_location}/vfh_home/libs
			PERMISSIONS
				OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
	endif()

	# Copy plugin
	cgr_install_runtime(${PROJECT_NAME} ${_location}/vfh_home/dso)

	# Copy config
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/deploy/vfh_home
			DESTINATION ${_location}
			PATTERN "otls/*" EXCLUDE
			PATTERN "otls" EXCLUDE
			PATTERN "*.pyc" EXCLUDE
	)

	if (WITH_SOHO)
		install(DIRECTORY ${CMAKE_SOURCE_DIR}/deploy/vfh_home/otls
				DESTINATION ${_location}/vfh_home
				PATTERN "backup/*" EXCLUDE
				PATTERN "backup" EXCLUDE
		)
	endif()
endfunction(vfh_install_common)

if(INSTALL_RELEASE)
	vfh_install_common(${INSTALL_RELEASE_PATH})

	# Generate launcher file
	vfh_generate_launcher(RELEASE BATCH DESTINATION ${INSTALL_RELEASE_PATH})
	vfh_generate_launcher(RELEASE       DESTINATION ${INSTALL_RELEASE_PATH})

	# Copy ui
	install(DIRECTORY   ${CMAKE_SOURCE_DIR}/deploy/ui
			DESTINATION ${INSTALL_RELEASE_PATH})

	# Copy AppSDK
	if(HOUDINI_QT_VERSION VERSION_EQUAL 4)
		set(EXCLUDE_LIBVRAY "*${CMAKE_SHARED_LIBRARY_PREFIX}vray${CMAKE_SHARED_LIBRARY_SUFFIX}")
	else()
		set(EXCLUDE_LIBVRAY "*${CMAKE_SHARED_LIBRARY_PREFIX}vray_qt4${CMAKE_SHARED_LIBRARY_SUFFIX}")
	endif()

	install(DIRECTORY ${APPSDK_ROOT}/bin
			DESTINATION ${INSTALL_RELEASE_PATH}/appsdk
			PATTERN ${EXCLUDE_LIBVRAY} EXCLUDE
	)

	# Default libvray.dll is Qt5
	if(HOUDINI_QT_VERSION VERSION_EQUAL 4)
		set(VRAYDSO_QT4 ${APPSDK_ROOT}/bin/${CMAKE_SHARED_LIBRARY_PREFIX}vray_qt4${CMAKE_SHARED_LIBRARY_SUFFIX})
		# If doesn't exist - it's already Qt 4
		if(EXISTS ${VRAYDSO_QT4})
			install(FILES ${VRAYDSO_QT4}
				DESTINATION
					${INSTALL_RELEASE_PATH}/appsdk/bin
				RENAME
					${CMAKE_SHARED_LIBRARY_PREFIX}vray${CMAKE_SHARED_LIBRARY_SUFFIX}
			)
		endif()
	endif()

	# Clone JSON plugin descriptions
	set(VFH_CLONE_PLUGINS_DESC ${CMAKE_BINARY_DIR}/vfh_clone_plugins_desc.cmake)
	configure_file(${CMAKE_SOURCE_DIR}/cmake/vfh_clone_plugins_desc.cmake.in ${VFH_CLONE_PLUGINS_DESC} @ONLY)
	install(SCRIPT ${VFH_CLONE_PLUGINS_DESC})

	# Archive the installation directory
	set(VFH_PACK ${CMAKE_BINARY_DIR}/vfh_pack.cmake)
	configure_file(${CMAKE_SOURCE_DIR}/cmake/vfh_pack.cmake.in ${VFH_PACK})
	install(SCRIPT ${VFH_PACK})

else()
	cgr_install_runtime(${PROJECT_NAME} ${VFH_DSO_DIRPATH})

	if(WIN32)
		set(TEMPLATE_FILENAME "hfs.local.bat.in")
	elseif(APPLE)
		set(TEMPLATE_FILENAME "hfs.local.osx.sh.in")
	else()
		set(TEMPLATE_FILENAME "hfs.local.sh.in")
	endif()

	file(TO_NATIVE_PATH ${VFH_DSO_DIRPATH} VFH_DSO_DIRPATH)
	file(TO_NATIVE_PATH ${VFH_BIN_DIRPATH} VFH_BIN_DIRPATH)

	vfh_generate_launcher(BATCH TEMPLATE_FILENAME ${TEMPLATE_FILENAME})
	vfh_generate_launcher(      TEMPLATE_FILENAME ${TEMPLATE_FILENAME})
endif()
