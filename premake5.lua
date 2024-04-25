--premake5.lua

include "Dependencies.lua"

includes["mist"] = "%{wks.location}/code/include"
includes["mistsrc"] = "%{wks.location}/code/src"

outputdir = "%{wks.location}/bin/%{cfg.buildcfg}_%{cfg.architecture}"

workspace "Mist"
    configurations {"Debug", "Release"}
    platforms {"Win64"}
    startproject "MistTest"
    flags {"MultiProcessorCompile"}

    -- filter config
    filter "platforms:Win64"
        system "Windows"
        architecture "x86_64"

    filter "configurations:Debug"
        defines {"_DEBUG"}
        symbols "On"
    
    filter "configurations:Release"
        defines {"_NDEBUG"}
        symbols "On"

    -- deactive filter
    filter{}

    group "ThirdParty"
    include "thirdparty"

    group "Engine"
    project "MistEngine"
        kind "StaticLib"
        language "C++"
        cppdialect "C++20"

        targetdir "%{outputdir}"

        defines { "VKMMC_VULKAN" }
        files { "code/**.h", "code/**.cpp"}

        includedirs {
            "%{includes.mist}/vkmmc",
            "%{includes.mistsrc}",
            "%{includes.generic}",
            "%{includes.glm}",
            "%{includes.gltf}",
            "%{includes.imgui}",
            "%{includes.sdl}",
            "%{includes.stbimage}",
            "%{includes.vma}",
            "%{includes.vkbootstrap}",
            "%{includes.vulkan}",
        }
        links {
            "glm",
            "ImGui",
            "VkBootstrap",
            "%{libs.vulkan}",
            "%{libs.sdl}",
        }
        
        filter "configurations:Debug"
        links {
                "%{libs.spirv}",
                "%{libs.spirvcrossd}",
                "%{libs.spirvcrosscppd}",
                "%{libs.spirvcrosscored}",
                "%{libs.spirvcrossglsld}",
                "%{libs.spirvcrossreflectd}",
                --"%{libs.spirvcrosscsharedd}",
                "%{libs.spirvcrosshlsld}",
                "%{libs.spirvcrossglsld}",
                "%{libs.spirvcrossmsld}",
                "%{libs.spirvcrossutild}",
            }
            
            filter "configurations:Release"
            links {
                "%{libs.spirvd}",
                "%{libs.spirvcross}",
                "%{libs.spirvcrosscore}",
                "%{libs.spirvcrosscpp}",
                "%{libs.spirvcrossglsl}",
                "%{libs.spirvcrossreflect}",
                --"%{libs.spirvcrosscshared}",
                "%{libs.spirvcrosshlsl}",
                "%{libs.spirvcrossglsl}",
                "%{libs.spirvcrossmsl}",
                "%{libs.spirvcrossutil}",
            }
        
    
    group "Test"
    project "MistTest"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++20"
        
        targetdir "%{outputdir}"
        
        links { "MistEngine" }
        files { "test/**.h", "test/**.cpp"}

        includedirs {
            "%{includes.mist}",
            "%{includes.glm}",
            "%{includes.generic}",
        }


