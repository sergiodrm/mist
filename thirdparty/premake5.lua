
dependenciesTargetDir = "%{wks.location}/thirdparty/lib"

--include "SDL/SDL2.lua"

project "glm"
    kind "StaticLib"
    files { "glm/**.h", "glm/**.cpp" }
    includedirs { "%{includes.glm}" }
    targetdir "%{dependenciesTargetDir}"

project "ImGui"
    kind "StaticLib"
    files { "imgui/**.h", "imgui/**.cpp" }
    includedirs { "%{includes.vulkan}", "%{includes.sdl}"}
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
