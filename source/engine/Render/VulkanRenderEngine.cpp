#include "Render/VulkanRenderEngine.h"

#include <cstdio>
#include <glm/gtx/transform.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <string.h>

#include "Render/Mesh.h"
#include "Render/Camera.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include "Core/Console.h"
#include "Core/Debug.h"
#include "Render/Globals.h"
#include "Scene/Scene.h"
#include "Utils/GenericUtils.h"
#include "RenderProcesses/RenderProcess.h"
#include "Utils/TimeUtils.h"
#include "Application/CmdParser.h"
#include "Application/Application.h"
#include "Application/Event.h"
#include "Core/SystemMemory.h"
#include "Render/DebugRender.h"


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

	CBoolVar CVar_EnableValidationLayer("r_enableValidationLayer", true);
	CBoolVar CVar_ExitValidationLayer("r_exitValidationLayer", true);
	CBoolVar CVar_ShowImGui("ShowImGui", true);
	CFloatVar CVar_JitterScale("r_jitterScale", 4.f);

	extern CBoolVar CVar_TAA;
	extern CIntVar CVar_ShowCpuProf;

	::render::Device* g_device = nullptr;
	::rendersystem::RenderSystem* g_render = nullptr;

	template <uint32_t N>
	class JitterSequence
	{
	public:
		static double HaltonSequence(int index, int base)
		{
			double f = 1.0;
			double r = 0.0;
			while (index > 0)
			{
				f /= base;
				r = r + f * (index % base);
				index /= base;
			}
			return r;
		}

		JitterSequence(uint32_t base)
		{
			for (uint32_t i = 0; i < N; ++i)
				m_haltonSequence[i] = HaltonSequence(i, base);
		}

		double GetHalton(uint32_t index) const { return m_haltonSequence[index % N]; }
		double operator[](uint32_t index) const { return GetHalton(index); }
	private:
		double m_haltonSequence[N];
	};

	static CameraData g_cameraData;
	static CameraData g_previousCameraData;

	const CameraData* GetCameraData() { return &g_cameraData; }
	const CameraData* GetPrevCameraData() { return &g_previousCameraData; }

	static void UpdateCameraData(const glm::mat4& view, const glm::mat4& proj, const glm::mat4& jitteredProj)
	{
		g_previousCameraData = g_cameraData;
		g_cameraData.Set(view, proj, jitteredProj);
	}

	static void JitterPerspectiveMatrix(glm::mat4& mat, float width, float height, uint32_t frame)
	{
		static JitterSequence<8> jitterSequence(2);
		glm::vec2 texelSize = { 1.f/width, 1.f/height };
		float h = jitterSequence[frame] * CVar_JitterScale.Get();
		mat[2][0] = h * 0.5f * texelSize.x;
		mat[2][1] = h * 0.5f * texelSize.y;
	}

	namespace Debug
	{
		uint32_t GVulkanLayerValidationErrors = 0;

		static VkBool32 DebugVulkanCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
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

	static void ExecCommand_ReloadShaders(const char* cmd)
	{
		VulkanRenderEngine* eng = IRenderEngine::GetRenderEngineAs<VulkanRenderEngine>();
		eng->ReloadShaders();
	}

	static void ExecCommand_DumpShadersInfo(const char* cmd)
	{
		VulkanRenderEngine* eng = IRenderEngine::GetRenderEngineAs<VulkanRenderEngine>();
		eng->DumpShadersInfo();
	}

	cMaterial* DefaultMaterial = nullptr;
	cMaterial* GetDefaultMaterial()
	{
		if (!DefaultMaterial)
		{
			DefaultMaterial = _new cMaterial();
			DefaultMaterial->SetName("DefaultMaterial");
			DefaultMaterial->m_albedo = glm::vec4(1.f, 0.f, 1.f,1.f);
		}
		return DefaultMaterial;
	}

	
	
	bool VulkanRenderEngine::Init(const Window& window)
	{
		CPU_PROFILE_SCOPE(Init);
#ifndef _DEBUG
		logfinfo("Running app in %s mode.\n", "RELEASE");
#else
		logfinfo("Running app in %s mode.\n", "DEBUG");
#endif // _DEBUG
			

		SDL_Init(SDL_INIT_VIDEO);

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

		SceneRenderer::Init();
		m_renderer.Init(m_renderSystem, this);
		m_gpuParticleSystem.Init(g_render);
		DebugRender::Init();
		//////////////////////////////////////
		// Console commands
		//////////////////////////////////////
		AddConsoleCommand("r_reloadshaders", ExecCommand_ReloadShaders);
		AddConsoleCommand("r_dumpshadersinfo", ExecCommand_DumpShadersInfo);

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
		rendersystem::ui::AddWindowCallback("System memory", [](void* data)
			{
				ImGui::Begin("System memory");
				const memory::stats::MemoryStats& stats = memory::GetMemoryStats();
				ImGui::Text("Allocated size		: %6lld bytes (%4.4f KB)", stats.allocatedBytes, (float)stats.allocatedBytes / 1024.f);
				ImGui::Text("Max Allocated size	: %6lld bytes", stats.maxAllocatedBytes);
				ImGui::Text("Frame alloc count	: %4lld", stats.frameAllocCount);
				ImGui::Text("Frame free count	: %4lld", stats.frameFreeCount);
				ImGui::Text("Alloc count		: %4lld", stats.allocatedCount);
				ImGui::End();
			}, nullptr, true);
#if 0
		rendersystem::ui::AddWindowCallback("Game of life demo", [](void* data)
			{
				check(data);
				Gol* g = static_cast<Gol*>(data);
				g->ImGuiDraw();
			}, & m_gol);
#endif // 0

		rendersystem::ui::AddWindowCallback("Scene renderer",
			[](void* data)
			{
				check(data);
				static_cast<SceneRenderer*>(data)->ImGuiDraw();
			}, SceneRenderer::GetSceneRenderer());


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
			m_renderer.Update();
			SceneRenderer::GetSceneRenderer()->BuildRenderLists(m_scene);
		}
		else
			m_renderer.Update();

		FlushPendingConsoleCommands();
		Draw();
		return true;
	}

	void VulkanRenderEngine::Shutdown()
	{
		loginfo("Shutdown render engine.\n");
		g_device->WaitIdle();
		if (m_scene)
		{
			m_scene->Destroy();
			delete m_scene;
			m_scene = nullptr;
		}
		m_gpuParticleSystem.Destroy(g_render);
		DebugRender::Destroy();
		m_renderer.Destroy(m_renderSystem);
		SceneRenderer::Destroy();
		g_device = nullptr;
		g_render = nullptr;
		m_renderSystem->Destroy();
		delete m_renderSystem;
	}

	void VulkanRenderEngine::UpdateSceneView(const glm::mat4& view, const glm::mat4& projection)
	{
		glm::mat4 jitteredProj = projection;
		if (CVar_TAA.Get())
			JitterPerspectiveMatrix(jitteredProj, (float)g_render->GetRenderResolution().width, (float)g_render->GetRenderResolution().height, g_render->GetFrameCounter());
		UpdateCameraData(view, projection, jitteredProj);
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
		if (m_scene)
		{
			m_scene->Destroy();
			delete m_scene;
			m_scene = nullptr;
		}
		if (scene)
		{
			m_scene = scene;
			m_scene->Init();

			rendersystem::ui::AddWindowCallback("Scene", [](void* data)
				{
					check(data);
					Scene* s = static_cast<Scene*>(data);
					s->ImGuiDraw();
				}, m_scene);
		}
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

		g_render->BeginFrame();

		g_render->BeginMarker("Renderer");
		m_renderer.Draw(m_renderSystem);
		g_render->EndMarker();

		g_render->BeginMarker("Test compute shaders");
		m_gpuParticleSystem.Dispatch(g_render);
		m_gpuParticleSystem.Draw(g_render);
		//m_gol->Compute();
		g_render->EndMarker();

		g_render->BeginMarker("Debug render");
		m_renderer.DebugRender();
		DebugRender::Draw(g_render->GetLDRTarget());
		g_render->EndMarker();
		
		ImGuiDraw();
		g_render->EndFrame();
	}

	void VulkanRenderEngine::ImGuiDraw()
	{
		// Show the rest of imgui windows only if is desired
		if (CVar_ShowImGui.Get())
		{
			rendersystem::ui::Show();
			Profiling::CpuProf_ImGuiDraw();
			tApplication::ImGuiDraw();
		}
	}

	bool VulkanRenderEngine::InitVulkan()
	{
		return true;
#if 0

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
				queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ? 1 : 0,
				queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT ? 1 : 0,
				queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT ? 1 : 0,
				queueFamilyProperties[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT ? 1 : 0);

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
#endif // 0

	}

	bool VulkanRenderEngine::InitCommands()
	{
#if 0
		VkCommandPoolCreateInfo graphicsPoolInfo = vkinit::CommandPoolCreateInfo(m_renderContext.GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		VkCommandPoolCreateInfo computePoolInfo = vkinit::CommandPoolCreateInfo(m_renderContext.ComputeQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

		vkcheck(vkCreateCommandPool(m_renderContext.Device, &graphicsPoolInfo, nullptr, &m_renderContext.TransferContext.CommandPool));
		VkCommandBufferAllocateInfo allocInfo = vkinit::CommandBufferCreateAllocateInfo(m_renderContext.TransferContext.CommandPool, 1);
		vkcheck(vkAllocateCommandBuffers(m_renderContext.Device, &allocInfo, &m_renderContext.TransferContext.CommandBuffer));
		char buff[64];
		sprintf_s(buff, "TransferCommandBuffer");
		SetVkObjectName(m_renderContext, &m_renderContext.TransferContext.CommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, buff);
#endif // 0

		return true;
	}


	bool VulkanRenderEngine::InitSync()
	{
#if 0
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
#endif // 0

		return true;
	}

	bool VulkanRenderEngine::InitPipeline()
	{
		return true;
	}
}