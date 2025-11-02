
set(THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/source/thirdparty")
set(SDL2_LIBRARY "$ENV{VULKAN_SDK}/Lib/SDL2.lib")


list(APPEND SPIRV_CROSS_OPT_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-c.lib")
list(APPEND SPIRV_CROSS_OPT_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-core.lib")
list(APPEND SPIRV_CROSS_OPT_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-cpp.lib")
list(APPEND SPIRV_CROSS_OPT_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-c-shared.lib")
list(APPEND SPIRV_CROSS_OPT_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-glsl.lib")
list(APPEND SPIRV_CROSS_OPT_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-hlsl.lib")
list(APPEND SPIRV_CROSS_OPT_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-msl.lib")
list(APPEND SPIRV_CROSS_OPT_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-reflect.lib")
list(APPEND SPIRV_CROSS_OPT_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-util.lib")

list(APPEND SPIRV_CROSS_DEBUG_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-cd.lib")
list(APPEND SPIRV_CROSS_DEBUG_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-cored.lib")
list(APPEND SPIRV_CROSS_DEBUG_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-cppd.lib")
list(APPEND SPIRV_CROSS_DEBUG_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-c-sharedd.lib")
list(APPEND SPIRV_CROSS_DEBUG_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-glsld.lib")
list(APPEND SPIRV_CROSS_DEBUG_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-hlsld.lib")
list(APPEND SPIRV_CROSS_DEBUG_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-msld.lib")
list(APPEND SPIRV_CROSS_DEBUG_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-reflectd.lib")
list(APPEND SPIRV_CROSS_DEBUG_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-utild.lib")

#file(GLOB SPIRV_CROSS_LIBS "$ENV{VULKAN_SDK}/Lib/spirv-cross-*d.lib")

file(GLOB ENGINE_SOURCES */*.cpp */*.h */*/*.cpp */*/*.h)

# compile imgui sources too
file(GLOB IMGUI_SOURCES 
	${THIRDPARTY_DIR}/imgui/*.h 
	${THIRDPARTY_DIR}/imgui/*.cpp
	${THIRDPARTY_DIR}/imgui/backends/imgui_impl_sdl2.h
	${THIRDPARTY_DIR}/imgui/backends/imgui_impl_sdl2.cpp
	${THIRDPARTY_DIR}/imgui/backends/imgui_impl_vulkan.h
	${THIRDPARTY_DIR}/imgui/backends/imgui_impl_vulkan.cpp
	)

#compile vkbootstrap
file(GLOB VKBOOTSTRAP_SOURCES
	${THIRDPARTY_DIR}/vkbootstrap/*.h
	${THIRDPARTY_DIR}/vkbootstrap/*.cpp
	)

# fetch yamp-cpp
include(FetchContent)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG a83cd31
)
FetchContent_MakeAvailable(yaml-cpp)

option(RBE_VK "Use Vulkan API" ON)

configure_file(version.h.in version.h)

include_directories(${THIRDPARTY_DIR}/yaml-cpp)


add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)
add_library(mist_engine ${ENGINE_SOURCES} ${IMGUI_SOURCES} ${VKBOOTSTRAP_SOURCES})
set_target_properties(mist_engine PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET mist_engine PROPERTY CXX_STANDARD 20)

if (!Vulkan_SPIRV-Tools_FOUND)
	message("Vulkan spirv-tools not found")
endif(!Vulkan_SPIRV-Tools_FOUND)

target_link_libraries(mist_engine 
	PUBLIC 
		${SDL2_LIBRARY} 
		Vulkan::Vulkan
		Vulkan::SPIRV-Tools
		Vulkan::shaderc_combined
		cppcoda_lib
		yaml-cpp::yaml-cpp
	debug ${SPIRV_CROSS_DEBUG_LIBS}
	optimized ${SPIRV_CROSS_OPT_LIBS}
)

message("Vulkan directory -> $ENV{VULKAN_SDK}")
message("Linking vulkan lib ->${Vulkan_LIBRARY}")
message("Linking sdl2 lib ->${SDL2_LIBRARY}")
message("Using vulkan include dir -> ${Vulkan_INCLUDE_DIRS}")
message("spirv-cross opt -> ${SPIRV_CROSS_OPT_LIBS}")
message("spirv-cross debug -> ${SPIRV_CROSS_DEBUG_LIBS}")

target_include_directories(mist_engine
		PUBLIC 
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR}
		${Vulkan_INCLUDE_DIRS}
		${Vulkan_INCLUDE_DIRS}/SDL2
		"${THIRDPARTY_DIR}/cppcoda"
		"${THIRDPARTY_DIR}/imgui"
		"${THIRDPARTY_DIR}/yaml-cpp/include"
		"${THIRDPARTY_DIR}"
		)
