#include "Render/VulkanRenderEngine.h"

#include <cstdio>
#include <glm/gtx/transform.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <string.h>

#include <vkbootstrap/VkBootstrap.h>
#include "Render/InitVulkanTypes.h"
#include "Render/Mesh.h"
#include "Render/RenderDescriptor.h"
#include "Render/Camera.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl2.h>
#include "Core/Console.h"
#include "Core/Debug.h"
#include "Render/Shader.h"
#include "Render/Globals.h"
#include "Scene/Scene.h"
#include "Utils/GenericUtils.h"
#include "Render/RenderTarget.h"
#include "RenderProcesses/RenderProcess.h"
#include "Utils/TimeUtils.h"
#include "Application/CmdParser.h"
#include "Application/Application.h"
#include "Application/Event.h"
#include "Core/SystemMemory.h"
#include "RenderProfiling.h"
#include "Render/DebugRender.h"
#include "Render/CommandList.h"
#include "Render/UI.h"


#include "RenderAPI/Device.h"
#include "RenderAPI/ShaderCompiler.h"
#include "RenderAPI/Utils.h"
#include "RenderSystem/RenderSystem.h"
#include "RenderProcesses/ShadowMap.h"
#include "RenderSystem/UI.h"

#define UNIFORM_ID_SCREEN_QUAD_INDEX "ScreenQuadIndex"
#define MAX_RT_SCREEN 6

namespace Mist
{
	::render::Device* g_device = nullptr;
	::rendersystem::RenderSystem* g_render = nullptr;

	CameraData g_cameraData;

	CBoolVar CVar_EnableValidationLayer("r_enableValidationLayer", true);
	CBoolVar CVar_ExitValidationLayer("r_exitValidationLayer", true);
	CBoolVar CVar_ShowImGui("ShowImGui", true);

	extern CIntVar CVar_ShowCpuProf;

	namespace Debug
	{
		uint32_t GVulkanLayerValidationErrors = 0;

