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


#define MIST_CRASH_ON_VALIDATION_LAYER

#define UNIFORM_ID_SCREEN_QUAD_INDEX "ScreenQuadIndex"
#define MAX_RT_SCREEN 6


namespace Mist
{
	CBoolVar CVar_EnableValidationLayer("EnableValidationLayer", true);
	CBoolVar CVar_ExitValidationLayer("ExitValidationLayer", true);
	CBoolVar CVar_ShowImGui("ShowImGui", true);

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
#if defined(_DEBUG)
				if (!CVar_ExitValidationLayer.Get())
					PrintCallstack();
#endif
#ifdef MIST_CRASH_ON_VALIDATION_LAYER
				check(!CVar_ExitValidationLayer.Get() && "Validation layer error");
#endif
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
	

	void CubemapPipeline::Init(const RenderContext& context, const RenderTarget* rt)
	{
		check(!Cube && !Shader);
		tShaderProgramDescription shaderDesc;
		shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("skybox.vert");
		shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("skybox.frag");
		shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
		shaderDesc.RenderTarget = rt;
		shaderDesc.CullMode = CULL_MODE_FRONT_BIT;
		shaderDesc.DepthStencilMode = DEPTH_STENCIL_NONE;
		shaderDesc.FrontFaceMode = FRONT_FACE_COUNTER_CLOCKWISE;
		Shader = ShaderProgram::Create(context, shaderDesc);

		Cube = _new cModel();
		Cube->LoadModel(context, ASSET_PATH("models/cube.gltf"));
	}

	void CubemapPipeline::Destroy(const RenderContext& context)
	{
		check(Cube);
		Cube->Destroy(context);
		delete Cube;
		Cube = nullptr;
	}

	bool VulkanRenderEngine::Init(const Window& window)
	{
		CPU_PROFILE_SCOPE(Init);
#ifdef _DEBUG
		Log(LogLevel::Info, "Running app in DEBUG mode.\n");
#else
		Log(LogLevel::Info, "Running app in RELEASE mode.\n");
#endif // _DEBUG

		Log(LogLevel::Info, "Initialize render engine.\n");
		m_renderContext.Window = &window;
		SDL_Init(SDL_INIT_VIDEO);
		Log(LogLevel::Ok, "Window created successfully!\n");

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
		check(m_swapchain.Init(m_renderContext, { window.Width, window.Height }));

		// Commands
		check(InitCommands());
		// Pipelines
		check(InitPipeline());
		// Init sync vars
		check(InitSync());

        m_renderContext.Queue = _new CommandQueue(&m_renderContext, QUEUE_GRAPHICS | QUEUE_COMPUTE | QUEUE_TRANSFER);
        m_renderContext.CmdList = _new CommandList(&m_renderContext);





		//const RenderTarget& gbufferRT = *m_renderer.GetRenderProcess(RENDERPROCESS_GBUFFER)->GetRenderTarget();
		//const RenderTarget& lightingRT = *m_renderer.GetRenderProcess(RENDERPROCESS_LIGHTING)->GetRenderTarget();
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; i++)
		{
			m_renderContext.FrameContextArray[i].GraphicsTimestampQueryPool.Init(m_renderContext.Device, 40);
			m_renderContext.FrameContextArray[i].ComputeTimestampQueryPool.Init(m_renderContext.Device, 40);

			// 3 stage buffer per frame with 20 MB each one.
			m_renderContext.FrameContextArray[i].TempStageBuffer.Init(m_renderContext, 20 * 1024 * 1024, 3);
		}

		// Initialize render processes after instantiate render context
		m_renderContext.Renderer = &m_renderer;
		m_renderer.Init(m_renderContext, m_renderContext.FrameContextArray, CountOf(m_renderContext.FrameContextArray), m_swapchain);
		DebugRender::Init(m_renderContext);
		ui::Init(m_renderContext, m_renderer.GetLDRTarget());
#if defined(MIST_CUBEMAP)
		m_cubemapPipeline.Init(m_renderContext, &m_renderer.GetLDRTarget());
#endif // defined(MIST_CUBEMAP)
		m_gpuParticleSystem.Init(m_renderContext);
		m_gpuParticleSystem.InitFrameData(m_renderContext, m_renderContext.FrameContextArray);

		//AddConsoleCommand(ExecCommand_CVar);
		AddConsoleCommand("r_reloadshaders", ExecCommand_ReloadShaders);
		AddConsoleCommand("r_dumpshadersinfo", ExecCommand_DumpShadersInfo);

