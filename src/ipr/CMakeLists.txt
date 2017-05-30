project(vray_for_houdini_ipr)

add_executable(${PROJECT_NAME} main.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME vfh_ipr)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "C:/programs/bin")