		VkBool32 DebugVulkanCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
			void* userData)
		{
			Mist::LogLevel level = Mist::LogLevel::Info;
			switch (severity)
			{
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: level = Mist::LogLevel::Error; ++GVulkanLayerValidationErrors; break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: level = Mist::LogLevel::Debug; break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: level = Mist::LogLevel::Warn; break;
			}
			Logf(level, "\nValidation layer\n> Message: %s\n\n", callbackData->pMessage);
			if (level == Mist::LogLevel::Error)
			{
				if (!CVar_ExitValidationLayer.Get())
					PrintCallstack();
				check(!CVar_ExitValidationLayer.Get() && "Validation layer error");
			}
			return VK_FALSE;
		}
	}

	CBoolVar CVar_ShowImGuiDemo("ShowImGuiDemo", false);

	void ExecCommand_ReloadShaders(const char* cmd)
	{
		VulkanRenderEngine* eng = IRenderEngine::GetRenderEngineAs<VulkanRenderEngine>();
		eng->ReloadShaders();
	}

	void ExecCommand_DumpShadersInfo(const char* cmd)
	{
		VulkanRenderEngine* eng = IRenderEngine::GetRenderEngineAs<VulkanRenderEngine>();
		eng->DumpShadersInfo();
	}

	void ExecCommand_ActiveCpuProf(const char* cmd)
	{
		if (CVar_ShowCpuProf.Get() == 0)
			CVar_ShowCpuProf.Set(3);
		if (CVar_ShowCpuProf.Get() == 1)
			CVar_ShowCpuProf.Set(2);
	}

	struct Quad
	{
		VertexBuffer VB;
		IndexBuffer IB;

		void Init(const RenderContext& context, float minx, float maxx, float miny, float maxy)
		{
			// Init vertexbuffer
			float vertices[] =
			{
				// vkscreencoords	// uvs
				minx, miny, 0.f,	0.f, 0.f,
				maxx, miny, 0.f,	1.f, 0.f,
				maxx, maxy, 0.f,	1.f, 1.f,
				minx, maxy, 0.f,	0.f, 1.f
			};
			BufferCreateInfo quadInfo;
			quadInfo.Data = vertices;
			quadInfo.Size = sizeof(vertices);
			VB.Init(context, quadInfo);

			uint32_t indices[] = { 0, 2, 1, 0, 3, 2 };
			quadInfo.Data = indices;
			quadInfo.Size = sizeof(indices);
			IB.Init(context, quadInfo);
		}

		void Draw(CommandList* commandList)
		{
			check(commandList);
			commandList->BindVertexBuffer(VB);
			commandList->BindIndexBuffer(IB);
			commandList->DrawIndexed(6, 1, 0, 0);
		}

		void Destroy(const RenderContext& context)
		{
			VB.Destroy(context);
			IB.Destroy(context);
		}
	} FullscreenQuad;

	void CmdDrawFullscreenQuad(CommandList* commandList)
	{
		FullscreenQuad.Draw(commandList);
	}

	cTexture* DefaultTexture = nullptr;
	cTexture* GetTextureCheckerboard4x4(const RenderContext& context)
	{
		if (!DefaultTexture)
		{
#if 0
			constexpr EFormat f = FORMAT_R8G8B8A8_SRGB;
			constexpr uint32_t w = 2;
			constexpr uint32_t h = 2;
			constexpr uint32_t d = 1;
			constexpr uint8_t pixels[] = {
				0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff,
				0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff
			};
			static_assert(utils::GetPixelSizeFromFormat(f) * w * h * d == sizeof(pixels));
			tImageDescription desc
			{
				.Format = f,
				.Width = w,
				.Height = h,
				.Depth = d,
			};
			DefaultTexture = cTexture::Create(context, desc);
			const uint8_t* layer = (uint8_t*)pixels;
			DefaultTexture->SetImageLayers(context, &layer, 1);
			DefaultTexture->CreateView(context, tViewDescription());
#else
			check(LoadTextureFromFile(context, "textures/checkerboard.jpg", &DefaultTexture, FORMAT_R8G8B8A8_UNORM));
			DefaultTexture->CreateView(context, tViewDescription());
#endif // 0
		}
		return DefaultTexture;
	}

	cMaterial* DefaultMaterial = nullptr;
	cMaterial* GetDefaultMaterial(const RenderContext& context)
	{
		if (!DefaultMaterial)
		{
			DefaultMaterial = _new cMaterial();
			DefaultMaterial->SetName("DefaultMaterial");
			DefaultMaterial->m_albedo = glm::vec3(1.f, 0.f, 1.f);
			DefaultMaterial->SetupShader(context);
		}
		return DefaultMaterial;
	}

	const CameraData* GetCameraData()
	{
		return &g_cameraData;
	}
	
	bool VulkanRenderEngine::Init(const Window& window)
	{
		CPU_PROFILE_SCOPE(Init);
#ifdef _DEBUG
		Log(LogLevel::Info, "Running app in DEBUG mode.\n");
#else
		Log(LogLevel::Info, "Running app in RELEASE mode.\n");
#endif // _DEBUG

		m_renderContext.Window = &window;
		SDL_Init(SDL_INIT_VIDEO);

#ifdef RENDER_BACKEND_TEST
		class WindowRenderInterface : public rendersystem::IWindow
		{
		public:
			const Window* window;
			virtual void* GetWindowHandle() const override { return const_cast<Window*>(window); }
			virtual void* GetWindowNative() const override { return const_cast<Window*>(window)->WindowInstance; }
		} windowInterface;
		windowInterface.window = &window;
		m_renderSystem = _new rendersystem::RenderSystem();
		m_renderSystem->Init(&windowInterface);

		g_render = m_renderSystem;
		g_device = m_renderSystem->GetDevice();
#else
		// Init vulkan context
		check(InitVulkan());

		// Descriptor layout and allocator
		m_descriptorLayoutCache.Init(m_renderContext);
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			m_descriptorAllocators[i].Init(m_renderContext, DescriptorPoolSizes::GetDefault());
			m_renderContext.FrameContextArray[i].DescriptorAllocator = &m_descriptorAllocators[i];
			m_renderContext.FrameContextArray[i].Renderer = &m_renderer;
			m_renderContext.FrameContextArray[i].StatusFlags = 0;
		}
		m_shaderDb.Init(m_renderContext);
		m_renderContext.LayoutCache = &m_descriptorLayoutCache;
		m_renderContext.DescAllocator = &m_descriptorAllocators[0];
		m_renderContext.ShaderDB = &m_shaderDb;

		m_renderContext.FrameIndex = 0;

		// Swapchain
		SwapchainInitializationSpec swapchainSpec;
		swapchainSpec.ImageWidth = window.Width;
		swapchainSpec.ImageHeight = window.Height;
		check(m_swapchain.Init(m_renderContext, swapchainSpec));

		// Commands
		check(InitCommands());
		// Pipelines
		check(InitPipeline());
		// Init sync vars
		check(InitSync());

        m_renderContext.Queue = _new CommandQueue(&m_renderContext, QUEUE_GRAPHICS | QUEUE_COMPUTE | QUEUE_TRANSFER);
        m_renderContext.CmdList = _new CommandList(&m_renderContext);

		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; i++)
		{
			m_renderContext.FrameContextArray[i].GraphicsTimestampQueryPool.Init(m_renderContext.Device, 40);
			m_renderContext.FrameContextArray[i].ComputeTimestampQueryPool.Init(m_renderContext.Device, 40);

			// 3 stage buffer per frame with 100 MB each one.
			m_renderContext.FrameContextArray[i].TempStageBuffer.Init(m_renderContext, 100 * 1024 * 1024, 3);
		}

		// Initialize render processes after instantiate render context
		m_renderer.Init(m_renderContext, m_renderContext.FrameContextArray, CountOf(m_renderContext.FrameContextArray), m_swapchain);
		DebugRender::Init(m_renderContext);
		ui::Init(m_renderContext, m_renderer.GetLDRTarget());
