# Mist Engine
Mist Engine is a render engine made from scratch. The target of this project is to learn graphics programming and develop low level engine features, just for fun and learn. The project uses Vulkan API, SDL, ImGui, glm and others dependencies.

![img](/docs/01_noImGui.png)

## Build
Execute `GenerateProject.bat` to create a sln file for Visual Studio 2022. If you use another version, edit bat file and change `vs2022` for your current version. After that, build and run!

## Features
Nowadays, the engine implements the following features:
* Deferred rendering.
![img](/docs/02_SceneEditor.png)
* SSAO.
* Shadow mapping.
![img](/docs/03_ShadowMapDebug.png)
* Bloom post process.
![img](/docs/04_ProfilingAndBloom.png)
* Scene management.
* Scene load and save from/to file.
* PBR.
* GLTF model loading.
* System memory tracker.
* Gpu memory tracker.
* Shader reflection.
* Shader include libraries.
* Hot shader reload.
* Compute shaders.
![img](/docs/05_GPUParticles.png)
* CPU and GPU profiling.
* Tags for external GPU profiling tools (NSight, RenderDoc, PIX...).
![img](/docs/06_NSight.png)
* Debug render primitives.
* CVar system.
* Cfg files.

Latest features:
* IBL.
* RenderSystem layer. Renderers or the render processes don't need to know about render pipelines or handle the render state. Now the top-level render engine is more OpenGL like.
* Render API layer abstraction used by RenderSystem.
* Compute shaders support by new render api layer and render system layer.


## First steps
First of all, we can change the `workspace` variable to point to our assets directory by the cmd line. 
`./bin/Release/Mist.exe -workspace:../../assets/`
By the default the `workspace` is set to `../assets/` folder.
All the files that the engine will read or write must be in this directory. All paths are relative to this root.
The program can read a configuration file (.cfg) to set the CVars. The CVar system is implemented to configure several aspects of the engine behavior, such as initial resolution, show debug information, enable/disable some rendering techniques, etc... We can change this CVars before launch by cmd line (for example `-workspace`) or in a `.cfg` file (see `./assets/default.cfg`). Besides, we can also set CVars in runtime by the engine console or with the CVar ImGui window. To open the runtime console, press `º` key.
On launch, the engine will load a scene file specified by `g_LoadScene` CVar. This is a `.yaml` file with the scene hierarchy, gltf models and more info (see `./assets/scenes/scene.yaml`).
One powerfull feature is hot shader reloading. To reload shaders in runtime we can use the console command `r_reloadshaders` or the shortcut `Ctrl+R`.
To see all the CVars introduce this command in the console: `cvarlist`
To see all the command functions, run this: `cmdlist`.

## Render frame graph
![img](/docs/Frame.jpg)

## Future work
Here are some features that I will develop from now on:
* Forward lighting+.
* Material system.
* DX12 backend.
* HBAO.
* SSR.
* Skinning animation.
* Capsule shadows.
* Cascaded shadows.
* Mesh LODs.
* CPU culling.
* GPU culling.
* Job system.
* Instance meshes.
* Tesselation.
* Terrains.
* Use custom data containers instead of STL.
* CMake instead of premake to generate the project.
* CTest.
* ...

## Dependencies
* Vulkan API (https://www.vulkan.org/).
* glm (included in vulkan).
* SDL2 (included in vulkan).
* shaderc (included in vulkan).
* vma (https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator).
* yaml-cpp (https://github.com/jbeder/yaml-cpp).
* imgui (https://github.com/ocornut/imgui).
* cgltf (https://github.com/jkuhlmann/cgltf).
* stb_image (https://github.com/nothings/stb).
* vkbootstrap (https://github.com/charles-lunarg/vk-bootstrap).
* premake (https://premake.github.io/).