
-- dependencies

VulkanSdk = os.getenv("VULKAN_SDK") -- from PATH

includes = {}
includes["generic"] = "%{wks.location}/source/thirdparty"
includes["glm"] = "%{wks.location}/source/thirdparty/glm"
includes["gltf"] = "%{wks.location}/source/thirdparty/gltf"
includes["imgui"] = "%{wks.location}/source/thirdparty/imgui"
includes["sdl"] = "%{VulkanSdk}/Include/SDL2"
includes["stbimage"] = "%{wks.location}/source/thirdparty/stb_image"
includes["vkbootstrap"] = "%{wks.location}/source/thirdparty/vkbootstrap"
includes["vma"] = "%{wks.location}/source/thirdparty/vma"
includes["vulkan"] = "%{VulkanSdk}/Include"
includes["yaml"] = "%{wks.location}/source/thirdparty/yaml-cpp/include"
includes["cppcoda"] = "%{wks.location}/source/thirdparty/cppcoda/cppcoda"

includes["mist"] = "%{wks.location}/source/engine"

libdirs = {}
libdirs["vulkan"] = "%{VulkanSdk}/lib"

libs = {}
libs["vulkan"] = "%{libdirs.vulkan}/vulkan-1.lib"
libs["sdl"] = "%{libdirs.vulkan}/SDL2.lib"
libs["spirv"] = "%{libdirs.vulkan}/SPIRV.lib"
libs["spirvd"] = "%{libdirs.vulkan}/SPIRVd.lib"
libs["spirvcross"] = "%{libdirs.vulkan}/spirv-cross-c.lib"
libs["spirvcrossd"] = "%{libdirs.vulkan}/spirv-cross-cd.lib"
libs["spirvcrosscore"] = "%{libdirs.vulkan}/spirv-cross-core.lib"
libs["spirvcrosscored"] = "%{libdirs.vulkan}/spirv-cross-cored.lib"
libs["spirvcrosscpp"] = "%{libdirs.vulkan}/spirv-cross-cpp.lib"
libs["spirvcrosscppd"] = "%{libdirs.vulkan}/spirv-cross-cppd.lib"
libs["spirvcrossglsl"] = "%{libdirs.vulkan}/spirv-cross-glsl.lib"
libs["spirvcrossglsld"] = "%{libdirs.vulkan}/spirv-cross-glsld.lib"
libs["spirvcrossreflect"] = "%{libdirs.vulkan}/spirv-cross-reflect.lib"
libs["spirvcrossreflectd"] = "%{libdirs.vulkan}/spirv-cross-reflectd.lib"
libs["spirvcrosscshared"] = "%{libdirs.vulkan}/spirv-cross-c-shared.lib"
libs["spirvcrosscsharedd"] = "%{libdirs.vulkan}/spirv-cross-c-sharedd.lib"
libs["spirvcrosshlsl"] = "%{libdirs.vulkan}/spirv-cross-hlsl.lib"
libs["spirvcrosshlsld"] = "%{libdirs.vulkan}/spirv-cross-hlsld.lib"
libs["spirvcrossmsl"] = "%{libdirs.vulkan}/spirv-cross-msl.lib"
libs["spirvcrossmsld"] = "%{libdirs.vulkan}/spirv-cross-msld.lib"
libs["spirvcrossutil"] = "%{libdirs.vulkan}/spirv-cross-util.lib"
libs["spirvcrossutild"] = "%{libdirs.vulkan}/spirv-cross-utild.lib"
libs["glslang"] = "%{libdirs.vulkan}/glslang.lib"
libs["glslangd"] = "%{libdirs.vulkan}/glslangd.lib"
libs["shaderc"] = "%{libdirs.vulkan}/shaderc_shared.lib"
libs["shadercd"] = "%{libdirs.vulkan}/shaderc_sharedd.lib"
libs["shadercutil"] = "%{libdirs.vulkan}/shaderc_util.lib"
libs["shadercutild"] = "%{libdirs.vulkan}/shaderc_utild.lib"


