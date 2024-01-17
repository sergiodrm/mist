# CMAKELISTS for third party libraries

find_package(Vulkan REQUIRED)


add_library(glm INTERFACE)
add_library(vma INTERFACE)
add_library(stb_image INTERFACE)
#add_library(SPIRV-Cross INTERFACE)
add_library(imgui STATIC)
add_library(vkbootstrap STATIC)

add_subdirectory(SDL EXCLUDE_FROM_ALL)
add_subdirectory(spirv-cross-main)

target_include_directories(stb_image INTERFACE stb_image)
target_include_directories(imgui PUBLIC imgui)
target_include_directories(vma INTERFACE vma)
target_include_directories(glm INTERFACE glm)
target_include_directories(vkbootstrap INTERFACE vkbootstrap)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL2::SDL2)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan)

target_sources(imgui PRIVATE
	imgui/imgui.h
	imgui/imgui.cpp
	imgui/imgui_demo.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_widgets.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_impl_vulkan.cpp
	imgui/imgui_impl_sdl2.cpp
)

target_sources(vkbootstrap PRIVATE
	vkbootstrap/vkbootstrap.h
	vkbootstrap/vkbootstrap.cpp
)

set_property(TARGET imgui PROPERTY FOLDER "ThirdParty")
set_property(TARGET vkbootstrap PROPERTY FOLDER "ThirdParty")
set_property(TARGET sdl_headers_copy PROPERTY FOLDER "ThirdParty")
set_property(TARGET SDL2-static PROPERTY FOLDER "ThirdParty")
set_property(TARGET spirv-cross PROPERTY FOLDER "ThirdParty")
set_property(TARGET spirv-cross-c PROPERTY FOLDER "ThirdParty")
set_property(TARGET spirv-cross-cpp PROPERTY FOLDER "ThirdParty")
set_property(TARGET spirv-cross-core PROPERTY FOLDER "ThirdParty")
set_property(TARGET spirv-cross-glsl PROPERTY FOLDER "ThirdParty")
set_property(TARGET spirv-cross-hlsl PROPERTY FOLDER "ThirdParty")
set_property(TARGET spirv-cross-msl PROPERTY FOLDER "ThirdParty")
set_property(TARGET spirv-cross-reflect PROPERTY FOLDER "ThirdParty")
set_property(TARGET spirv-cross-util PROPERTY FOLDER "ThirdParty")

