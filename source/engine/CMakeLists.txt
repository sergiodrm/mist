
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

list(APPEND ENGINE_GENERIC_LIBS ${SDL2_LIBRARY})
list(APPEND ENGINE_GENERIC_LIBS Vulkan::Vulkan)
list(APPEND ENGINE_GENERIC_LIBS Vulkan::SPIRV-Tools)
list(APPEND ENGINE_GENERIC_LIBS Vulkan::shaderc_combined)
list(APPEND ENGINE_GENERIC_LIBS cppcoda_lib)
list(APPEND ENGINE_GENERIC_LIBS yaml-cpp::yaml-cpp)
list(APPEND ENGINE_GENERIC_LIBS Tracy::TracyClient)

list(APPEND ENGINE_DEBUG_LIBS ${SPIRV_CROSS_DEBUG_LIBS})

list(APPEND ENGINE_RELEASE_LIBS ${SPIRV_CROSS_OPT_LIBS})

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


option(RBE_VK "Use Vulkan API" ON)

configure_file(version.h.in version.h)

if (RBE_VK)
	add_compile_definitions(RBE_VK)
endif(RBE_VK)
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)
add_library(mist_engine ${ENGINE_SOURCES} ${IMGUI_SOURCES} ${VKBOOTSTRAP_SOURCES})
set_target_properties(mist_engine PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET mist_engine PROPERTY CXX_STANDARD 20)

if (!Vulkan_SPIRV-Tools_FOUND)
	message("Vulkan spirv-tools not found")
endif(!Vulkan_SPIRV-Tools_FOUND)

target_link_libraries(mist_engine PUBLIC ${ENGINE_GENERIC_LIBS})

# Link debug libs
foreach (entry ${ENGINE_DEBUG_LIBS})
	target_link_libraries(mist_engine PUBLIC debug ${entry})
endforeach()

# Link release libs
foreach (entry ${ENGINE_RELEASE_LIBS})
	target_link_libraries(mist_engine PUBLIC optimized ${entry})
endforeach()

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