#if defined(MIST_CUBEMAP)
		m_cubemapPipeline.Init(m_renderContext, &m_renderer.GetLDRTarget());
#endif // defined(MIST_CUBEMAP)
		m_gpuParticleSystem.Init(m_renderContext);
		m_gpuParticleSystem.InitFrameData(m_renderContext, m_renderContext.FrameContextArray);
		m_gol = _new Gol(&m_renderContext);
        m_gol->Init(256, 256);
		FullscreenQuad.Init(m_renderContext, -1.f, 1.f, -1.f, 1.f);
#endif

		m_renderContext.Renderer = &m_renderer;
		m_renderer.Init(m_renderContext, m_renderContext.FrameContextArray, CountOf(m_renderContext.FrameContextArray), m_swapchain);
		DebugRender::Init();
		//////////////////////////////////////
		// Console commands
		//////////////////////////////////////
		AddConsoleCommand("r_reloadshaders", ExecCommand_ReloadShaders);
		AddConsoleCommand("r_dumpshadersinfo", ExecCommand_DumpShadersInfo);
		AddConsoleCommand("s_setcpuprof", ExecCommand_ActiveCpuProf);

		//////////////////////////////////////
		// ImGui callbacks
		//////////////////////////////////////
		rendersystem::ui::AddWindowCallback("AppInfo", [](void*) { Profiling::ImGuiDraw(); }, nullptr, true);
		rendersystem::ui::AddWindowCallback("InputState", &ImGuiDrawInputState);
		rendersystem::ui::AddWindowCallback("ImGuiDemo", [](void*) { ImGui::ShowDemoWindow(); });
		rendersystem::ui::AddWindowCallback("Gpu particles", [](void* data) 
			{
				check(data);
				GPUParticleSystem* ps = static_cast<GPUParticleSystem*>(data);
				ps->ImGuiDraw();
			}, &m_gpuParticleSystem);
		rendersystem::ui::AddWindowCallback("Game of life demo", [](void* data) 
			{
				check(data);
				Gol* g = static_cast<Gol*>(data);
				g->ImGuiDraw();
			}, &m_gol);

		for (uint32_t i = 0; i < m_renderer.GetRenderProcessCount(); ++i)
			rendersystem::ui::AddWindowCallback(RenderProcessNames[i], [](void* data) 
				{
					check(data);
					Mist::RenderProcess* rp = static_cast<Mist::RenderProcess*>(data);
					rp->ImGuiDraw();
				}, m_renderer.GetRenderProcess((RenderProcessType)i));

		return true;
	}

	bool VulkanRenderEngine::RenderProcess()
	{
		CPU_PROFILE_SCOPE(Process);
		if (m_scene)
		{
			m_scene->UpdateRenderData();
			ShadowMapProcess* shadowMap = static_cast<ShadowMapProcess*>(m_renderer.GetRenderProcess(RENDERPROCESS_SHADOWMAP));
			shadowMap->CollectLightData(*m_scene);
		}
		FlushPendingConsoleCommands();
		Draw();
		return true;
	}

	void VulkanRenderEngine::Shutdown()
	{
		loginfo("Shutdown render engine.\n");

#ifdef RENDER_BACKEND_TEST
		g_device->WaitIdle();
		if (m_scene)
		{
			m_scene->Destroy();
			delete m_scene;
			m_scene = nullptr;
		}
		DebugRender::Destroy();
		m_renderer.Destroy(m_renderContext);
		g_device = nullptr;
		g_render = nullptr;
		m_renderSystem->Destroy();
		delete m_renderSystem;
	}

	void VulkanRenderEngine::UpdateSceneView(const glm::mat4& view, const glm::mat4& projection)
	{
		g_cameraData.Set(view, projection);
	}

	Scene* VulkanRenderEngine::GetScene()
	{
		return m_scene;
	}

	const Scene* VulkanRenderEngine::GetScene() const
	{
		return m_scene;
	}

	void VulkanRenderEngine::SetScene(Scene* scene)
	{
		m_scene = static_cast<Scene*>(scene);
		m_scene->Init();
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			m_renderContext.FrameContextArray[i].Scene = m_scene;
			if (scene)
				m_scene->InitFrameData(m_renderContext, m_renderContext.FrameContextArray[i]);
		}

		rendersystem::ui::AddWindowCallback("Scene", &ImGuiCallback_Scene, m_scene);
	}

	void VulkanRenderEngine::ReloadShaders()
	{
		PROFILE_SCOPE_LOG(ReloadShaders, "reload shaders");
		g_render->ReloadAllShaders();
		logok("Shader reloaded.\n");
	}

	void VulkanRenderEngine::DumpShadersInfo()
	{
#if 0
		for (uint32_t i = 0; i < m_shaderDb.GetShaderCount(); ++i)
			m_shaderDb.GetShaderArray()[i]->DumpInfo();
#endif // 0

	}

	void VulkanRenderEngine::BeginFrame()
	{
		CPU_PROFILE_SCOPE(BeginFrame);
		g_render->BeginFrame(); 
	}

	void VulkanRenderEngine::Draw()
	{
		//DumpMemoryStats();
		RenderFrameContext& frameContext = GetFrameContext();
		uint32_t frameIndex = m_renderContext.GetFrameIndex();

		g_render->BeginFrame();
		ImGuiDraw();

		g_render->BeginMarker("Renderer");
		m_renderer.Draw(m_renderContext, frameContext);
		g_render->EndMarker();

		g_render->BeginMarker("Test compute shaders");
		m_gpuParticleSystem.Dispatch(m_renderContext, frameIndex);
		m_gpuParticleSystem.Draw(m_renderContext, frameContext);
		m_gol->Compute();
		g_render->EndMarker();

		g_render->BeginMarker("Debug render");
		m_renderer.DebugRender();
		DebugRender::Draw(g_render->GetLDRTarget());
		g_render->EndMarker();
		
		g_render->EndFrame();
	}

	void VulkanRenderEngine::ImGuiDraw()
	{
		// Show always profiling for fps and frame counter and console
		//DrawConsole();
		Profiling::ImGuiDraw();

		// Show the rest of imgui windows only if is desired
		if (CVar_ShowImGui.Get())
		{
			rendersystem::ui::Show();
			tApplication::ImGuiDraw();
		}
	}

	void VulkanRenderEngine::DrawCubemap(const RenderContext& context, const cTexture& texture)
	{
#if defined(MIST_CUBEMAP)
		m_cubemapPipeline.Shader->UseProgram(context);
		const RenderFrameContext& frameContext = context.GetFrameContext();
		glm::mat4 viewRot = frameContext.CameraData->InvView;
		viewRot[3] = { 0.f,0.f,0.f,1.f };
		glm::mat4 ubo[2];
		ubo[0] = viewRot;
		ubo[1] = frameContext.CameraData->Projection * viewRot;
		m_cubemapPipeline.Shader->SetBufferData(context, "u_ubo", ubo, sizeof(glm::mat4) * 2);
		m_cubemapPipeline.Shader->BindTextureSlot(context, 1, texture);
		m_cubemapPipeline.Shader->FlushDescriptors(context);
		CommandBuffer cmd = frameContext.GraphicsCommandContext.CommandBuffer;
		check(m_cubemapPipeline.Cube->m_meshes.GetSize() == 1);
		const cMesh& mesh = m_cubemapPipeline.Cube->m_meshes[0];
		mesh.BindBuffers(cmd);
		RenderAPI::CmdDrawIndexed(cmd, mesh.IndexCount, 1, 0, 0, 0);
#endif // defined(MIST_CUBEMAP)

	}

	RenderFrameContext& VulkanRenderEngine::GetFrameContext()
	{
		return m_renderContext.GetFrameContext();
	}

	bool VulkanRenderEngine::InitVulkan()
	{
		return true;

		// Get Vulkan version
        uint32_t instanceVersion = VK_API_VERSION_1_0;
		PFN_vkEnumerateInstanceVersion FN_vkEnumerateInstanceVersion = PFN_vkEnumerateInstanceVersion(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
		if (FN_vkEnumerateInstanceVersion) 
		{
			vkcheck(FN_vkEnumerateInstanceVersion(&instanceVersion));
		}
		else
			logerror("Fail to get Vulkan API version. Using base api version.\n");

        // 3 macros to extract version info
        uint32_t major = VK_VERSION_MAJOR(instanceVersion);
        uint32_t minor = VK_VERSION_MINOR(instanceVersion);
        uint32_t patch = VK_VERSION_PATCH(instanceVersion);
		logfok("Vulkan API version: %d.%d.%d\n", major, minor, patch);

		vkb::InstanceBuilder builder;
		vkb::Result<vkb::Instance> instanceReturn = builder
			.set_app_name("Vulkan renderer")
			.request_validation_layers(CVar_EnableValidationLayer.Get())
			.require_api_version(major, minor, patch)
			//.use_default_debug_messenger()
			.set_debug_callback(&Mist::Debug::DebugVulkanCallback)
			//.enable_extension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)
			.build();
		check(instanceReturn.has_value());
		//check(instanceReturn.full_error().vk_result == VK_SUCCESS);
		//check(false && "holaholahola");
		if (instanceReturn.matches_error(vkb::InstanceError::failed_create_instance))
		{
			Log(LogLevel::Error, "Failed to create vulkan instance.\n");
			return false;
		}
		vkb::Instance instance = instanceReturn.value();
		m_renderContext.Instance = instance.instance;
		m_renderContext.DebugMessenger = instance.debug_messenger;
		Log(LogLevel::Ok, "Vulkan render instance created...\n");

		// Physical device
		Window::CreateSurface(*m_renderContext.Window, &m_renderContext.Instance, &m_renderContext.Surface);
		Log(LogLevel::Ok, "Vulkan surface instance created...\n");
		vkb::PhysicalDeviceSelector selector(instance);
		vkb::PhysicalDevice physicalDevice = selector
			.set_minimum_version(1, 1)
			.set_surface(m_renderContext.Surface)
			.allow_any_gpu_device_type(false)
			.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
			.select()
			.value();
		vkb::DeviceBuilder deviceBuilder{ physicalDevice };
		Log(LogLevel::Ok, "Vulkan physical device instance created...\n");

		// Enable shader draw parameters
		VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParamsFeatures = {};
		shaderDrawParamsFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
		shaderDrawParamsFeatures.pNext = nullptr;
		shaderDrawParamsFeatures.shaderDrawParameters = VK_TRUE;
		deviceBuilder.add_pNext(&shaderDrawParamsFeatures);

		// Build PhysicalDeviceFeatures
		VkPhysicalDeviceFeatures2 features;
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = nullptr;
		vkGetPhysicalDeviceFeatures2(physicalDevice.physical_device, &features);
		deviceBuilder.add_pNext(&features);

		// Enable timeline semaphores.
		VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures;
		timelineSemaphoreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
		timelineSemaphoreFeatures.pNext = nullptr;
		timelineSemaphoreFeatures.timelineSemaphore = VK_TRUE;
		deviceBuilder.add_pNext(&timelineSemaphoreFeatures);

        // Enable synchronization2
        VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features;
        sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
        sync2Features.pNext = nullptr;
        sync2Features.synchronization2 = VK_TRUE;
        deviceBuilder.add_pNext(&sync2Features);

		// Create Device
		vkb::Result<vkb::Device> deviceResult = deviceBuilder.build();
		check(deviceResult.has_value());
		vkb::Device device = deviceResult.value();
		m_renderContext.Device = device.device;
		m_renderContext.GPUDevice = physicalDevice.physical_device;
		Log(LogLevel::Ok, "Vulkan logical device instance created...\n");

		// Graphics queue from device
		m_renderContext.GraphicsQueue = device.get_queue(vkb::QueueType::graphics).value();
		m_renderContext.GraphicsQueueFamily = device.get_queue_index(vkb::QueueType::graphics).value();
		Log(LogLevel::Ok, "Vulkan graphics queue instance created...\n");

		// Compute queue from device
#if 0
		m_renderContext.ComputeQueue = device.get_queue(vkb::QueueType::compute).value();
		m_renderContext.ComputeQueueFamily = device.get_queue_index(vkb::QueueType::compute).value();
		check(m_renderContext.ComputeQueue != VK_NULL_HANDLE);
		check(m_renderContext.ComputeQueueFamily != (uint32_t)vkb::QueueError::invalid_queue_family_index
			&& m_renderContext.ComputeQueueFamily != vkb::detail::QUEUE_INDEX_MAX_VALUE);

#else
        // Get queue families and find a queue that supports both graphics and compute
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(m_renderContext.GPUDevice, &queueFamilyCount, nullptr);
		VkQueueFamilyProperties* queueFamilyProperties = _new VkQueueFamilyProperties[queueFamilyCount];
		vkGetPhysicalDeviceQueueFamilyProperties(m_renderContext.GPUDevice, &queueFamilyCount, queueFamilyProperties);

		uint32_t graphicsComputeQueueIndex = UINT32_MAX;
		for (uint32_t i = 0; i < queueFamilyCount; ++i)
		{
			logfinfo("DeviceQueue %d:\n", i);
			logfinfo("* Graphics: %d, Compute: %d, Transfer: %d, SparseBinding: %d\n",
				queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT? 1 : 0,
				queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT? 1 : 0,
				queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT? 1 : 0,
				queueFamilyProperties[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT? 1 : 0);

			if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT
				&& queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				graphicsComputeQueueIndex = i;
			}
		}

		check(graphicsComputeQueueIndex != UINT32_MAX && "Architecture doesn't support separate graphics and compute hardware queues.");
		delete[] queueFamilyProperties;
		queueFamilyProperties = nullptr;

		m_renderContext.ComputeQueueFamily = graphicsComputeQueueIndex;
		vkGetDeviceQueue(m_renderContext.Device, graphicsComputeQueueIndex, 0, &m_renderContext.ComputeQueue);
		check(m_renderContext.ComputeQueue);
#endif // 0


		// Dump physical device info
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(m_renderContext.GPUDevice, &deviceProperties);
		Logf(LogLevel::Info, "Physical device:\n\t- %s\n\t- Id: %d\n\t- VendorId: %d\n",
			deviceProperties.deviceName, deviceProperties.deviceID, deviceProperties.vendorID);

		m_renderContext.GPUProperties = device.physical_device.properties;
		Logf(LogLevel::Info, "GPU has minimum buffer alignment of %Id bytes.\n",
			m_renderContext.GPUProperties.limits.minUniformBufferOffsetAlignment);
		Logf(LogLevel::Info, "GPU max bound descriptor sets: %d\n",
			m_renderContext.GPUProperties.limits.maxBoundDescriptorSets);

		// Init memory allocator
		Memory::Init(m_renderContext.Allocator, m_renderContext.Instance, m_renderContext.Device, m_renderContext.GPUDevice);
		Log(LogLevel::Ok, "Vulkan memory allocator instance created...\n");

		// Load external pfn
#define GET_VK_PROC_ADDRESS(inst, fn) (PFN_##fn)vkGetInstanceProcAddr(inst, #fn)
		m_renderContext.pfn_vkCmdBeginDebugUtilsLabelEXT = GET_VK_PROC_ADDRESS(m_renderContext.Instance, vkCmdBeginDebugUtilsLabelEXT);
		m_renderContext.pfn_vkCmdEndDebugUtilsLabelEXT = GET_VK_PROC_ADDRESS(m_renderContext.Instance, vkCmdEndDebugUtilsLabelEXT);
		m_renderContext.pfn_vkCmdInsertDebugUtilsLabelEXT = GET_VK_PROC_ADDRESS(m_renderContext.Instance, vkCmdInsertDebugUtilsLabelEXT);
		m_renderContext.pfn_vkSetDebugUtilsObjectNameEXT = GET_VK_PROC_ADDRESS(m_renderContext.Instance, vkSetDebugUtilsObjectNameEXT);
#undef GET_VK_PROC_ADDRESS

		return true;
	}

	bool VulkanRenderEngine::InitCommands()
	{
		VkCommandPoolCreateInfo graphicsPoolInfo = vkinit::CommandPoolCreateInfo(m_renderContext.GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		VkCommandPoolCreateInfo computePoolInfo = vkinit::CommandPoolCreateInfo(m_renderContext.ComputeQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

		vkcheck(vkCreateCommandPool(m_renderContext.Device, &graphicsPoolInfo, nullptr, &m_renderContext.TransferContext.CommandPool));
		VkCommandBufferAllocateInfo allocInfo = vkinit::CommandBufferCreateAllocateInfo(m_renderContext.TransferContext.CommandPool, 1);
		vkcheck(vkAllocateCommandBuffers(m_renderContext.Device, &allocInfo, &m_renderContext.TransferContext.CommandBuffer));
        char buff[64];
        sprintf_s(buff, "TransferCommandBuffer");
        SetVkObjectName(m_renderContext, &m_renderContext.TransferContext.CommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, buff);
		return true;
	}


	bool VulkanRenderEngine::InitSync()
	{
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			RenderFrameContext& frameContext = m_renderContext.FrameContextArray[i];
			// Render fence
			VkFenceCreateInfo fenceInfo = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);

			// Render semaphore
			VkSemaphoreTypeCreateInfo typeInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr };
			typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
			VkSemaphoreCreateInfo semaphoreInfo = vkinit::SemaphoreCreateInfo();
			vkcheck(vkCreateSemaphore(m_renderContext.Device, &semaphoreInfo, nullptr, &frameContext.RenderSemaphore));
			// Present semaphore
			semaphoreInfo.pNext = nullptr;
			vkcheck(vkCreateSemaphore(m_renderContext.Device, &semaphoreInfo, nullptr, &frameContext.PresentSemaphore));

			char buff[256];
			sprintf_s(buff, "RenderSemaphore_%u", i);
			SetVkObjectName(m_renderContext, &frameContext.RenderSemaphore, VK_OBJECT_TYPE_SEMAPHORE, buff);
			sprintf_s(buff, "PresentSemaphore_%u", i);
			SetVkObjectName(m_renderContext, &frameContext.PresentSemaphore, VK_OBJECT_TYPE_SEMAPHORE, buff);
		}

		VkFenceCreateInfo info = vkinit::FenceCreateInfo();
		vkcheck(vkCreateFence(m_renderContext.Device, &info, nullptr, &m_renderContext.TransferContext.Fence));
		return true;
	}

	bool VulkanRenderEngine::InitPipeline()
	{
		return true;
	}
}