cmake_minimum_required(VERSION 4.1)

# main project
project(MistEngine LANGUAGES CXX VERSION 1.0)

find_package(Vulkan REQUIRED COMPONENTS SPIRV-Tools shaderc_combined)

# Set cpp standard
set(CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED True)

add_subdirectory(source/thirdparty)
add_subdirectory(source/engine)
add_subdirectory(source/test)

set_target_properties(mist_engine PROPERTIES FOLDER "Engine")
set_target_properties(sandbox PROPERTIES FOLDER "Sandbox")

