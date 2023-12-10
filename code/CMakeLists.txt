# CMAKELISTS for code

add_library(vkmmc)

target_sources(vkmmc
	PRIVATE

	"${CMAKE_CURRENT_SOURCE_DIR}/include/vkmmc/RenderEngine.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/vkmmc/RenderHandle.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/vkmmc/Vertex.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/vkmmc/Mesh.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/vkmmc/Camera.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/vkmmc/RenderObject.h"
	
	
	
	"${CMAKE_CURRENT_SOURCE_DIR}/src/RenderObject.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Camera.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/RenderDescriptor.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/RenderDescriptor.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/VertexInputDescription.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/VertexInputDescription.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Mesh.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Vertex.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/RenderEngine.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/FunctionStack.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/VulkanRenderEngine.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/VulkanRenderEngine.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Swapchain.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Swapchain.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/RenderTypes.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/RenderTypes.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/InitVulkanTypes.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/InitVulkanTypes.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/RenderPass.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/RenderPass.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/RenderPipeline.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/RenderPipeline.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/GenericUtils.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/GenericUtils.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Logger.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Logger.cpp"
)

set_property(TARGET vkmmc PROPERTY CXX_STANDARD 20)
target_compile_definitions(vkmmc PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(vkmmc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(vkmmc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/vkmmc")
target_include_directories(vkmmc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(vkmmc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty")

target_link_libraries(vkmmc PUBLIC
	vma
	glm
	Vulkan::Vulkan
	stb_image
	imgui
	SDL2::SDL2
	vkbootstrap
)

#add_custom_command(
#	TARGET vkmmc POST_BUILD
#	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:vkmmc> $<TARGET_FILE_DIR:vkmmc>
#	COMMAND_EXPAND_LISTS
#)

