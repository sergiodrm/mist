#include "VulkanRenderEngine.h"

#include <cstdio>
#include <glm/gtx/transform.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <string.h>

#include <vkbootstrap/VkBootstrap.h>
#include "InitVulkanTypes.h"
#include "Mesh.h"
#include "RenderDescriptor.h"
#include "Camera.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_sdl2.h>
#include "Logger.h"
#include "Debug.h"
#include "Shader.h"
#include "Globals.h"
#include "Renderers/ModelRenderer.h"
#include "Renderers/DebugRenderer.h"
#include "Renderers/UIRenderer.h"
#include "SceneImpl.h"
#include "GenericUtils.h"
#include "Renderers/QuadRenderer.h"
#include "RenderTarget.h"

#define VKMMC_CRASH_ON_VALIDATION_LAYER

#define UNIFORM_ID_SCREEN_QUAD_INDEX "ScreenQuadIndex"
#define MAX_RT_SCREEN 6

namespace vkmmc_debug
{
	uint32_t GVulkanLayerValidationErrors = 0;

	VkBool32 DebugVulkanCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
		void* userData)
	{
		vkmmc::LogLevel level = vkmmc::LogLevel::Info;
		switch (severity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: level = vkmmc::LogLevel::Error; ++GVulkanLayerValidationErrors; break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: level = vkmmc::LogLevel::Debug; break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: level = vkmmc::LogLevel::Warn; break;
		}
		Logf(level, "\nValidation layer\n> Message: %s\n\n", callbackData->pMessage);
		if (level == vkmmc::LogLevel::Error)
		{
#if defined(_DEBUG)
			PrintCallstack();
#endif
#ifdef VKMMC_CRASH_ON_VALIDATION_LAYER
			check(false && "Validation layer error");
#endif
		}
		return VK_FALSE;
	}
}

namespace vkmmc
{
	RenderHandle GenerateRenderHandle()
	{
		static RenderHandle h;
		++h.Handle;
		return h;
	}

	Window Window::Create(uint32_t width, uint32_t height, const char* title)
	{
		Window newWindow;
		newWindow.Width = width;
		newWindow.Height = height;
		strcpy_s(newWindow.Title, title);
		SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
		newWindow.WindowInstance = SDL_CreateWindow(
			title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			width, height, windowFlags
		);

		return newWindow;
	}

