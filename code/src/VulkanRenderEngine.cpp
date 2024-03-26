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

namespace vkmmc_debug
{
	bool GTerminatedWithErrors = false;

	VkBool32 DebugVulkanCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
		void* userData)
	{
		vkmmc::LogLevel level = vkmmc::LogLevel::Info;
		switch (severity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: level = vkmmc::LogLevel::Error; GTerminatedWithErrors = true; break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: level = vkmmc::LogLevel::Debug; break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: level = vkmmc::LogLevel::Warn; break;
		}
		Logf(level, "\nValidation layer\n> Message: %s\n\n", callbackData->pMessage);
#if defined(_DEBUG)
		if (level == vkmmc::LogLevel::Error)
			PrintCallstack();
#endif
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
		m_shutdownStack.Add([this]() mutable
			{
				m_descriptorAllocator.Destroy();
				m_descriptorLayoutCache.Destroy();
			});
		m_renderContext.LayoutCache = &m_descriptorLayoutCache;
		m_renderContext.DescAllocator = &m_descriptorAllocator;

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
		rendererCreateInfo.RContext = m_renderContext;
		rendererCreateInfo.DescriptorAllocator = &m_descriptorAllocator;
		rendererCreateInfo.LayoutCache = &m_descriptorLayoutCache;
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			rendererCreateInfo.FrameUniformBufferArray[i] = &m_frameContextArray[i].GlobalBuffer;
			check(m_shadowMapAttachments[i].FramebufferArray.size() == globals::MaxShadowMapAttachments);
			for (uint32_t j = 0; j < globals::MaxShadowMapAttachments; ++j)
				rendererCreateInfo.ShadowMapAttachments[i].push_back(m_shadowMapAttachments[i].FramebufferArray[j]->GetImageViewAt(0));
		}

		rendererCreateInfo.ConstantRange = nullptr;
		rendererCreateInfo.ConstantRangeCount = 0;

		m_renderers[RENDER_PASS_SHADOW_MAP].push_back(new ShadowMapRenderer());
		m_renderers[RENDER_PASS_LIGHTING].push_back(new LightingRenderer());
		m_renderers[RENDER_PASS_LIGHTING].push_back(new DebugRenderer());
		m_renderers[RENDER_PASS_POST_PROCESS].push_back(new QuadRenderer());
		m_renderers[RENDER_PASS_POST_PROCESS].push_back(new UIRenderer());
		for (uint32_t i = 0; i < RENDER_PASS_COUNT; i++)
		{
			rendererCreateInfo.Pass = m_renderPassArray[i].RenderPass;
			for (IRendererBase* renderer : m_renderers[i])
				renderer->Init(rendererCreateInfo);
		}

		m_gbuffer.Init(m_renderContext);
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
			m_gbuffer.InitFrameData(m_renderContext, &m_frameContextArray[i].GlobalBuffer, i);
		m_shutdownStack.Add([this] { m_gbuffer.Destroy(m_renderContext); });

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
		if (vkmmc_debug::GTerminatedWithErrors)
			Log(LogLevel::Error, "Render engine was terminated with vulkan validation layer errors registered.\n");
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
		}

		{
			m_gbuffer.DrawPass(m_renderContext, frameContext);

			uint32_t frameIndex = GetFrameIndex();
			render::DrawQuadTexture(m_quadSets[frameIndex]);
			for (uint32_t pass = RENDER_PASS_SHADOW_MAP; pass < RENDER_PASS_COUNT; ++pass)
				DrawPass(cmd, (ERenderPassType)pass, GetAttachment((ERenderPassType)pass));
		}


		// Terminate command buffer
		vkcheck(vkEndCommandBuffer(cmd));

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
		// Create Vulkan instance
		vkb::InstanceBuilder builder;
		vkb::Result<vkb::Instance> instanceReturn = builder
			.set_app_name("Vulkan renderer")
			.request_validation_layers(true)
			.require_api_version(1, 1, 0)
			//.use_default_debug_messenger()
			.set_debug_callback(&vkmmc_debug::DebugVulkanCallback)
			.build();
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
		vkb::Device device = deviceBuilder.build().value();
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
		// Color RenderPass
		{
			VkSubpassDependency dependencies[2];
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			m_renderPassArray[RENDER_PASS_LIGHTING].RenderPass = RenderPassBuilder::Create()
				.AddAttachment(m_swapchain.GetImageFormat(), IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				.AddAttachment(FORMAT_D32_SFLOAT, IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
				.AddSubpass(
					{ 0 }, // Color attachments
					1, // Depth attachment
					{} // Input attachment
				)
				.AddDependencies(dependencies, sizeof(dependencies) / sizeof(VkSubpassDependency))
				.Build(m_renderContext);
			check(m_renderPassArray[RENDER_PASS_LIGHTING].RenderPass != VK_NULL_HANDLE);

			m_renderPassArray[RENDER_PASS_LIGHTING].Width = m_window.Width;
			m_renderPassArray[RENDER_PASS_LIGHTING].Height = m_window.Height;
			m_renderPassArray[RENDER_PASS_LIGHTING].OffsetX = 0;
			m_renderPassArray[RENDER_PASS_LIGHTING].OffsetY = 0;
			VkClearValue value;
			value.color = { 0.f, 0.f, 0.f };
			m_renderPassArray[RENDER_PASS_LIGHTING].ClearValues.push_back(value);
			value.depthStencil.depth = 1.f;
			m_renderPassArray[RENDER_PASS_LIGHTING].ClearValues.push_back(value);
		}

		// Depth RenderPass for shadow mapping
		{
			std::array<VkSubpassDependency, 2> dependencies;
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			m_renderPassArray[RENDER_PASS_SHADOW_MAP].RenderPass = RenderPassBuilder::Create()
				.AddAttachment(FORMAT_D32_SFLOAT, IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
				.AddSubpass({}, 0, {})
				.AddDependencies(dependencies.data(), (uint32_t)dependencies.size())
				.Build(m_renderContext);
			check(m_renderPassArray[RENDER_PASS_SHADOW_MAP].RenderPass != VK_NULL_HANDLE);

			m_renderPassArray[RENDER_PASS_SHADOW_MAP].Width = m_window.Width;
			m_renderPassArray[RENDER_PASS_SHADOW_MAP].Height = m_window.Height;
			m_renderPassArray[RENDER_PASS_SHADOW_MAP].OffsetX = 0;
			m_renderPassArray[RENDER_PASS_SHADOW_MAP].OffsetY = 0;
			VkClearValue value;
			value.depthStencil.depth = 1.f;
			m_renderPassArray[RENDER_PASS_SHADOW_MAP].ClearValues.push_back(value);
		}

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

		return true;
	}

	bool VulkanRenderEngine::InitFramebuffers()
	{
		// Collect image in the swapchain
		const uint32_t swapchainImageCount = m_swapchain.GetImageCount();
		//check(globals::MaxOverlappedFrames <= swapchainImageCount);

		// One framebuffer per swapchain image
		m_swapchainAttachments.resize(swapchainImageCount);
		for (uint32_t i = 0; i < swapchainImageCount; ++i)
		{
			// Swapchain Framebuffer
			const RenderPass& pass = m_renderPassArray[RENDER_PASS_POST_PROCESS];
			Framebuffer* fb = new Framebuffer();
			Framebuffer::Builder builder(m_renderContext, { .width = pass.Width, .height = pass.Height });
			builder.AddAttachment(m_swapchain.GetImageViewAt(i));
			builder.Build(*fb, pass.RenderPass);

			m_swapchainAttachments[i].FramebufferArray.push_back(fb);
			m_shutdownStack.Add([this, i] { m_swapchainAttachments[i].Destroy(m_renderContext); });
		}

		// One framebuffer for each of the swapchain image view.
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			// Shadow map Framebuffer
			const RenderPass& shadowPass = m_renderPassArray[RENDER_PASS_SHADOW_MAP];
			tExtent3D extent{ .width = shadowPass.Width, .height = shadowPass.Height, .depth = 1 };
			VkImageCreateInfo imageInfo = vkinit::ImageCreateInfo(FORMAT_D32_SFLOAT,
				IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | IMAGE_USAGE_SAMPLED_BIT,
				extent, globals::MaxShadowMapAttachments);
			AllocatedImage depthImage = Memory::CreateImage(m_renderContext.Allocator, imageInfo, MEMORY_USAGE_GPU);
			m_shadowMapAttachments[i].Image = depthImage;
			for (uint32_t j = 0; j < globals::MaxShadowMapAttachments; ++j)
			{
				VkImageViewCreateInfo viewInfo = vkinit::ImageViewCreateInfo(FORMAT_D32_SFLOAT, depthImage.Image, IMAGE_ASPECT_DEPTH_BIT, j);
				VkImageView view;
				vkcheck(vkCreateImageView(m_renderContext.Device, &viewInfo, nullptr, &view));

				Framebuffer* fb = new Framebuffer();
				Framebuffer::Builder builder(m_renderContext, extent);
				builder.AddAttachment(view, true);
				builder.Build(*fb, shadowPass.RenderPass);
				m_shadowMapAttachments[i].FramebufferArray.push_back(fb);
			}

			// Color framebuffer
			const RenderPass& colorPass = m_renderPassArray[RENDER_PASS_LIGHTING];
			RenderPassAttachment& colorPassAttachment = m_colorAttachments[i];
			colorPassAttachment.FramebufferArray.resize(1);
			Framebuffer* fb = new Framebuffer();
			Framebuffer::Builder builder(m_renderContext, extent);
			builder.CreateAttachment(m_swapchain.GetImageFormat(), IMAGE_USAGE_COLOR_ATTACHMENT_BIT | IMAGE_USAGE_SAMPLED_BIT);
			builder.CreateAttachment(m_swapchain.GetDepthFormat(), IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | IMAGE_USAGE_SAMPLED_BIT);
			builder.Build(*fb, colorPass.RenderPass);
			colorPassAttachment.FramebufferArray[0] = fb;

			// Destroy on shutdown
			m_shutdownStack.Add([this, i]()
				{
					m_shadowMapAttachments[i].Destroy(m_renderContext);
					m_colorAttachments[i].Destroy(m_renderContext);
				});
		}
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

			VkDescriptorImageInfo quadImageInfo;
			quadImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			//quadImageInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			quadImageInfo.imageView = m_colorAttachments[i].FramebufferArray[0]->GetImageViewAt(0);
			quadImageInfo.sampler = m_quadSampler.GetSampler();
			DescriptorBuilder::Create(m_descriptorLayoutCache, m_descriptorAllocator)
				.BindImage(0, &quadImageInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.Build(m_renderContext, m_quadSets[i]);

			// Scene buffer allocation. TODO: this should be done by scene.
			frameContext.GlobalBuffer.AllocUniform(m_renderContext, UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY, sizeof(glm::mat4) * globals::MaxRenderObjects);
			frameContext.GlobalBuffer.AllocUniform(m_renderContext, UNIFORM_ID_SCENE_ENV_DATA, sizeof(EnvironmentData));
		}
		return true;
	}

	void VulkanRenderEngine::DrawPass(VkCommandBuffer cmd, ERenderPassType passType, const RenderPassAttachment& passAttachment)
	{
		RenderPass& renderPass = m_renderPassArray[passType];
		std::vector<IRendererBase*>& renderers = m_renderers[passType];
		for (uint32_t i = 0; i < (uint32_t)passAttachment.FramebufferArray.size(); ++i)
		{
			renderPass.BeginPass(cmd, passAttachment.FramebufferArray[i]->GetFramebufferHandle());
			for (IRendererBase* it : renderers)
				it->RecordCmd(m_renderContext, GetFrameContext(), i);
			renderPass.EndPass(cmd);
		}
	}

	const RenderPassAttachment& VulkanRenderEngine::GetAttachment(ERenderPassType passType) const
	{
		switch (passType)
		{
		case RENDER_PASS_SHADOW_MAP: return m_shadowMapAttachments[GetFrameIndex()];
		case RENDER_PASS_LIGHTING: return m_colorAttachments[GetFrameIndex()];
		case RENDER_PASS_PRESENT: return m_swapchainAttachments[m_currentSwapchainIndex];
		default:
			check(false && "Unreachable");
		}
		return *(RenderPassAttachment*)0;
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