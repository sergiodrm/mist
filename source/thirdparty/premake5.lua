
dependenciesTargetDir = "%{libdir}"

--include "SDL/SDL2.lua"

project "glm"
    kind "StaticLib"
    files { "glm/**.h", "glm/**.cpp" }
    includedirs { "%{includes.glm}" }
    targetdir "%{dependenciesTargetDir}"

project "ImGui"
    kind "StaticLib"
    files { 
        "imgui/*.h", "imgui/*.cpp",
        "imgui/backends/imgui_impl_vulkan.h", "imgui/backends/imgui_impl_vulkan.cpp",
        "imgui/backends/imgui_impl_sdl2.h", "imgui/backends/imgui_impl_sdl2.cpp",
    }
    includedirs { "%{includes.vulkan}", "%{includes.sdl}", "%{includes.imgui}"}
    targetdir "%{dependenciesTargetDir}"

project "VkBootstrap"
    kind "StaticLib"
    files { "vkbootstrap/**.h", "vkbootstrap/**.cpp" }
    includedirs { "%{includes.vkbootstrap}", "%{includes.vulkan}" }
    targetdir "%{dependenciesTargetDir}"

project "YamlCpp"
    kind "StaticLib"
    cppdialect "C++20"
    files { "yaml-cpp/include/**.h", "yaml-cpp/src/**.h", "yaml-cpp/src/**.cpp"}
    includedirs { "%{includes.yaml}" }
    targetdir "%{dependenciesTargetDir}"
    defines { "YAML_CPP_STATIC_DEFINE" }
    
project "cppcoda"
    kind "StaticLib"
    cppdialect "C++20"
    files { "cppcoda/cppcoda/**.h", "cppcoda/cppcoda/**.cpp" }
    targetdir "%{dependenciesTargetDir}"

project "tracy"
    kind "StaticLib"
    cppdialect "C++20"
    files { 
        "tracy/public/common/**.h", "tracy/public/common/**.cpp",  "tracy/public/common/**.hpp",
        "tracy/public/client/**.h", "tracy/public/client/**.cpp", "tracy/public/client/**.hpp",
        "tracy/public/tracy/**.h", "tracy/public/tracy/**.cpp", "tracy/public/tracy/**.hpp",
        "tracy/public/TracyClient.cpp"
    }
    includedirs { "%{includes.tracy}" }
    targetdir "%{dependenciesTargetDir}"
    defines { "TRACY_ENABLE", "TRACY_ON_DEMAND" }

