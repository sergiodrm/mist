--premake5.lua

include "Dependencies.lua"

includes["mist"] = "%{wks.location}/code/include"
includes["mistsrc"] = "%{wks.location}/code/src"

outputdir = "%{wks.location}/bin/"
temporaldir = "%{wks.location}/temp/%{cfg.buildcfg}_%{cfg.architecture}"

newaction {
    trigger = "clean",
    description = "clear project files",
    execute = function()
        os.execute("del /s *.vcxproj*")
        os.execute("del /s *.sln")
    end
}

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
        objdir "%{temporaldir}"
        location "%{wks.location}/code"

        defines { "MIST_VULKAN", "YAML_CPP_STATIC_DEFINE" }
        files { 
            "code/**.h", "code/**.cpp",
            "thirdparty/yaml-cpp/include/**.h",
            "thirdparty/yaml-cpp/src/**.h",
            "thirdparty/yaml-cpp/src/**.cpp",
        }

        includedirs {
            "%{includes.mist}/mist",
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
            "%{includes.yaml}",
        }
        links {
            "glm",
            "ImGui",
            "VkBootstrap",
            --"YamlCpp",
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
                "%{libs.glslangd}",
                "%{libs.shadercd}",
                "%{libs.shadercutild}",
            }
            targetname "Mist_dbg"
            
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
                "%{libs.glslang}",
                "%{libs.shaderc}",
                "%{libs.shadercutil}",
            }
            targetname "Mist"
        
    
    group "Test"
    project "MistTest"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++20"
        
        targetdir "%{outputdir}"
        objdir "%{temporaldir}"
        location "%{wks.location}/test"
        
        links { "MistEngine" }
        files { "test/**.h", "test/**.cpp"}

        includedirs {
            "%{includes.mist}",
            "%{wks.location}/code/include/mist",
            "%{includes.glm}",
            "%{includes.generic}",
        }

        filter "configurations:Debug"
        targetname "MistTest_dbg"
        filter "configurations:Release"
        targetname "MistTest"


