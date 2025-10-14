# Mist Engine
Mist Engine is a project used to learn graphics programming and develop low level game engine features from scratch. The target of this project is improve and to create a space to trying out ideas and learning in the process.

Mist Engine is developed in C++ and Vulkan currently, but the idea is abstract the render layer and use DX12 as well, or any render api.

<!-- GIFS -->
<!-- ![img](/docs/01_noImGui.png) -->
![gif](/docs/demo_intro.gif)

## 🚀 Features
Nowadays, the program implements the following features:
* Deferred rendering.
* SSAO.
* Shadow mapping.
* Bloom post process.
* Scene management.
* Scene load and save from/to file.
* PBR.
* GLTF model loading.
* Host memory tracker.
* Gpu memory tracker.
* Shader reflection.
* Shader include libraries.
* Hot shader reload.
* Compute shaders.
* CPU and GPU profiling.
* Tags for external GPU profiling tools (NSight, RenderDoc, PIX...).
* Debug render primitives.
* CVar system.
* Cfg files.
* Dear ImGui integration.

## 🛠️ Requirements
* Vulkan SDK (currently develop in 1.4.313.1 version).

## 📦 Build and run

### Clone repository
This repository uses submodules to refer to another repositories. Use the next line to clone the repository with all the submodules:

```bash
git clone --recurse-submodules https://github.com/sergiodrm/mist.git
```

Or, if you already have the cloned repository, run this line to initialize and update submodules:

```bash
git submodule update --init --recursive
```

### Generate VS project
Execute `GenerateProject.bat` to create a sln file for Visual Studio 2022. If you use another version, edit bat file and change `vs2022` for your current version. 

Then, the solution `Mist.sln` should be created on the root of the repository with the right configuration to build the project on Release and Debug modes.

### First steps
Mist Engine uses its own implementation of CVars (like Doom) to set up and configure the engine. These variables can be set by command line args, in runtime by the console in-engine and with the `*.cfg` file. Some of them only can be set by cmd, like `workspace`. By default, the engine will find `default.cfg` on the root of the workspace folder. Here is an example of a cmd line to launch the engine:

```bash
Mist.exe -workspace:../assets/ -windowWidth:1920 -windowHeight:1080
```

> [!NOTE]
> CVar system are case insensitive.
> Execute `cvarlist` on the console to see the list of all the CVar's.
> Execute `cmdlist` on the console to see all the commands.

By the default the `workspace` is set to `../assets/` folder, so the project setup is ready to be executed without set the `workspace`. All the files that the engine will read or write must be in this directory. All paths are relative to this root.

The program can read a configuration file (`.cfg`) to set the CVars. The CVar system is implemented to configure several aspects of the engine behavior, such as initial resolution, show debug information, enable/disable some rendering techniques, etc... We can change this CVars before launch by cmd line (for example `-workspace`) or in a `.cfg` file (see `./assets/default.cfg`). Besides, we can also set CVars in runtime by the engine console or with the CVar ImGui window. To open the runtime console, press `º` key.

On launch, the engine will load a scene file specified by `g_LoadScene` CVar. This is a `.yaml` file with the scene hierarchy, gltf models and more info (see `./assets/scenes/scene.yaml`).

One powerfull feature is hot shader reloading. To reload shaders in runtime we can use the console command `r_reloadshaders` or the shortcut `Ctrl+R`.


### Latest update
* IBL.
* RenderSystem layer. Renderers or the render processes don't need to know about render pipelines or handle the render state. Now the top-level render engine is more OpenGL like.
* Render API layer abstraction used by RenderSystem.
* Compute shaders support by new render api layer and render system layer.

## Samples
![img](/docs/ssao.gif)
![img](/docs/02_SceneEditor.png)
<!--![img](/docs/03_ShadowMapDebug.png)-->
![img](/docs/shadow_mapping.gif)
![img](/docs/04_ProfilingAndBloom.png)
![img](/docs/05_GPUParticles.png)
![img](/docs/06_NSight.png)

## Render frame graph
![img](/docs/Frame.jpg)

## Future work
Here are some features that I will develop from now on:
* Forward lighting+.
* Forward lighting pipeline for alpha materials.
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
* glm (included in Vulkan SDK) (https://github.com/g-truc/glm).
* SDL2 (included in Vulkan SDK) (https://www.libsdl.org/).
* shaderc (included in Vulkan SDK) (https://github.com/google/shaderc).
* vma (included in Vulkan SDK) (https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator).
* yaml-cpp (https://github.com/jbeder/yaml-cpp).
* imgui (https://github.com/ocornut/imgui).
* cgltf (https://github.com/jkuhlmann/cgltf).
* stb_image (https://github.com/nothings/stb).
* vkbootstrap (https://github.com/charles-lunarg/vk-bootstrap).
* premake (https://premake.github.io/).