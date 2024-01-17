cmake_minimum_required (VERSION 3.8)

project ("vkmmc")
include(CommonMacros.txt)


find_package(Vulkan REQUIRED)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set (VKMMC_SHADERS_DIRECTORY "${PROJECT_SOURCE_DIR}/assets/shaders")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(thirdparty)
add_subdirectory(code)
add_subdirectory(test)



find_program(GLSL_VALIDATOR 
    glslangValidator HINTS /usr/bin /usr/local/bin 
    $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/
    )


file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${VKMMC_SHADERS_DIRECTORY}/*.frag"
    "${VKMMC_SHADERS_DIRECTORY}/*.vert"
    "${VKMMC_SHADERS_DIRECTORY}/*.comp"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${VKMMC_SHADERS_DIRECTORY}/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )
set_property(TARGET Shaders PROPERTY FOLDER "Assets")