	void ScreenQuadPipeline::Init(const RenderContext& context, const Swapchain& swapchain)
	{
		RenderTargetDescription rtDesc;
		rtDesc.RenderArea.extent = { .width = context.Window->Width, .height = context.Window->Height };
		rtDesc.AddColorAttachment(swapchain.GetImageFormat(), IMAGE_LAYOUT_PRESENT_SRC_KHR, SAMPLE_COUNT_1_BIT, { .color = {1.f, 0.f, 0.f, 1.f} });
		rtDesc.ExternalAttachmentCount = 1;

		uint32_t swapchainCount = swapchain.GetImageCount();
		RenderTargetArray.resize(swapchainCount);
		for (uint32_t i = 0; i < swapchainCount; ++i)
		{
			rtDesc.ExternalAttachments[0] = swapchain.GetImageViewAt(i);
			RenderTargetArray[i].Create(context, rtDesc);
		}

		// Init shader
		ShaderProgramDescription shaderDesc;
		shaderDesc.VertexShaderFile = globals::PostProcessVertexShader;
		shaderDesc.FragmentShaderFile = globals::PostProcessFragmentShader;
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

	bool VulkanRenderEngine::Init(const InitializationSpecs& spec)
	{
		CPU_PROFILE_SCOPE(Init);
		InitLog("../../log.html");
#ifdef _DEBUG
		Log(LogLevel::Info, "Running app in DEBUG mode.\n");
#else
		Log(LogLevel::Info, "Running app in RELEASE mode.\n");
#endif // _DEBUG

		Log(LogLevel::Info, "Initialize render engine.\n");
		SDL_Init(SDL_INIT_VIDEO);
		m_window = Window::Create(spec.WindowWidth, spec.WindowHeight, spec.WindowTitle);
		m_renderContext.Window = &m_window;
		Log(LogLevel::Ok, "Window created successfully!\n");
		
		// Init vulkan context
		check(InitVulkan());

		// Descriptor layout and allocator
		m_descriptorLayoutCache.Init(m_renderContext);
		m_descriptorAllocator.Init(m_renderContext, DescriptorPoolSizes::GetDefault());
		m_renderContext.LayoutCache = &m_descriptorLayoutCache;
		m_renderContext.DescAllocator = &m_descriptorAllocator;
		m_renderContext.ShaderDB = &m_shaderDb;
		m_shutdownStack.Add([this]() mutable
			{
				m_descriptorAllocator.Destroy();
				m_descriptorLayoutCache.Destroy();
				m_shaderDb.Destroy(m_renderContext);
			});

		// Swapchain
		check(m_swapchain.Init(m_renderContext, { spec.WindowWidth, spec.WindowHeight }));
		m_shutdownStack.Add(
			[this]()
			{
				m_swapchain.Destroy(m_renderContext);
			}
		);

		// Commands
		check(InitCommands());

		// RenderPass
		check(InitRenderPass());
		// Framebuffers
		check(InitFramebuffers());
		// Pipelines
		check(InitPipeline());

		// Init sync vars
		check(InitSync());

		RendererCreateInfo rendererCreateInfo;
		rendererCreateInfo.Context = m_renderContext;
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			rendererCreateInfo.FrameUniformBufferArray[i] = &m_frameContextArray[i].GlobalBuffer;
			//rendererCreateInfo.ShadowMapAttachments[i] = m_renderTargetArray[i][RENDER_PASS_SHADOW_MAP].GetRenderTarget(0);
		}

		rendererCreateInfo.ConstantRange = nullptr;
		rendererCreateInfo.ConstantRangeCount = 0;

		LightingRenderer* lightingRenderer = new LightingRenderer();
		//UIRenderer* uiRenderer = new UIRenderer();

		m_renderers[RENDER_PASS_SHADOW_MAP].push_back(new ShadowMapRenderer());
		m_renderers[RENDER_PASS_LIGHTING].push_back(lightingRenderer);
		//m_renderers[RENDER_PASS_POST_PROCESS].push_back(uiRenderer);
		//m_renderers[RENDER_PASS_POST_PROCESS].push_back(new QuadRenderer());
		for (uint32_t i = 0; i < RENDER_PASS_COUNT; i++)
		{
			//rendererCreateInfo.Pass = m_renderTargetArray[0][i].GetRenderPass();
			for (IRendererBase* renderer : m_renderers[i])
				renderer->Init(rendererCreateInfo);
		}

		m_gbuffer.Init(m_renderContext);
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
			m_gbuffer.InitFrameData(m_renderContext, &m_frameContextArray[i].GlobalBuffer, i);
		m_shutdownStack.Add([this] { m_gbuffer.Destroy(m_renderContext); });

		m_screenPipeline.Init(m_renderContext, m_swapchain);
		m_shutdownStack.Add([this] { m_screenPipeline.Destroy(m_renderContext); });

#if 1
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; i++)
		{
			tArray<VkDescriptorImageInfo, MAX_RT_SCREEN> quadImageInfoArray;
			quadImageInfoArray[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			quadImageInfoArray[0].imageView = lightingRenderer->GetRenderTarget(i, 0);
			quadImageInfoArray[0].sampler = m_quadSampler.GetSampler();
			quadImageInfoArray[1].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			quadImageInfoArray[1].imageView = lightingRenderer->GetDepthBuffer(i, 0);
			quadImageInfoArray[1].sampler = m_quadSampler.GetSampler();
			quadImageInfoArray[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			quadImageInfoArray[2].imageView = m_gbuffer.GetRenderTarget(GBuffer::RT_POSITION);
			quadImageInfoArray[2].sampler = m_quadSampler.GetSampler();
			quadImageInfoArray[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			quadImageInfoArray[3].imageView = m_gbuffer.GetRenderTarget(GBuffer::RT_NORMAL);
			quadImageInfoArray[3].sampler = m_quadSampler.GetSampler();
			quadImageInfoArray[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			quadImageInfoArray[4].imageView = m_gbuffer.GetRenderTarget(GBuffer::RT_ALBEDO);
			quadImageInfoArray[4].sampler = m_quadSampler.GetSampler();
			quadImageInfoArray[5].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			quadImageInfoArray[5].imageView = m_gbuffer.GetComposition();
			quadImageInfoArray[5].sampler = m_quadSampler.GetSampler();

			UniformBuffer& buffer = m_frameContextArray[i].GlobalBuffer;
			buffer.AllocUniform(m_renderContext, UNIFORM_ID_SCREEN_QUAD_INDEX, sizeof(int));
			buffer.SetUniform(m_renderContext, UNIFORM_ID_SCREEN_QUAD_INDEX, &m_screenPipeline.QuadIndex, sizeof(int));
			VkDescriptorBufferInfo bufferInfo = buffer.GenerateDescriptorBufferInfo(UNIFORM_ID_SCREEN_QUAD_INDEX);

			DescriptorBuilder builder = DescriptorBuilder::Create(m_descriptorLayoutCache, m_descriptorAllocator);
			/*for (uint32_t j = 0; j < (uint32_t)quadImageInfoArray.size(); ++j)
				builder.BindImage(j, &quadImageInfoArray[j], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);*/
			builder.BindImage(0, quadImageInfoArray.data(), (uint32_t)quadImageInfoArray.size(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
			builder.BindBuffer(1, &bufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
			builder.Build(m_renderContext, m_screenPipeline.QuadSets[i]);

			m_screenPipeline.DebugInstance.PushFrameData(m_renderContext, &buffer);
		}
#endif // 0


		AddImGuiCallback(&vkmmc_profiling::ImGuiDraw);
		AddImGuiCallback([this]() { ImGuiDraw(); });
		AddImGuiCallback([this]() { if (m_scene) m_scene->ImGuiDraw(true); });

		return true;
	}

	bool VulkanRenderEngine::RenderProcess()
	{
		CPU_PROFILE_SCOPE(Process);
		bool res = true;
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			ImGui_ImplSDL2_ProcessEvent(&e);
			switch (e.type)
			{
			case SDL_QUIT: res = false; break;
			}
			const uint8_t* keystate = SDL_GetKeyboardState(nullptr);
			static int s = 0;
			int c = keystate[SDL_SCANCODE_SPACE];
			if (s != c)
			{
				s = c;
				if (c)
					m_screenPipeline.QuadIndex = ++m_screenPipeline.QuadIndex % MAX_RT_SCREEN;
			}
			else if (keystate[SDL_SCANCODE_ESCAPE])
				m_screenPipeline.QuadIndex = -1;
			if (m_eventCallback)
				m_eventCallback(&e);
		}

		BeginFrame();
		for (auto& fn : m_imguiCallbackArray)
			fn();
		vkmmc_profiling::GRenderStats.Reset();
		Draw();
		return res;
	}

	void VulkanRenderEngine::Shutdown()
	{
		Log(LogLevel::Info, "Shutdown render engine.\n");

		for (size_t i = 0; i < globals::MaxOverlappedFrames; ++i)
			WaitFence(m_frameContextArray[i].RenderFence);

		if (m_scene)
			IScene::DestroyScene(m_scene);

		for (uint32_t i = 0; i < RENDER_PASS_COUNT; ++i)
		{
			for (IRendererBase* it : m_renderers[i])
			{
				it->Destroy(m_renderContext);
				delete it;
			}
		}

		m_shutdownStack.Flush();
		vkDestroyDevice(m_renderContext.Device, nullptr);
		vkDestroySurfaceKHR(m_renderContext.Instance, m_renderContext.Surface, nullptr);
		vkb::destroy_debug_utils_messenger(m_renderContext.Instance,
			m_renderContext.DebugMessenger);
		vkDestroyInstance(m_renderContext.Instance, nullptr);
		SDL_DestroyWindow(m_window.WindowInstance);

		Log(LogLevel::Ok, "Render engine terminated.\n");
		Logf(vkmmc_debug::GVulkanLayerValidationErrors > 0 ? LogLevel::Error : LogLevel::Ok, 
			"Total vulkan layer validation errors: %u.\n", vkmmc_debug::GVulkanLayerValidationErrors);
		TerminateLog();
	}

	void VulkanRenderEngine::UpdateSceneView(const glm::mat4& view, const glm::mat4& projection)
	{
		m_cameraData.View = glm::inverse(view);
		m_cameraData.Projection = projection;
		m_cameraData.ViewProjection = m_cameraData.Projection * m_cameraData.View;
	}

	IScene* VulkanRenderEngine::GetScene()
	{
		return m_scene;
	}

	const IScene* VulkanRenderEngine::GetScene() const
	{
		return m_scene;
	}

	void VulkanRenderEngine::SetScene(IScene* scene)
	{
		m_scene = static_cast<Scene*>(scene);

		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{ 
			m_frameContextArray[i].Scene = m_scene;
		}
	}

	RenderHandle VulkanRenderEngine::GetDefaultTexture() const
	{
		static RenderHandle texture;
		if (!texture.IsValid())
			texture = m_scene->LoadTexture("../../assets/textures/checkerboard.jpg");
		return texture;
	}

	Material VulkanRenderEngine::GetDefaultMaterial() const
	{
		static Material material;
		if (!material.GetHandle().IsValid())
		{
			m_scene->SubmitMaterial(material);
		}
		return material;
	}

	void VulkanRenderEngine::BeginFrame()
	{
		// Update scene graph data
		RenderFrameContext& frameContext = GetFrameContext();
		glm::vec3 cameraPos = math::GetPos(glm::inverse(frameContext.CameraData->View));
		frameContext.Scene->UpdateRenderData(m_renderContext, &frameContext.GlobalBuffer, cameraPos);

		// Renderers do your things...
		for (uint32_t i = 0; i < RENDER_PASS_COUNT; i++)
		{
			for (IRendererBase* it : m_renderers[i])
				it->PrepareFrame(m_renderContext, GetFrameContext());
		}

		frameContext.GlobalBuffer.SetUniform(m_renderContext, UNIFORM_ID_SCREEN_QUAD_INDEX, &m_screenPipeline.QuadIndex, sizeof(uint32_t));

		m_screenPipeline.DebugInstance.PrepareFrame(m_renderContext, &frameContext.GlobalBuffer);
		m_screenPipeline.UIInstance.BeginFrame(m_renderContext);
	}

	void VulkanRenderEngine::Draw()
	{
		CPU_PROFILE_SCOPE(Draw);
		RenderFrameContext& frameContext = GetFrameContext();
		frameContext.Scene = static_cast<Scene*>(m_scene);
		WaitFence(frameContext.RenderFence);

		{
			CPU_PROFILE_SCOPE(UpdateBuffers);
			frameContext.GlobalBuffer.SetUniform(m_renderContext, UNIFORM_ID_CAMERA, &m_cameraData, sizeof(CameraData));
		}

		VkCommandBuffer cmd = frameContext.GraphicsCommand;
		{
			CPU_PROFILE_SCOPE(PrepareFrame);
			// Acquire render image from swapchain
			vkcheck(vkAcquireNextImageKHR(m_renderContext.Device, m_swapchain.GetSwapchainHandle(), 1000000000, frameContext.PresentSemaphore, 
				nullptr, &m_currentSwapchainIndex));

			// Reset command buffer
			vkcheck(vkResetCommandBuffer(frameContext.GraphicsCommand, 0));

			// Prepare command buffer
			VkCommandBufferBeginInfo cmdBeginInfo = {};
			cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cmdBeginInfo.pNext = nullptr;
			cmdBeginInfo.pInheritanceInfo = nullptr;
			cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			// Begin command buffer
			vkcheck(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
			BeginGPUEvent(m_renderContext, cmd, "Begin CMD", 0xff0000ff);
		}

		{
			m_gbuffer.DrawPass(m_renderContext, frameContext);

			uint32_t frameIndex = GetFrameIndex();
			render::DrawQuadTexture(m_screenPipeline.QuadSets[frameIndex]);
			for (uint32_t pass = RENDER_PASS_SHADOW_MAP; pass < RENDER_PASS_COUNT; ++pass)
				DrawPass(cmd, frameIndex, (ERenderPassType)pass);

			BeginGPUEvent(m_renderContext, cmd, "ScreenDraw");
			m_screenPipeline.RenderTargetArray[m_currentSwapchainIndex].BeginPass(cmd);
			// Post process
			m_screenPipeline.VB.Bind(cmd);
			m_screenPipeline.IB.Bind(cmd);
			m_screenPipeline.Shader->UseProgram(cmd);
			m_screenPipeline.Shader->BindDescriptorSets(cmd, &m_screenPipeline.QuadSets[frameIndex], 1);
			vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
			// Flush debug draw list
			m_screenPipeline.DebugInstance.Draw(m_renderContext, cmd, frameIndex);
			// UI
			m_screenPipeline.UIInstance.Draw(m_renderContext, cmd);

			m_screenPipeline.RenderTargetArray[m_currentSwapchainIndex].EndPass(cmd);
			EndGPUEvent(m_renderContext, cmd);
		}


		// Terminate command buffer
		vkcheck(vkEndCommandBuffer(cmd));
		EndGPUEvent(m_renderContext, cmd);


		{
			CPU_PROFILE_SCOPE(QueueSubmit);
			// Submit command buffer
			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.pNext = nullptr;
			VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			submitInfo.pWaitDstStageMask = &waitStage;
			// Wait for last frame terminates present image
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &frameContext.PresentSemaphore;
			// Make wait present process until this Queue has finished.
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &frameContext.RenderSemaphore;
			// The command buffer will be procesed
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmd;
			vkcheck(vkQueueSubmit(m_renderContext.GraphicsQueue, 1, &submitInfo, frameContext.RenderFence));
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
		for (uint32_t passIndex = 0; passIndex < RENDER_PASS_COUNT; ++passIndex)
		{
			for (uint32_t i = 0; i < (uint32_t)m_renderers[passIndex].size(); ++i)
				m_renderers[passIndex][i]->ImGuiDraw();
		}
		m_gbuffer.ImGuiDraw();

		ImGui::Begin("Engine");
		ImGui::DragInt("Screen quad index", &m_screenPipeline.QuadIndex, 1, 0, MAX_RT_SCREEN-1);
		ImGui::End();
	}

	void VulkanRenderEngine::WaitFence(VkFence fence, uint64_t timeoutSeconds)
	{
		vkcheck(vkWaitForFences(m_renderContext.Device, 1, &fence, false, timeoutSeconds));
		vkcheck(vkResetFences(m_renderContext.Device, 1, &fence));
	}

	RenderFrameContext& VulkanRenderEngine::GetFrameContext()
	{
		return m_frameContextArray[m_frameCounter % globals::MaxOverlappedFrames];
	}

	VkDescriptorSet VulkanRenderEngine::AllocateDescriptorSet(VkDescriptorSetLayout layout)
	{
		VkDescriptorSet set;
		m_descriptorAllocator.Allocate(&set, layout);
		return set;
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
			appInfo.pApplicationName = "VkMMC";
			appInfo.applicationVersion = VK_API_VERSION_1_1;
			appInfo.apiVersion = VK_API_VERSION_1_1;
			appInfo.engineVersion = VK_API_VERSION_1_1;
			appInfo.pEngineName = "VkMMC";

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
			.set_debug_callback(&vkmmc_debug::DebugVulkanCallback)
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
		SDL_Vulkan_CreateSurface(m_window.WindowInstance, m_renderContext.Instance, &m_renderContext.Surface);
		Log(LogLevel::Ok, "Vulkan surface instance created...\n");
		vkb::PhysicalDeviceSelector selector(instance);
		vkb::PhysicalDevice physicalDevice = selector
			.set_minimum_version(1, 1)
			.set_surface(m_renderContext.Surface)
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
		m_shutdownStack.Add([this]()
			{
				Memory::Destroy(m_renderContext.Allocator);
			});
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
		VkCommandPoolCreateInfo poolInfo = vkinit::CommandPoolCreateInfo(m_renderContext.GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		for (size_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			RenderFrameContext& frameContext = m_frameContextArray[i];
			vkcheck(vkCreateCommandPool(m_renderContext.Device, &poolInfo, nullptr, &frameContext.GraphicsCommandPool));
			VkCommandBufferAllocateInfo allocInfo = vkinit::CommandBufferCreateAllocateInfo(frameContext.GraphicsCommandPool);
			vkcheck(vkAllocateCommandBuffers(m_renderContext.Device, &allocInfo, &frameContext.GraphicsCommand));
			m_shutdownStack.Add([this, i]()
				{
					vkDestroyCommandPool(m_renderContext.Device, m_frameContextArray[i].GraphicsCommandPool, nullptr);
				}
			);
		}

		vkcheck(vkCreateCommandPool(m_renderContext.Device, &poolInfo, nullptr, &m_renderContext.TransferContext.CommandPool));
		VkCommandBufferAllocateInfo allocInfo = vkinit::CommandBufferCreateAllocateInfo(m_renderContext.TransferContext.CommandPool, 1);
		vkcheck(vkAllocateCommandBuffers(m_renderContext.Device, &allocInfo, &m_renderContext.TransferContext.CommandBuffer));
		m_shutdownStack.Add([this]()
			{
				vkDestroyCommandPool(m_renderContext.Device, m_renderContext.TransferContext.CommandPool, nullptr);
			});
		return true;
	}

	bool VulkanRenderEngine::InitRenderPass()
	{
#if 0
		tClearValue clearValues[2];
		clearValues[0].color = { 1.f, 0.f, 0.f, 1.f };
		clearValues[1].depthStencil.depth = 1.f;
		// Color description
		RenderTargetDescription colorDesc;
		colorDesc.RenderArea.extent = { .width = m_window.Width, .height = m_window.Height };
		colorDesc.RenderArea.offset = { 0, 0 };
		colorDesc.AddColorAttachment(m_swapchain.GetImageFormat(), IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValues[0]);
		colorDesc.DepthAttachmentDescription.Format = FORMAT_D32_SFLOAT;
		colorDesc.DepthAttachmentDescription.Layout = IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
		colorDesc.DepthAttachmentDescription.ClearValue = clearValues[1];

		// Depth shadow map description
		RenderTargetDescription shadowMapDesc;
		shadowMapDesc.RenderArea = colorDesc.RenderArea;
		shadowMapDesc.SetDepthAttachment(FORMAT_R32G32B32A32_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValues[1]);

		// Postprocess render target desc
		RenderTargetDescription postDesc;
		postDesc.RenderArea = colorDesc.RenderArea;
		postDesc.AddColorAttachment(m_swapchain.GetImageFormat(), IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValues[0]);

		for (uint32_t i = 0; i < (uint32_t)m_renderTargetArray.size(); ++i)
		{
			m_renderTargetArray[i][RENDER_PASS_LIGHTING].Create(m_renderContext, colorDesc);
			m_renderTargetArray[i][RENDER_PASS_SHADOW_MAP].Create(m_renderContext, shadowMapDesc);
			m_renderTargetArray[i][RENDER_PASS_POST_PROCESS].Create(m_renderContext, postDesc);
		}
		m_shutdownStack.Add(
			[this] {
				for (uint32_t i = 0; i < (uint32_t)m_renderTargetArray.size(); ++i)
				{
					m_renderTargetArray[i][RENDER_PASS_LIGHTING].Destroy(m_renderContext);
					m_renderTargetArray[i][RENDER_PASS_SHADOW_MAP].Destroy(m_renderContext);
					m_renderTargetArray[i][RENDER_PASS_POST_PROCESS].Destroy(m_renderContext);
				}
			}
		);
#endif // 0

#if 0

		// Post process render pass
		{
			VkSubpassDependency dependencies[2];
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			dependencies[0].dependencyFlags = 0;
			dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].dstSubpass = 0;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].srcAccessMask = 0;
			dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			dependencies[1].dependencyFlags = 0;

			m_renderPassArray[RENDER_PASS_POST_PROCESS].RenderPass = RenderPassBuilder::Create()
				.AddAttachment(m_swapchain.GetImageFormat(), IMAGE_LAYOUT_PRESENT_SRC_KHR)
				.AddSubpass({ 0 }, UINT32_MAX, {})
				.AddDependencies(dependencies, sizeof(dependencies) / sizeof(VkSubpassDependency))
				.Build(m_renderContext);

			m_renderPassArray[RENDER_PASS_POST_PROCESS].Width = m_window.Width;
			m_renderPassArray[RENDER_PASS_POST_PROCESS].Height = m_window.Height;
			m_renderPassArray[RENDER_PASS_POST_PROCESS].OffsetX = 0;
			m_renderPassArray[RENDER_PASS_POST_PROCESS].OffsetY = 0;
			VkClearValue value;
			value.color = { 0.f, 0.f, 0.f };
			m_renderPassArray[RENDER_PASS_POST_PROCESS].ClearValues.push_back(value);
		}

		m_shutdownStack.Add([this]()
			{
				for (uint32_t i = 0; i < RENDER_PASS_COUNT; ++i)
					vkDestroyRenderPass(m_renderContext.Device, m_renderPassArray[i].RenderPass, nullptr);
			});
#endif // 0


		return true;
	}

	bool VulkanRenderEngine::InitFramebuffers()
	{
#if 0
		// Collect image in the swapchain
		const uint32_t swapchainImageCount = m_swapchain.GetImageCount();
		//check(globals::MaxOverlappedFrames <= swapchainImageCount);

		// One framebuffer per swapchain image
		m_swapchainTargetArray.resize(swapchainImageCount);
		tClearValue clearValue{ .color = {1.f, 0.f, 0.f, 1.f} };
		RenderTargetDescription presentDesc;
		presentDesc.RenderArea.extent = { .width = m_window.Width, .height = m_window.Height };
		presentDesc.AddColorAttachment(m_swapchain.GetImageFormat(), IMAGE_LAYOUT_PRESENT_SRC_KHR, SAMPLE_COUNT_1_BIT, clearValue);
		presentDesc.ExternalAttachmentCount = 1;
		for (uint32_t i = 0; i < swapchainImageCount; ++i)
		{
			// Swapchain RT
			presentDesc.ExternalAttachments[0] = m_swapchain.GetImageViewAt(i);
			m_swapchainTargetArray[i].Create(m_renderContext, presentDesc);
		}
		m_shutdownStack.Add(
			[this] {
				for (auto& rt : m_swapchainTargetArray)
					rt.Destroy(m_renderContext);
			});
#endif // 0

		return true;
	}

	bool VulkanRenderEngine::InitSync()
	{
		for (size_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			RenderFrameContext& frameContext = m_frameContextArray[i];
			// Render fence
			VkFenceCreateInfo fenceInfo = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
			vkcheck(vkCreateFence(m_renderContext.Device, &fenceInfo, nullptr, &frameContext.RenderFence));
			// Render semaphore
			VkSemaphoreCreateInfo semaphoreInfo = vkinit::SemaphoreCreateInfo();
			vkcheck(vkCreateSemaphore(m_renderContext.Device, &semaphoreInfo, nullptr, &frameContext.RenderSemaphore));
			// Present semaphore
			vkcheck(vkCreateSemaphore(m_renderContext.Device, &semaphoreInfo, nullptr, &frameContext.PresentSemaphore));
			m_shutdownStack.Add([this, i]()
				{
					vkDestroyFence(m_renderContext.Device, m_frameContextArray[i].RenderFence, nullptr);
					vkDestroySemaphore(m_renderContext.Device, m_frameContextArray[i].RenderSemaphore, nullptr);
					vkDestroySemaphore(m_renderContext.Device, m_frameContextArray[i].PresentSemaphore, nullptr);
				});
		}

		VkFenceCreateInfo info = vkinit::FenceCreateInfo();
		vkcheck(vkCreateFence(m_renderContext.Device, &info, nullptr, &m_renderContext.TransferContext.Fence));
		m_shutdownStack.Add([this]()
			{
				vkDestroyFence(m_renderContext.Device, m_renderContext.TransferContext.Fence, nullptr);
			});
		return true;
	}

	bool VulkanRenderEngine::InitPipeline()
	{
		SamplerBuilder builder;
		m_quadSampler = builder.Build(m_renderContext);
		m_shutdownStack.Add([this]() { m_quadSampler.Destroy(m_renderContext); });

		for (size_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			RenderFrameContext& frameContext = m_frameContextArray[i];
			frameContext.CameraData = &m_cameraData;

			// Size for uniform frame buffer
			uint32_t size = 1024 * 1024; // 1MB
			frameContext.GlobalBuffer.Init(m_renderContext, size, BUFFER_USAGE_UNIFORM);

			m_shutdownStack.Add([this, &frameContext]()
				{
					frameContext.GlobalBuffer.Destroy(m_renderContext);
				});

			// Update global descriptors
			uint32_t cameraBufferSize = sizeof(CameraData);
			uint32_t cameraBufferOffset = frameContext.GlobalBuffer.AllocUniform(m_renderContext, UNIFORM_ID_CAMERA, cameraBufferSize);
			VkDescriptorBufferInfo bufferInfo;
			bufferInfo.buffer = frameContext.GlobalBuffer.GetBuffer();
			bufferInfo.offset = cameraBufferOffset;
			bufferInfo.range = cameraBufferSize;
			DescriptorBuilder::Create(m_descriptorLayoutCache, m_descriptorAllocator)
				.BindBuffer(0, &bufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
				.Build(m_renderContext, frameContext.CameraDescriptorSet, m_globalDescriptorLayout);

#if 0
			tArray<VkDescriptorImageInfo, 2> quadImageInfoArray;
			quadImageInfoArray[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			quadImageInfoArray[0].imageView = m_colorAttachments[i].FramebufferArray[0]->GetImageViewAt(0);
			quadImageInfoArray[0].sampler = m_quadSampler.GetSampler();
			quadImageInfoArray[1].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			quadImageInfoArray[1].imageView = m_colorAttachments[i].FramebufferArray[0]->GetImageViewAt(0);
			quadImageInfoArray[1].sampler = m_quadSampler.GetSampler();
			DescriptorBuilder::Create(m_descriptorLayoutCache, m_descriptorAllocator)
				.BindImage(0, &quadImageInfoArray[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.BindImage(1, &quadImageInfoArray[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.Build(m_renderContext, m_quadSets[i]);
#endif // 0


			// Scene buffer allocation. TODO: this should be done by scene.
			frameContext.GlobalBuffer.AllocUniform(m_renderContext, UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY, sizeof(glm::mat4) * globals::MaxRenderObjects);
			frameContext.GlobalBuffer.AllocUniform(m_renderContext, UNIFORM_ID_SCENE_ENV_DATA, sizeof(EnvironmentData));
		}
		return true;
	}

	void VulkanRenderEngine::DrawPass(VkCommandBuffer cmd, uint32_t frameIndex, ERenderPassType passType)
	{
		//RenderTarget& rt = m_renderTargetArray[frameIndex][passType];

		std::vector<IRendererBase*>& renderers = m_renderers[passType];
		//rt.BeginPass(cmd);
		for (IRendererBase* it : renderers)
			it->RecordCmd(m_renderContext, GetFrameContext(), 0);
		//rt.EndPass(cmd);
	}

	void RenderPassAttachment::Destroy(const RenderContext& renderContext)
	{
		for (uint32_t i = 0; i < (uint32_t)FramebufferArray.size(); ++i)
		{
			FramebufferArray[i]->Destroy(renderContext);
			delete FramebufferArray[i];
		}
		FramebufferArray.clear();

		if (Image.IsAllocated())
			Memory::DestroyImage(renderContext.Allocator, Image);
	}



}