		FullscreenQuad.Init(m_renderContext, -1.f, 1.f, -1.f, 1.f);

		return true;
	}

	bool VulkanRenderEngine::RenderProcess()
	{
		CPU_PROFILE_SCOPE(Process);
		static tTimePoint point = GetTimePoint();
		tTimePoint now = GetTimePoint();
		float ms = GetMiliseconds(now - point);
		point = now;
		Profiling::AddCPUTime(ms);

		ui::Begin(m_renderContext);
		{
			CPU_PROFILE_SCOPE(ImGuiCalbacks);
			ImGuiDraw();
		}
		//BeginFrame();
		Mist::Profiling::GRenderStats.Reset();
		Draw();
		return true;
	}

	void VulkanRenderEngine::Shutdown()
	{
		loginfo("Shutdown render engine.\n");

		RenderContext_ForceFrameSync(m_renderContext);

		delete m_renderContext.CmdList;
		delete m_renderContext.Queue;

		FullscreenQuad.Destroy(m_renderContext);

		if (m_scene)
		{
			m_scene->Destroy();
			delete m_scene;
			m_scene = nullptr;
		}

		m_gpuParticleSystem.Destroy(m_renderContext);

		ui::Destroy(m_renderContext);
		DebugRender::Destroy(m_renderContext);
#if defined(MIST_CUBEMAP)
		m_cubemapPipeline.Destroy(m_renderContext);
#endif // defined(MIST_CUBEMAP)

		m_renderer.Destroy(m_renderContext);

		// Destroy Default resources
		if (DefaultTexture)
			cTexture::Destroy(m_renderContext, DefaultTexture);
		DefaultTexture = nullptr;
		if (DefaultMaterial)
		{
			DefaultMaterial->Destroy(m_renderContext);
			delete DefaultMaterial;
			DefaultMaterial = nullptr;
		}


		DestroySamplers(m_renderContext);
		vkDestroyFence(m_renderContext.Device, m_renderContext.TransferContext.Fence, nullptr);
		vkDestroyCommandPool(m_renderContext.Device, m_renderContext.TransferContext.CommandPool, nullptr);
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			m_renderContext.FrameContextArray[i].TempStageBuffer.Destroy(m_renderContext);
			m_renderContext.FrameContextArray[i].GraphicsTimestampQueryPool.Destroy(m_renderContext.Device);
			m_renderContext.FrameContextArray[i].ComputeTimestampQueryPool.Destroy(m_renderContext.Device);
			m_renderContext.FrameContextArray[i].GlobalBuffer->Destroy(m_renderContext);
            delete m_renderContext.FrameContextArray[i].GlobalBuffer;
			vkDestroySemaphore(m_renderContext.Device, m_renderContext.FrameContextArray[i].RenderSemaphore, nullptr);
			vkDestroySemaphore(m_renderContext.Device, m_renderContext.FrameContextArray[i].PresentSemaphore, nullptr);
		}

		m_swapchain.Destroy(m_renderContext);
		m_shaderDb.Destroy(m_renderContext);
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
			m_descriptorAllocators[i].Destroy();
		m_descriptorLayoutCache.Destroy();

		Memory::Destroy(m_renderContext.Allocator);
		vkDestroyDevice(m_renderContext.Device, nullptr);
		vkDestroySurfaceKHR(m_renderContext.Instance, m_renderContext.Surface, nullptr);
		vkb::destroy_debug_utils_messenger(m_renderContext.Instance,
			m_renderContext.DebugMessenger);
		vkDestroyInstance(m_renderContext.Instance, nullptr);



		logok("Render engine terminated.\n");
		Logf(Mist::Debug::GVulkanLayerValidationErrors > 0 ? LogLevel::Error : LogLevel::Ok,
			"Total vulkan layer validation errors: %u.\n", Mist::Debug::GVulkanLayerValidationErrors);

	}

	void VulkanRenderEngine::UpdateSceneView(const glm::mat4& view, const glm::mat4& projection)
	{
		m_cameraData.InvView = glm::inverse(view);
		m_cameraData.Projection = projection;
		m_cameraData.ViewProjection = m_cameraData.Projection * m_cameraData.InvView;
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
	}

	void VulkanRenderEngine::ReloadShaders()
	{
		RenderContext_ForceFrameSync(m_renderContext);
		for (uint32_t i = 0; i < m_shaderDb.GetShaderCount(); ++i)
		{
			m_shaderDb.GetShaderArray()[i]->Destroy(m_renderContext);
			m_shaderDb.GetShaderArray()[i]->Reload(m_renderContext);
		}
	}

	void VulkanRenderEngine::DumpShadersInfo()
	{
		for (uint32_t i = 0; i < m_shaderDb.GetShaderCount(); ++i)
			m_shaderDb.GetShaderArray()[i]->DumpInfo();
	}

	void VulkanRenderEngine::BeginFrame()
	{
		CPU_PROFILE_SCOPE(BeginFrame);
		// TODO descriptor allocators must be instanciated on each frame context.
		m_renderContext.DescAllocator = &m_descriptorAllocators[m_renderContext.GetFrameIndex()];
		RenderFrameContext& frameContext = GetFrameContext();
		frameContext.DescriptorAllocator = m_renderContext.DescAllocator;

		frameContext.GlobalBuffer->SetUniform(m_renderContext, UNIFORM_ID_CAMERA, &m_cameraData, sizeof(CameraData));
		//frameContext.GlobalBuffer->SetUniform(m_renderContext, UNIFORM_ID_SCREEN_QUAD_INDEX, &m_screenPipeline.QuadIndex, sizeof(uint32_t));

		UBOTime time{ 0.033f, 0.f };
		frameContext.GlobalBuffer->SetUniform(m_renderContext, UNIFORM_ID_TIME, &time, sizeof(UBOTime));

		//frameContext.PresentTex = m_screenPipeline.PresentTexSets[m_renderContext.GetFrameIndex()];
		frameContext.Scene->UpdateRenderData(m_renderContext, frameContext);
		//m_screenPipeline.DebugInstance.PrepareFrame(m_renderContext, &frameContext.GlobalBuffer);
		m_renderer.UpdateRenderData(m_renderContext, frameContext);
		m_gpuParticleSystem.UpdateBuffers(m_renderContext, frameContext);
	}

	void VulkanRenderEngine::Draw()
	{
		{
			CPU_PROFILE_SCOPE(NextFrame);
			RenderContext_NewFrame(m_renderContext);
			GpuProf_Resolve(m_renderContext);
			Profiling::AddGPUTime((float)(0.001 * GpuProf_GetGpuTime(m_renderContext, "GpuTime")));
		}
		RenderFrameContext& frameContext = GetFrameContext();
		uint32_t frameIndex = m_renderContext.GetFrameIndex();
		frameContext.Scene = static_cast<Scene*>(m_scene);
		m_renderContext.Queue->ProcessInFlightCommands();

		BeginFrame();
		{
			CPU_PROFILE_SCOPE(AcquireImage);
			// Acquire render image from swapchain
			vkcheck(vkAcquireNextImageKHR(m_renderContext.Device, m_swapchain.GetSwapchainHandle(), 1000000000, frameContext.PresentSemaphore,
				nullptr, &m_currentSwapchainIndex));

			m_renderContext.Queue->AddWaitSemaphore(frameContext.PresentSemaphore, 0);
			m_renderContext.Queue->AddSignalSemaphore(frameContext.RenderSemaphore, 0);
		}
		CommandList* commandList = m_renderContext.CmdList;
		m_renderContext.CmdList->Begin();

		CPU_PROFILE_SCOPE(Draw);
		
		// Graphics
		CPU_PROFILE_SCOPE(CpuGraphics);
		// Timestamp queries
		GpuProf_Reset(m_renderContext);
		GpuProf_Begin(m_renderContext, "GpuTime");

		commandList->BeginMarker("Graphics");
		GpuProf_Begin(m_renderContext, "Graphics_Renderer");
		m_renderer.Draw(m_renderContext, frameContext);
		GpuProf_End(m_renderContext);

		commandList->BeginMarker("ComputeParticles");
		m_gpuParticleSystem.Dispatch(m_renderContext, frameIndex);
		commandList->EndMarker();
		commandList->BeginMarker("DrawParticles");
		m_gpuParticleSystem.Draw(m_renderContext, frameContext);
		commandList->EndMarker();

		DebugRender::Draw(m_renderContext);
		ui::End(m_renderContext);

		GpuProf_Begin(m_renderContext, "Graphics_ScreenDraw");
        commandList->BeginMarker("ScreenDraw");
		RenderTarget& rt = m_renderer.GetPresentRenderTarget(m_currentSwapchainIndex);
		Renderer::tCopyParams copyParams;
		copyParams.BlendState.Enabled = false;
		copyParams.Src = &m_renderer.GetLDRTarget();
		copyParams.Dst = &rt;
		copyParams.Context = &m_renderContext;
		m_renderer.CopyRenderTarget(copyParams);
        commandList->EndMarker();
		GpuProf_End(m_renderContext);

		// End command buffers
		GpuProf_End(m_renderContext);
		commandList->End();
		commandList->EndMarker();
		frameContext.GraphicsTimestampQueryPool.EndFrame();

		{
			CPU_PROFILE_SCOPE(QueueSubmit);
			CommandList* lists[] = { commandList };
			frameContext.SubmissionId = CommandList::ExecuteCommandLists(lists, CountOf(lists));
		}

		{
			CPU_PROFILE_SCOPE(Present);
			// Present
			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.pNext = nullptr;
			VkSwapchainKHR swapchain = m_swapchain.GetSwapchainHandle();
			presentInfo.pSwapchains = &swapchain;
			presentInfo.swapchainCount = 1;
			presentInfo.pWaitSemaphores = &frameContext.RenderSemaphore;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pImageIndices = &m_currentSwapchainIndex;
			vkcheck(vkQueuePresentKHR(m_renderContext.GraphicsQueue, &presentInfo));
		}
	}

	void VulkanRenderEngine::ImGuiDraw()
	{
		// Show always profiling for fps and frame counter and console
		DrawConsole();
		Profiling::ImGuiDraw();

		// Show the rest of imgui windows only if is desired
		if (CVar_ShowImGui.Get())
		{
			// Application imgui calls
			ImGuiCVars();
			ImGuiDrawInputState();

			// Render engine imgui calls
			m_renderer.ImGuiDraw();
			GpuProf_ImGuiDraw(m_renderContext);
			m_gpuParticleSystem.ImGuiDraw();
			if (m_scene)
				m_scene->ImGuiDraw();

			// Demo imgui
			if (CVar_ShowImGuiDemo.Get())
				ImGui::ShowDemoWindow();
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

	void VulkanRenderEngine::ImGuiCVars()
	{
		ImGui::Begin("CVars");
		uint32_t count = GetCVarCount();
		CVar** cvarArray = GetCVarArray();
		for (uint32_t i = 0; i < count; ++i)
		{
			CVar* cvar = cvarArray[i];
			ImGuiUtils::EditCVar(*cvar);
		}
		ImGui::End();
	}

	bool VulkanRenderEngine::InitVulkan()
	{
		// Get Vulkan version
        uint32_t instanceVersion = VK_API_VERSION_1_0;
        auto FN_vkEnumerateInstanceVersion = PFN_vkEnumerateInstanceVersion(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
		if (vkEnumerateInstanceVersion) {
			vkEnumerateInstanceVersion(&instanceVersion);
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
		for (size_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			RenderFrameContext& frameContext = m_renderContext.FrameContextArray[i];
			frameContext.CameraData = &m_cameraData;
			frameContext.GlobalBuffer = _new UniformBufferMemoryPool(&m_renderContext);

			// Size for uniform frame buffer
			constexpr uint32_t size = 1024 * 1024; // 1MB
			frameContext.GlobalBuffer->Init(m_renderContext, size, BUFFER_USAGE_UNIFORM);

			// Update global descriptors
			uint32_t cameraBufferSize = sizeof(CameraData);
			uint32_t cameraBufferOffset = frameContext.GlobalBuffer->AllocUniform(m_renderContext, UNIFORM_ID_CAMERA, cameraBufferSize);
			VkDescriptorBufferInfo bufferInfo;
			bufferInfo.buffer = frameContext.GlobalBuffer->GetBuffer();
			bufferInfo.offset = cameraBufferOffset;
			bufferInfo.range = cameraBufferSize;
			DescriptorBuilder::Create(m_descriptorLayoutCache, m_descriptorAllocators[i])
				.BindBuffer(0, &bufferInfo, 1, DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
				.Build(m_renderContext, frameContext.CameraDescriptorSet, m_globalDescriptorLayout);

			// Scene buffer allocation. TODO: this should be done by scene.
			frameContext.GlobalBuffer->AllocDynamicUniform(m_renderContext, UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY, sizeof(glm::mat4), globals::MaxRenderObjects);
			frameContext.GlobalBuffer->AllocUniform(m_renderContext, UNIFORM_ID_SCENE_ENV_DATA, sizeof(EnvironmentData));
			frameContext.GlobalBuffer->AllocUniform(m_renderContext, UNIFORM_ID_TIME, sizeof(UBOTime));
		}
		return true;
	}
}