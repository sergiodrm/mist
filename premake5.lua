--premake5.lua

include "Dependencies.lua"

includes["mist"] = "%{wks.location}/engine"

outputdir = "%{wks.location}/bin/%{cfg.buildcfg}"
temporaldir = "%{wks.location}/temp/%{cfg.buildcfg}_%{cfg.architecture}"
libdir = "%{wks.location}/lib/%{cfg.buildcfg}"

newaction {
    trigger = "clean",
    description = "clear project files",
    execute = function()
        os.execute("del /q /s *.vcxproj*")
        os.execute("del /q /s *.sln")
        os.execute("del /q /s bin")
        os.execute("del /q /s temp")
        os.execute("rmdir bin")
        os.execute("rmdir temp")
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
        optimize "Off"
    
    filter "configurations:Release"
        defines {"_NDEBUG"}
        symbols "On"
        optimize "On"

    -- deactive filter
    filter{}

    group "ThirdParty"
        include "thirdparty"
        -- project "YamlCpp"
        -- kind "StaticLib"
        -- language "C++"
        -- cppdialect "C++20"
        
        -- targetdir "%{outputdir}"
        --     objdir "%{temporaldir}"
        --     location "%{wks.location}/code"
        --     targetname "YamlCpp"
            
        --     defines { "YAML_CPP_STATIC_DEFINE" }
        --     files {
        --         "thirdparty/yaml-cpp/include/**.h",
        --         "thirdparty/yaml-cpp/src/**.h",
        --         "thirdparty/yaml-cpp/src/**.cpp",
        --     }
        --     includedirs {
        --         "%{includes.yaml}",
        --     }

    
        
    group "Engine"
    project "MistEngine"
        kind "StaticLib"
        language "C++"
        cppdialect "C++20"

        targetdir "%{outputdir}"
        targetname "Mist"
        objdir "%{temporaldir}"
        location "%{wks.location}/engine"

        defines { "MIST_VULKAN", "YAML_CPP_STATIC_DEFINE" }
        files { 
            "engine/**.h", "engine/**.cpp",
        }

        includedirs {
            "%{includes.mist}",
            "%{includes.generic}",
            "%{includes.glm}",
            "%{includes.gltf}",
            "%{includes.imgui}",
            "%{includes.imgui}/backends",
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
            "YamlCpp",
            "%{libs.vulkan}",
            "%{libs.sdl}",
        }
        
        filter "configurations:Debug"
        links {
                "%{libs.spirvd}",
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
            targetsuffix "d"
            
        filter "configurations:Release"
        links {
                "%{libs.spirv}",
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

        defines { "MIST_VULKAN", "YAML_CPP_STATIC_DEFINE" }
        includedirs {
            "test/",
            "%{includes.mist}",
            "%{includes.glm}",
            "%{includes.generic}",
            "%{includes.vulkan}",
            "%{includes.vma}",
            "%{includes.vkbootstrap}",
            "%{includes.sdl}",
            "%{includes.stbimage}",
            "%{includes.imgui}/backends",
        }

        filter "configurations:Debug"
        targetname "MistTest_dbg"
        filter "configurations:Release"
        targetname "MistTest"


