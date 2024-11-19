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


#define MIST_CRASH_ON_VALIDATION_LAYER

#define UNIFORM_ID_SCREEN_QUAD_INDEX "ScreenQuadIndex"
#define MAX_RT_SCREEN 6


namespace Mist
{
	CBoolVar CVar_ExitValidationLayer("ExitValidationLayer", true);

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
				PrintCallstack();
#endif
#ifdef MIST_CRASH_ON_VALIDATION_LAYER
				check(!CVar_ExitValidationLayer.Get() && "Validation layer error");
#endif
			}
			return VK_FALSE;
		}
	}

	CBoolVar CVar_ShowConsole("ShowConsole", false);
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

		void Draw(CommandBuffer cmd)
		{
			VB.Bind(cmd);
			IB.Bind(cmd);
			RenderAPI::CmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
		}

		void Destroy(const RenderContext& context)
		{
			VB.Destroy(context);
			IB.Destroy(context);
		}
	} FullscreenQuad;

	void CmdDrawFullscreenQuad(CommandBuffer cmd)
	{
		FullscreenQuad.Draw(cmd);
	}

	cTexture* DefaultTexture = nullptr;
	cTexture* GetTextureCheckerboard4x4(const RenderContext& context)
	{
		if (!DefaultTexture)
		{
			tImageDescription desc
			{
				.Format = FORMAT_R8G8B8A8_SRGB,
				.Width = 2,
				.Height = 2,
				.Depth = 1,
			};
			DefaultTexture = cTexture::Create(context, desc);
			float pixels[] = { 0x000000ff, 0xffffffff, 0x000000ff, 0xffffffff };
			const uint8_t* layer = (uint8_t*)pixels;
			DefaultTexture->SetImageLayers(context, &layer, 1);
			DefaultTexture->CreateView(context, tViewDescription());
		}
		return DefaultTexture;
	}

	Window Window::Create(uint32_t width, uint32_t height, uint32_t posx, uint32_t posy, const char* title)
	{
		Window newWindow;
		newWindow.Width = width;
		newWindow.Height = height;
		strcpy_s(newWindow.Title, title);
		SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
		newWindow.WindowInstance = SDL_CreateWindow(
			title, posx, posy,
			width, height, windowFlags);
		SDL_Init(SDL_INIT_VIDEO);
		return newWindow;
	}

	void Window::CreateSurface(const Window& window, void* renderApiInstance, void* outSurface)
	{
		SDL_Vulkan_CreateSurface((SDL_Window*)window.WindowInstance, *((VkInstance*)renderApiInstance), (VkSurfaceKHR*)outSurface);
	}

	void Window::Destroy(Window& window)
	{
		SDL_DestroyWindow((SDL_Window*)window.WindowInstance);
		ZeroMemory(&window, sizeof(Window));
	}

	void ScreenQuadPipeline::Init(const RenderContext& context, const Swapchain& swapchain)
	{
		uint32_t swapchainCount = swapchain.GetImageCount();
		RenderTargetArray.resize(swapchainCount);
		for (uint32_t i = 0; i < swapchainCount; ++i)
		{
			RenderTargetDescription rtDesc;
			rtDesc.RenderArea.extent = { .width = context.Window->Width, .height = context.Window->Height };
			rtDesc.AddColorAttachment(swapchain.GetImageFormat(), IMAGE_LAYOUT_PRESENT_SRC_KHR, SAMPLE_COUNT_1_BIT, { .color = {0.2f, 0.4f, 0.1f, 0.f} });
			rtDesc.AddExternalAttachment(swapchain.GetImageViewAt(i), swapchain.GetImageFormat(), IMAGE_LAYOUT_PRESENT_SRC_KHR, SAMPLE_COUNT_1_BIT, { 0.2f, 0.4f, 0.1f, 0.f });
			rtDesc.ResourceName.Fmt("Swapchaing_%d_RT", i);
			RenderTargetArray[i].Create(context, rtDesc);
		}

		// Init shader
		tShaderProgramDescription shaderDesc;
		shaderDesc.VertexShaderFile.Filepath = globals::PostProcessVertexShader;
		shaderDesc.FragmentShaderFile.Filepath = globals::PostProcessFragmentShader;
		shaderDesc.InputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float3, EAttributeType::Float2 });
		shaderDesc.RenderTarget = &RenderTargetArray[0];
		Shader = ShaderProgram::Create(context, shaderDesc);

		// Init vertexbuffer
		float vertices[] =
		{
			// vkscreencoords	// uvs
			-1.f, -1.f, 0.f,	0.f, 0.f,
			1.f, -1.f, 0.f,		1.f, 0.f,
			1.f, 1.f, 0.f,		1.f, 1.f,
			-1.f, 1.f, 0.f,		0.f, 1.f
		};
		BufferCreateInfo quadInfo;
		quadInfo.Data = vertices;
		quadInfo.Size = sizeof(vertices);
		VB.Init(context, quadInfo);

		uint32_t indices[] = { 0, 2, 1, 0, 3, 2 };
		quadInfo.Data = indices;
		quadInfo.Size = sizeof(indices);
		IB.Init(context, quadInfo);

		// Init ui and debug pipelines
		DebugInstance.Init(context, &RenderTargetArray[0]);
		UIInstance.Init(context, RenderTargetArray[0].GetRenderPass());
	}

	void ScreenQuadPipeline::Destroy(const RenderContext& context)
	{
		UIInstance.Destroy(context);
		DebugInstance.Destroy(context);
		IB.Destroy(context);
		VB.Destroy(context);
		for (uint32_t i = 0; i < (uint32_t)RenderTargetArray.size(); ++i)
		{
			RenderTargetArray[i].Destroy(context);
		}
	}

	void CubemapPipeline::Init(const RenderContext& context, const RenderTarget* rt)
	{
		tShaderProgramDescription shaderDesc;
		shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("skybox.vert");
		shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("skybox.frag");
		shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
		shaderDesc.RenderTarget = rt;
		shaderDesc.CullMode = CULL_MODE_FRONT_BIT;
		shaderDesc.DepthStencilMode = DEPTH_STENCIL_NONE;
		shaderDesc.FrontFaceMode = FRONT_FACE_COUNTER_CLOCKWISE;
		Shader = ShaderProgram::Create(context, shaderDesc);
	}

	void CubemapPipeline::Destroy(const RenderContext& context)
	{
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
			m_renderContext.FrameContextArray[i].FrameIndex = i;
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

		m_screenPipeline.Init(m_renderContext, m_swapchain);
		m_cubemapPipeline.Init(m_renderContext, &m_screenPipeline.RenderTargetArray[0]);
		// Initialize render processes after instantiate render context
		m_renderer.Init(m_renderContext, m_renderContext.FrameContextArray, sizeof(m_renderContext.FrameContextArray) / sizeof(RenderFrameContext));
		DebugRender::Init(m_renderContext);

		const RenderTarget& gbufferRT = *m_renderer.GetRenderProcess(RENDERPROCESS_GBUFFER)->GetRenderTarget();
		const RenderTarget& lightingRT = *m_renderer.GetRenderProcess(RENDERPROCESS_LIGHTING)->GetRenderTarget();
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; i++)
		{
			UniformBufferMemoryPool& buffer = m_renderContext.FrameContextArray[i].GlobalBuffer;

			// Screen quad pipeline
			{
				buffer.AllocUniform(m_renderContext, UNIFORM_ID_SCREEN_QUAD_INDEX, sizeof(int));
				buffer.SetUniform(m_renderContext, UNIFORM_ID_SCREEN_QUAD_INDEX, &m_screenPipeline.QuadIndex, sizeof(int));
				VkDescriptorBufferInfo bufferInfo = buffer.GenerateDescriptorBufferInfo(UNIFORM_ID_SCREEN_QUAD_INDEX);

				DescriptorBuilder builder = DescriptorBuilder::Create(m_descriptorLayoutCache, m_descriptorAllocators[i]);
				builder.BindBuffer(0, &bufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
				builder.Build(m_renderContext, m_screenPipeline.QuadSets[i]);

				VkDescriptorImageInfo quadImageInfoArray;
				quadImageInfoArray.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				quadImageInfoArray.imageView = lightingRT.GetRenderTarget(0);
				quadImageInfoArray.sampler = CreateSampler(m_renderContext);
				DescriptorBuilder::Create(m_descriptorLayoutCache, m_descriptorAllocators[i])
					.BindImage(0, &quadImageInfoArray, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
					.Build(m_renderContext, m_screenPipeline.PresentTexSets[i]);

				m_screenPipeline.DebugInstance.PushFrameData(m_renderContext, &buffer);
			}

			// Cubemap pipeline
			{
				buffer.AllocUniform(m_renderContext, "ProjViewRot", 2 * sizeof(glm::mat4));
				VkDescriptorBufferInfo bufferInfo = buffer.GenerateDescriptorBufferInfo("ProjViewRot");
				DescriptorBuilder::Create(*m_renderContext.LayoutCache, *m_renderContext.DescAllocator)
					.BindBuffer(0, &bufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
					.Build(m_renderContext, m_cubemapPipeline.Sets[i]);
			}

			m_renderContext.FrameContextArray[i].GraphicsTimestampQueryPool.Init(m_renderContext.Device, 20);
			m_renderContext.FrameContextArray[i].ComputeTimestampQueryPool.Init(m_renderContext.Device, 20);
		}


		AddImGuiCallback(&Profiling::ImGuiDraw);
		AddImGuiCallback([this]() { ImGuiDraw(); });
		AddImGuiCallback([this]() { if (m_scene) m_scene->ImGuiDraw(); });

		m_gpuParticleSystem.Init(m_renderContext, &m_screenPipeline.RenderTargetArray[0]);
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
		Profiling::ShowFps(1000.f / (ms));

		bool exitFlag = false;
		struct ProcessEventPayload
		{
			VulkanRenderEngine* Engine;
			bool* Exit;
		} data{ this, &exitFlag };

		ProcessEvents([](void* e, void* userData)
			{
				ProcessEventPayload& payload = *(ProcessEventPayload*)userData;
				SDL_Event& ev = *(SDL_Event*)e;
				switch (ev.type)
				{
				case SDL_QUIT: *payload.Exit = true; break;
				case SDL_WINDOWEVENT:
				{
					uint8_t windowEvent = (*(SDL_WindowEvent*)&ev).event;
					if (windowEvent == SDL_WINDOWEVENT_CLOSE)
						*payload.Exit = true;
					break;
				}
				}
				if (payload.Engine->m_eventCallback)
					payload.Engine->m_eventCallback(&ev);
			}, &data);
		UpdateInputState();

		if (GetKeyboardState(MIST_KEY_CODE_TAB) && !GetKeyboardPreviousState(MIST_KEY_CODE_TAB))
			CVar_ShowConsole.Set(!CVar_ShowConsole.Get());

		m_screenPipeline.UIInstance.BeginFrame(m_renderContext);
		for (auto& fn : m_imguiCallbackArray)
			fn();
		//BeginFrame();
		Mist::Profiling::GRenderStats.Reset();
		Draw();
		return !exitFlag;
	}

	void VulkanRenderEngine::Shutdown()
	{
		loginfo("Shutdown render engine.\n");

		RenderContext_ForceFrameSync(m_renderContext);

		FullscreenQuad.Destroy(m_renderContext);

		if (m_scene)
		{
			m_scene->Destroy();
			delete m_scene;
			m_scene = nullptr;
		}

		m_gpuParticleSystem.Destroy(m_renderContext);

		DebugRender::Destroy(m_renderContext);
		m_renderer.Destroy(m_renderContext);
		m_screenPipeline.Destroy(m_renderContext);

		if (DefaultTexture)
			cTexture::Destroy(m_renderContext, DefaultTexture);
		DefaultTexture = nullptr;
		DestroySamplers(m_renderContext);
		vkDestroyFence(m_renderContext.Device, m_renderContext.TransferContext.Fence, nullptr);
		vkDestroyCommandPool(m_renderContext.Device, m_renderContext.TransferContext.CommandPool, nullptr);
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			m_renderContext.FrameContextArray[i].GraphicsTimestampQueryPool.Destroy(m_renderContext.Device);
			m_renderContext.FrameContextArray[i].ComputeTimestampQueryPool.Destroy(m_renderContext.Device);
			m_renderContext.FrameContextArray[i].GlobalBuffer.Destroy(m_renderContext);

			vkDestroyFence(m_renderContext.Device, m_renderContext.FrameContextArray[i].ComputeCommandContext.Fence, nullptr);
			vkDestroyFence(m_renderContext.Device, m_renderContext.FrameContextArray[i].GraphicsCommandContext.Fence, nullptr);
			vkDestroySemaphore(m_renderContext.Device, m_renderContext.FrameContextArray[i].ComputeSemaphore, nullptr);
			vkDestroySemaphore(m_renderContext.Device, m_renderContext.FrameContextArray[i].RenderSemaphore, nullptr);
			vkDestroySemaphore(m_renderContext.Device, m_renderContext.FrameContextArray[i].PresentSemaphore, nullptr);
			vkDestroyCommandPool(m_renderContext.Device, m_renderContext.FrameContextArray[i].GraphicsCommandContext.CommandPool, nullptr);
			vkDestroyCommandPool(m_renderContext.Device, m_renderContext.FrameContextArray[i].ComputeCommandContext.CommandPool, nullptr);
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
		// TODO descriptor allocators must be instanciated on each frame context.
		m_renderContext.DescAllocator = &m_descriptorAllocators[m_renderContext.GetFrameIndex()];
		RenderFrameContext& frameContext = GetFrameContext();
		frameContext.DescriptorAllocator = m_renderContext.DescAllocator;

		glm::mat4 viewRot = m_cameraData.InvView;
		viewRot[3] = { 0.f,0.f,0.f,1.f };
		glm::mat4 ubo[2];
		ubo[0] = viewRot;
		ubo[1] = m_cameraData.Projection * viewRot;
		frameContext.GlobalBuffer.SetUniform(m_renderContext, "ProjViewRot", &ubo, 2 * sizeof(glm::mat4));
		frameContext.GlobalBuffer.SetUniform(m_renderContext, UNIFORM_ID_CAMERA, &m_cameraData, sizeof(CameraData));
		frameContext.GlobalBuffer.SetUniform(m_renderContext, UNIFORM_ID_SCREEN_QUAD_INDEX, &m_screenPipeline.QuadIndex, sizeof(uint32_t));

		UBOTime time{ 0.033f, 0.f };
		frameContext.GlobalBuffer.SetUniform(m_renderContext, UNIFORM_ID_TIME, &time, sizeof(UBOTime));

		frameContext.PresentTex = m_screenPipeline.PresentTexSets[m_renderContext.GetFrameIndex()];
		frameContext.Scene->UpdateRenderData(m_renderContext, frameContext);
		m_screenPipeline.DebugInstance.PrepareFrame(m_renderContext, &frameContext.GlobalBuffer);
		m_renderer.UpdateRenderData(m_renderContext, frameContext);
		m_gpuParticleSystem.UpdateBuffers(m_renderContext, frameContext);
	}

	void VulkanRenderEngine::Draw()
	{
		CPU_PROFILE_SCOPE(Draw);
		RenderContext_NewFrame(m_renderContext);
		uint32_t frameIndex = m_renderContext.GetFrameIndex();
		RenderFrameContext& frameContext = GetFrameContext();
		frameContext.Scene = static_cast<Scene*>(m_scene);

		BeginFrame();
		{
			CPU_PROFILE_SCOPE(PrepareFrame);
			// Acquire render image from swapchain
			vkcheck(vkAcquireNextImageKHR(m_renderContext.Device, m_swapchain.GetSwapchainHandle(), 1000000000, frameContext.PresentSemaphore,
				nullptr, &m_currentSwapchainIndex));
		}

		// Compute
		{
			frameContext.ComputeCommandContext.ResetCommandBuffer();
			frameContext.ComputeCommandContext.BeginCommandBuffer();
			BeginGPUEvent(m_renderContext, frameContext.ComputeCommandContext.CommandBuffer, "Begin Compute");
			m_gpuParticleSystem.Dispatch(m_renderContext, frameIndex);
			frameContext.ComputeCommandContext.EndCommandBuffer();
			EndGPUEvent(m_renderContext, frameContext.ComputeCommandContext.CommandBuffer);
		}

		// Graphics
		{
			frameContext.GraphicsCommandContext.ResetCommandBuffer();
			frameContext.GraphicsCommandContext.BeginCommandBuffer();
			VkCommandBuffer cmd = frameContext.GraphicsCommandContext.CommandBuffer;

			// Timestamp queries
			GpuProf_Reset(m_renderContext);

			BeginGPUEvent(m_renderContext, cmd, "Begin Graphics", 0xff0000ff);


			GpuProf_Begin(m_renderContext, "Graphics_Renderer");
			m_renderer.Draw(m_renderContext, frameContext);
			GpuProf_End(m_renderContext);

			m_gpuParticleSystem.Draw(m_renderContext, frameContext);


			GpuProf_Begin(m_renderContext, "Graphics_ScreenDraw");
			BeginGPUEvent(m_renderContext, cmd, "ScreenDraw");
			m_screenPipeline.RenderTargetArray[m_currentSwapchainIndex].BeginPass(m_renderContext, cmd);
			// Skybox
			if (m_scene)
			{
				m_cubemapPipeline.Shader->UseProgram(cmd);
				m_cubemapPipeline.Shader->BindDescriptorSets(cmd, &m_cubemapPipeline.Sets[frameIndex], 1, 0, nullptr, 0);
				m_scene->DrawSkybox(cmd, m_cubemapPipeline.Shader);
			}



			// Post process
			m_screenPipeline.VB.Bind(cmd);
			m_screenPipeline.IB.Bind(cmd);
			m_screenPipeline.Shader->UseProgram(cmd);
			VkDescriptorSet sets[] = { m_screenPipeline.QuadSets[frameIndex], m_screenPipeline.PresentTexSets[frameIndex] };
			m_screenPipeline.Shader->BindDescriptorSets(cmd, sets, sizeof(sets) / sizeof(VkDescriptorSet));
			vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
			// Flush debug draw list
			m_screenPipeline.DebugInstance.Draw(m_renderContext, cmd, frameIndex);
			// UI
			m_screenPipeline.UIInstance.Draw(m_renderContext, cmd);

			m_screenPipeline.RenderTargetArray[m_currentSwapchainIndex].EndPass(cmd);
			// Terminate command buffer
			GpuProf_End(m_renderContext);
			frameContext.GraphicsCommandContext.EndCommandBuffer();
			EndGPUEvent(m_renderContext, cmd);
			frameContext.GraphicsTimestampQueryPool.EndFrame();
		}

		{
			CPU_PROFILE_SCOPE(QueueSubmit);

			// Submit Compute Queue 
			VkSubmitInfo computeSubmitInfo{};
			computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			computeSubmitInfo.pNext = nullptr;
			VkPipelineStageFlags waitComputeStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			computeSubmitInfo.pWaitDstStageMask = &waitComputeStage;
			// Wait for last frame terminates present image
			computeSubmitInfo.waitSemaphoreCount = 0;
			computeSubmitInfo.pWaitSemaphores = nullptr;
			// Make wait present process until this Queue has finished.
			computeSubmitInfo.signalSemaphoreCount = 1;
			computeSubmitInfo.pSignalSemaphores = &frameContext.ComputeSemaphore;
			// The command buffer will be procesed
			computeSubmitInfo.commandBufferCount = 1;
			computeSubmitInfo.pCommandBuffers = &frameContext.ComputeCommandContext.CommandBuffer;
			vkcheck(vkQueueSubmit(m_renderContext.ComputeQueue, 1, &computeSubmitInfo, frameContext.ComputeCommandContext.Fence));

			// Submit command buffer
			VkSemaphore graphicsWaitSemaphores[] = { frameContext.PresentSemaphore, frameContext.ComputeSemaphore };
			uint32_t waitSemaphoresCount = sizeof(graphicsWaitSemaphores) / sizeof(VkSemaphore);
			VkSubmitInfo graphicsSubmitInfo = {};
			graphicsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			graphicsSubmitInfo.pNext = nullptr;
			VkPipelineStageFlags waitStage[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT };
			graphicsSubmitInfo.pWaitDstStageMask = waitStage;
			// Wait for last frame terminates present image
			graphicsSubmitInfo.waitSemaphoreCount = waitSemaphoresCount;
			graphicsSubmitInfo.pWaitSemaphores = graphicsWaitSemaphores;
			// Make wait present process until this Queue has finished.
			graphicsSubmitInfo.signalSemaphoreCount = 1;
			graphicsSubmitInfo.pSignalSemaphores = &frameContext.RenderSemaphore;
			// The command buffer will be procesed
			graphicsSubmitInfo.commandBufferCount = 1;
			graphicsSubmitInfo.pCommandBuffers = &frameContext.GraphicsCommandContext.CommandBuffer;
			vkcheck(vkQueueSubmit(m_renderContext.GraphicsQueue, 1, &graphicsSubmitInfo, frameContext.GraphicsCommandContext.Fence));
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
		m_renderer.ImGuiDraw();
		ImGuiDrawInputState();

		if (CVar_ShowConsole.Get())
			DrawConsole();
		if (CVar_ShowImGuiDemo.Get())
			ImGui::ShowDemoWindow();

#if 0
		ImGui::Begin("Engine");

		auto lmbShowMemStat = [](const char* label, uint32_t allocated, uint32_t maxAllocated)
			{
				ImGui::Text("%s:		%.3f MB | %.3f MB max", label, (float)allocated / 1024.f / 1024.f, (float)maxAllocated / 1024.f / 1024.f);
				ImGui::NextColumn();
				ImGui::ProgressBar((float)allocated / (float)maxAllocated);
				ImGui::NextColumn();
			};

		const tSystemMemStats& systemStats = GetMemoryStats();
		const tMemStats& bufferStats = m_renderContext.Allocator->BufferStats;
		const tMemStats& texStats = m_renderContext.Allocator->TextureStats;
		ImGui::Columns(2);
		lmbShowMemStat("System memory", systemStats.Allocated, systemStats.MaxAllocated);
		lmbShowMemStat("GPU buffer memory", bufferStats.Allocated, bufferStats.MaxAllocated);
		lmbShowMemStat("GPU texture memory", texStats.Allocated, texStats.MaxAllocated);
		ImGui::Columns();



		ImGui::Separator();
		if (ImGui::TreeNode("Graphics Shaders"))
		{
			ImGui::Columns(3);
			ShaderProgram** shaderArray = m_shaderDb.GetShaderArray();
			uint32_t shaderCount = m_shaderDb.GetShaderCount();
			for (uint32_t i = 0; i < shaderCount; ++i)
			{
				char label[32];
				sprintf_s(label, "Reload_%d", i);
				if (ImGui::Button(label))
				{
					RenderContext_ForceFrameSync(m_renderContext);
					shaderArray[i]->Destroy(m_renderContext);
					shaderArray[i]->Reload(m_renderContext);
				}
				ImGui::NextColumn();
				const tShaderProgramDescription& shaderDesc = shaderArray[i]->GetDescription();
				ImGui::Text("%s", shaderDesc.VertexShaderFile.Filepath.c_str());
				ImGui::NextColumn();
				ImGui::Text("%s", shaderDesc.FragmentShaderFile.Filepath.c_str());
				ImGui::NextColumn();
			}
			ImGui::Columns();
			ImGui::TreePop();
		}
		if (ImGui::Button("Reload all"))
		{
			RenderContext_ForceFrameSync(m_renderContext);
			for (uint32_t i = 0; i < m_shaderDb.GetShaderCount(); ++i)
			{
				m_shaderDb.GetShaderArray()[i]->Destroy(m_renderContext);
				m_shaderDb.GetShaderArray()[i]->Reload(m_renderContext);
			}
		}
		ImGui::End();
#endif // 0

#if 0
		ImGui::Begin("Gpu queries");
		if (m_renderContext.FrameIndex > globals::MaxOverlappedFrames && m_renderContext.FrameIndex % globals::MaxOverlappedFrames == 0)
		{
			uint32_t prevframe = m_renderContext.FrameIndex - 1;
			RenderFrameContext& frameContext = m_renderContext.FrameContextArray[prevframe % globals::MaxOverlappedFrames];
			if (frameContext.GraphicsTimestampQueryPool.Flags == sTimestampQueryPool::QueryPool_Terminated)
			{
				uint64_t ns = frameContext.GraphicsTimestampQueryPool.GetElapsedTimestamp(m_renderContext.Device, m_renderContext.GPUDevice, m_drawQueryTimestamp);
				double us = (double)ns / 1000.0;
				ImGui::Text("%f us", us);
			}
		}
		ImGui::End();
#else
		GpuProf_Resolve(m_renderContext);
		GpuProf_ImGuiDraw(m_renderContext);
#endif // 0

		m_gpuParticleSystem.ImGuiDraw();
	}


	RenderFrameContext& VulkanRenderEngine::GetFrameContext()
	{
		return m_renderContext.GetFrameContext();
	}

	bool VulkanRenderEngine::InitVulkan()
	{
#if 0
		{
			// Create Vulkan instance
			uint32_t extensionCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
			vkEnumerateInstanceExtensionProperties()

				VkInstance instance;
			VkApplicationInfo appInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pNext = nullptr };
			appInfo.pApplicationName = "Mist";
			appInfo.applicationVersion = VK_API_VERSION_1_1;
			appInfo.apiVersion = VK_API_VERSION_1_1;
			appInfo.engineVersion = VK_API_VERSION_1_1;
			appInfo.pEngineName = "Mist";

			tDynArray<const char*> extensions;
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			tDynArray<const char*> layers;
			layers.push_back("VK_LAYER_KHRONOS_validation");

			VkInstanceCreateInfo instanceCreateInfo{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pNext = nullptr };
			//instanceCreateInfo.
		}
#endif // 0


		vkb::InstanceBuilder builder;
		vkb::Result<vkb::Instance> instanceReturn = builder
			.set_app_name("Vulkan renderer")
			.request_validation_layers(true)
			.require_api_version(1, 1, 0)
			//.use_default_debug_messenger()
			.set_debug_callback(&Mist::Debug::DebugVulkanCallback)
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
		VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParamsFeatures = {};
		shaderDrawParamsFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
		shaderDrawParamsFeatures.pNext = nullptr;
		shaderDrawParamsFeatures.shaderDrawParameters = VK_TRUE;
		deviceBuilder.add_pNext(&shaderDrawParamsFeatures);
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
#if 1
		m_renderContext.ComputeQueue = device.get_queue(vkb::QueueType::compute).value();
		m_renderContext.ComputeQueueFamily = device.get_queue_index(vkb::QueueType::compute).value();
		check(m_renderContext.ComputeQueue != VK_NULL_HANDLE);
		check(m_renderContext.ComputeQueueFamily != (uint32_t)vkb::QueueError::invalid_queue_family_index
			&& m_renderContext.ComputeQueueFamily != vkb::detail::QUEUE_INDEX_MAX_VALUE);

#else
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(m_renderContext.GPUDevice, &queueFamilyCount, nullptr);
		VkQueueFamilyProperties* queueFamilyProperties = _newVkQueueFamilyProperties[queueFamilyCount];
		vkGetPhysicalDeviceQueueFamilyProperties(m_renderContext.GPUDevice, &queueFamilyCount, queueFamilyProperties);

		uint32_t graphicsComputeQueueIndex = UINT32_MAX;
		for (uint32_t i = 0; i < queueFamilyCount; ++i)
		{
			if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT
				&& queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				graphicsComputeQueueIndex = i;
				break;
			}
		}

		check(graphicsComputeQueueIndex != UINT32_MAX);
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
		for (size_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			RenderFrameContext& frameContext = m_renderContext.FrameContextArray[i];

			// Graphics commands
			{
				vkcheck(vkCreateCommandPool(m_renderContext.Device, &graphicsPoolInfo, nullptr, &frameContext.GraphicsCommandContext.CommandPool));
				VkCommandBufferAllocateInfo allocInfo = vkinit::CommandBufferCreateAllocateInfo(frameContext.GraphicsCommandContext.CommandPool);
				vkcheck(vkAllocateCommandBuffers(m_renderContext.Device, &allocInfo, &frameContext.GraphicsCommandContext.CommandBuffer));
			}

			// Compute commands
			{
				vkcheck(vkCreateCommandPool(m_renderContext.Device, &computePoolInfo, nullptr, &frameContext.ComputeCommandContext.CommandPool));
				VkCommandBufferAllocateInfo allocInfo = vkinit::CommandBufferCreateAllocateInfo(frameContext.ComputeCommandContext.CommandPool);
				vkcheck(vkAllocateCommandBuffers(m_renderContext.Device, &allocInfo, &frameContext.ComputeCommandContext.CommandBuffer));
			}
		}

		vkcheck(vkCreateCommandPool(m_renderContext.Device, &graphicsPoolInfo, nullptr, &m_renderContext.TransferContext.CommandPool));
		VkCommandBufferAllocateInfo allocInfo = vkinit::CommandBufferCreateAllocateInfo(m_renderContext.TransferContext.CommandPool, 1);
		vkcheck(vkAllocateCommandBuffers(m_renderContext.Device, &allocInfo, &m_renderContext.TransferContext.CommandBuffer));
		return true;
	}


	bool VulkanRenderEngine::InitSync()
	{
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			RenderFrameContext& frameContext = m_renderContext.FrameContextArray[i];
			// Render fence
			VkFenceCreateInfo fenceInfo = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
			vkcheck(vkCreateFence(m_renderContext.Device, &fenceInfo, nullptr, &frameContext.GraphicsCommandContext.Fence));
			vkcheck(vkCreateFence(m_renderContext.Device, &fenceInfo, nullptr, &frameContext.ComputeCommandContext.Fence));

			// Render semaphore
			VkSemaphoreCreateInfo semaphoreInfo = vkinit::SemaphoreCreateInfo();
			vkcheck(vkCreateSemaphore(m_renderContext.Device, &semaphoreInfo, nullptr, &frameContext.RenderSemaphore));
			vkcheck(vkCreateSemaphore(m_renderContext.Device, &semaphoreInfo, nullptr, &frameContext.ComputeSemaphore));
			// Present semaphore
			vkcheck(vkCreateSemaphore(m_renderContext.Device, &semaphoreInfo, nullptr, &frameContext.PresentSemaphore));

			char buff[256];
			sprintf_s(buff, "GraphicsFence_%u", i);
			SetVkObjectName(m_renderContext, &frameContext.GraphicsCommandContext.Fence, VK_OBJECT_TYPE_FENCE, buff);
			sprintf_s(buff, "ComputeFence_%u", i);
			SetVkObjectName(m_renderContext, &frameContext.ComputeCommandContext.Fence, VK_OBJECT_TYPE_FENCE, buff);
			sprintf_s(buff, "RenderSemaphore_%u", i);
			SetVkObjectName(m_renderContext, &frameContext.RenderSemaphore, VK_OBJECT_TYPE_SEMAPHORE, buff);
			sprintf_s(buff, "ComputeSemaphore_%u", i);
			SetVkObjectName(m_renderContext, &frameContext.ComputeSemaphore, VK_OBJECT_TYPE_SEMAPHORE, buff);
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

			// Size for uniform frame buffer
			uint32_t size = 1024 * 1024; // 1MB
			frameContext.GlobalBuffer.Init(m_renderContext, size, BUFFER_USAGE_UNIFORM);

			// Update global descriptors
			uint32_t cameraBufferSize = sizeof(CameraData);
			uint32_t cameraBufferOffset = frameContext.GlobalBuffer.AllocUniform(m_renderContext, UNIFORM_ID_CAMERA, cameraBufferSize);
			VkDescriptorBufferInfo bufferInfo;
			bufferInfo.buffer = frameContext.GlobalBuffer.GetBuffer();
			bufferInfo.offset = cameraBufferOffset;
			bufferInfo.range = cameraBufferSize;
			DescriptorBuilder::Create(m_descriptorLayoutCache, m_descriptorAllocators[i])
				.BindBuffer(0, &bufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
				.Build(m_renderContext, frameContext.CameraDescriptorSet, m_globalDescriptorLayout);

			// Scene buffer allocation. TODO: this should be done by scene.
			frameContext.GlobalBuffer.AllocDynamicUniform(m_renderContext, UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY, sizeof(glm::mat4), globals::MaxRenderObjects);
			frameContext.GlobalBuffer.AllocUniform(m_renderContext, UNIFORM_ID_SCENE_ENV_DATA, sizeof(EnvironmentData));
			frameContext.GlobalBuffer.AllocUniform(m_renderContext, UNIFORM_ID_TIME, sizeof(UBOTime));
		}
		return true;
	}
}