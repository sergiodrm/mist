#include "VulkanRenderEngine.h"

#include <cstdio>
#include <glm/gtx/transform.hpp>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
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

	void ImGuiDraw()
	{
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoDecoration
			| ImGuiWindowFlags_AlwaysAutoResize
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoInputs;
		ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0.12f, 0.22f, 0.12f, 1.f });
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::SetNextWindowPos({ 0.f, 0.f });
		ImGui::Begin("Render stats", nullptr, flags);
		for (auto item : vkmmc::GRenderStats.Profiler.m_items)
		{
			ImGui::Text("%s:\t%.4f ms", item.first.c_str(), item.second.m_elapsed);
		}
		ImGui::Separator();
		ImGui::Text("Draw calls:	%u", vkmmc::GRenderStats.DrawCalls);
		ImGui::Text("Triangles:		%u", vkmmc::GRenderStats.TrianglesCount);
		ImGui::Text("Binding count: %u", vkmmc::GRenderStats.SetBindingCount);
		ImGui::End();
		ImGui::PopStyleColor();
	}
}

namespace vkmmc
{
	vkmmc::RenderStats GRenderStats;

	void ProfilingTimer::Start()
	{
		m_start = std::chrono::high_resolution_clock::now();
	}

	double ProfilingTimer::Stop()
	{
		auto stop = std::chrono::high_resolution_clock::now();
		auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - m_start);
		double s = (double)diff.count() * 1e-6;
		return s;
	}

	ScopedTimer::ScopedTimer(const char* nameId, Profiler* profiler)
		: m_nameId(nameId), m_profiler(profiler)
	{
		m_timer.Start();
	}

	ScopedTimer::~ScopedTimer()
	{
		double elapsed = m_timer.Stop();
		if (!m_profiler->m_items.contains(m_nameId))
			m_profiler->m_items[m_nameId] = ProfilerItem{ 0.0 };
		m_profiler->m_items[m_nameId].m_elapsed += elapsed;
	}

	void RenderStats::Reset()
	{
		TrianglesCount = 0;
		DrawCalls = 0;
		SetBindingCount = 0;
		for (auto& it : Profiler.m_items)
			it.second.m_elapsed = 0.0;
	}


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
		PROFILE_SCOPE(Init);
		InitLog("../../log.html");
		Log(LogLevel::Info, "Initialize render engine.\n");
		SDL_Init(SDL_INIT_VIDEO);
		m_window = Window::Create(spec.WindowWidth, spec.WindowHeight, spec.WindowTitle);
		m_renderContext.Width = spec.WindowWidth;
		m_renderContext.Height = spec.WindowHeight;
		m_renderContext.Window = m_window.WindowInstance;
		Log(LogLevel::Ok, "Window created successfully!\n");
		

		// Init vulkan context
		check(InitVulkan());

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
		rendererCreateInfo.ColorPass = m_colorPass.RenderPass;
		rendererCreateInfo.DepthPass = m_depthPass.RenderPass;
		rendererCreateInfo.DescriptorAllocator = &m_descriptorAllocator;
		rendererCreateInfo.LayoutCache = &m_descriptorLayoutCache;
		for (uint32_t i = 0; i < MaxOverlappedFrames; ++i)
		{
			rendererCreateInfo.FrameUniformBufferArray.push_back(&m_frameContextArray[i].GlobalBuffer);
			rendererCreateInfo.DepthImageViewArray.push_back(m_depthPass.FramebufferArray[i].GetImageViewAt(0));
		}

		rendererCreateInfo.ConstantRange = nullptr;
		rendererCreateInfo.ConstantRangeCount = 0;

		m_renderers.push_back(new ModelRenderer());
		m_renderers.push_back(new DebugRenderer());
		m_renderers.push_back(new UIRenderer());
		for (IRendererBase* renderer : m_renderers)
			renderer->Init(rendererCreateInfo);

		AddImGuiCallback(&vkmmc_debug::ImGuiDraw);
		AddImGuiCallback([this]() { ImGuiDraw(); });

		return true;
	}

	bool VulkanRenderEngine::RenderProcess()
	{
		// Reset stats
		PROFILE_SCOPE(Process);
		bool res = true;
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			ImGui_ImplSDL2_ProcessEvent(&e);
			switch (e.type)
			{
			case SDL_QUIT: res = false; break;
			}
		}

		// Update scene graph transforms
		GetFrameContext().Scene->RecalculateTransforms();
		for (IRendererBase* it : m_renderers)
			it->PrepareFrame(m_renderContext, GetFrameContext());

		for (auto& fn : m_imguiCallbackArray)
			fn();

		GRenderStats.Reset();
		Draw();
		return res;
	}

	void VulkanRenderEngine::Shutdown()
	{
		Log(LogLevel::Info, "Shutdown render engine.\n");

		for (size_t i = 0; i < MaxOverlappedFrames; ++i)
			WaitFence(m_frameContextArray[i].RenderFence);

		if (m_scene)
			IScene::DestroyScene(m_scene);

		for (IRendererBase* it : m_renderers)
		{
			it->Destroy(m_renderContext);
			delete it;
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

	void VulkanRenderEngine::SetScene(IScene* scene)
	{
		for (uint32_t i = 0; i < MaxOverlappedFrames; ++i)
			m_frameContextArray[i].Scene = static_cast<Scene*>(scene);
		m_scene = scene;
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

	void VulkanRenderEngine::Draw()
	{
		PROFILE_SCOPE(Draw);
		RenderFrameContext& frameContext = GetFrameContext();
		frameContext.Scene = static_cast<Scene*>(m_scene);
		WaitFence(frameContext.RenderFence);

		{
			PROFILE_SCOPE(UpdateBuffers);
			frameContext.GlobalBuffer.SetUniform(m_renderContext, "Camera", &m_cameraData, sizeof(CameraData));

		}

		VkCommandBuffer cmd = frameContext.GraphicsCommand;
		uint32_t swapchainImageIndex;
		{
			PROFILE_SCOPE(PrepareFrame);
			// Acquire render image from swapchain
			vkcheck(vkAcquireNextImageKHR(m_renderContext.Device, m_swapchain.GetSwapchainHandle(), 1000000000, frameContext.PresentSemaphore, nullptr, &swapchainImageIndex));
			//frameContext.Framebuffer = m_framebufferArray[swapchainImageIndex].GetFramebufferHandle();

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
			PROFILE_SCOPE(DepthPass);
			m_depthPass.BeginPass(cmd, swapchainImageIndex);
			for (IRendererBase* renderer : m_renderers)
				renderer->RecordDepthPass(m_renderContext, frameContext);
			m_depthPass.EndPass(cmd);
		}
		{
			PROFILE_SCOPE(ColorPass);
			m_colorPass.BeginPass(cmd, swapchainImageIndex);
			for (IRendererBase* renderer : m_renderers)
				renderer->RecordColorPass(m_renderContext, frameContext);
			m_colorPass.EndPass(cmd);
		}


		// Terminate command buffer
		vkcheck(vkEndCommandBuffer(cmd));

		{
			PROFILE_SCOPE(QueueSubmit);
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
			PROFILE_SCOPE(Present);
			// Present
			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.pNext = nullptr;
			VkSwapchainKHR swapchain = m_swapchain.GetSwapchainHandle();
			presentInfo.pSwapchains = &swapchain;
			presentInfo.swapchainCount = 1;
			presentInfo.pWaitSemaphores = &frameContext.RenderSemaphore;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pImageIndices = &swapchainImageIndex;
			vkcheck(vkQueuePresentKHR(m_renderContext.GraphicsQueue, &presentInfo));
		}
	}

	void VulkanRenderEngine::ImGuiDraw()
	{
		for (uint32_t i = 0; i < (uint32_t)m_renderers.size(); ++i)
			m_renderers[i]->ImGuiDraw();
	}

	void VulkanRenderEngine::WaitFence(VkFence fence, uint64_t timeoutSeconds)
	{
		vkcheck(vkWaitForFences(m_renderContext.Device, 1, &fence, false, timeoutSeconds));
		vkcheck(vkResetFences(m_renderContext.Device, 1, &fence));
	}

	RenderFrameContext& VulkanRenderEngine::GetFrameContext()
	{
		return m_frameContextArray[m_frameCounter % MaxOverlappedFrames];
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
		vkb::Instance instance = instanceReturn.value();
		m_renderContext.Instance = instance.instance;
		m_renderContext.DebugMessenger = instance.debug_messenger;

		// Physical device
		SDL_Vulkan_CreateSurface(m_window.WindowInstance, m_renderContext.Instance, &m_renderContext.Surface);
		vkb::PhysicalDeviceSelector selector(instance);
		vkb::PhysicalDevice physicalDevice = selector
			.set_minimum_version(1, 1)
			.set_surface(m_renderContext.Surface)
			.select()
			.value();
		vkb::DeviceBuilder deviceBuilder{ physicalDevice };
		VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParamsFeatures = {};
		shaderDrawParamsFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
		shaderDrawParamsFeatures.pNext = nullptr;
		shaderDrawParamsFeatures.shaderDrawParameters = VK_TRUE;
		deviceBuilder.add_pNext(&shaderDrawParamsFeatures);
		vkb::Device device = deviceBuilder.build().value();
		m_renderContext.Device = device.device;
		m_renderContext.GPUDevice = physicalDevice.physical_device;

		// Graphics queue from device
		m_renderContext.GraphicsQueue = device.get_queue(vkb::QueueType::graphics).value();
		m_renderContext.GraphicsQueueFamily = device.get_queue_index(vkb::QueueType::graphics).value();

		// Dump physical device info
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(m_renderContext.GPUDevice, &deviceProperties);
		Logf(LogLevel::Info, "Physical device:\n\t- %s\n\t- Id: %d\n\t- VendorId: %d\n",
			deviceProperties.deviceName, deviceProperties.deviceID, deviceProperties.vendorID);

		// Init memory allocator
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = m_renderContext.GPUDevice;
		allocatorInfo.device = m_renderContext.Device;
		allocatorInfo.instance = m_renderContext.Instance;
		vmaCreateAllocator(&allocatorInfo, &m_renderContext.Allocator);
		m_shutdownStack.Add([this]()
			{
				vmaDestroyAllocator(m_renderContext.Allocator);
			});
		m_renderContext.GPUProperties = device.physical_device.properties;
		Logf(LogLevel::Info, "GPU has minimum buffer alignment of %Id bytes.\n",
			m_renderContext.GPUProperties.limits.minUniformBufferOffsetAlignment);
		Logf(LogLevel::Info, "GPU max bound descriptor sets: %d\n",
			m_renderContext.GPUProperties.limits.maxBoundDescriptorSets);
		return true;
	}

	bool VulkanRenderEngine::InitCommands()
	{
		VkCommandPoolCreateInfo poolInfo = vkinit::CommandPoolCreateInfo(m_renderContext.GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		for (size_t i = 0; i < MaxOverlappedFrames; ++i)
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
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = 0;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = 0;

			dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].dstSubpass = 0;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
				| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[1].srcAccessMask = 0;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
				| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[1].dependencyFlags = 0;

			m_colorPass.RenderPass = RenderPassBuilder::Create()
				.AddColorAttachmentDescription(m_swapchain.GetImageFormat(), true)
				.AddDepthAttachmentDescription(FORMAT_D32)
				.AddSubpass(
					{ 0 }, // Color attachments
					1, // Depth attachment
					{} // Input attachment
				)
				.AddDependencies(dependencies, sizeof(dependencies) / sizeof(VkSubpassDependency))
				.Build(m_renderContext);
			m_shutdownStack.Add([this]()
				{
					vkDestroyRenderPass(m_renderContext.Device, m_colorPass.RenderPass, nullptr);
				}
			);
			check(m_colorPass.RenderPass != VK_NULL_HANDLE);

			m_colorPass.Width = m_window.Width;
			m_colorPass.Height = m_window.Height;
			m_colorPass.OffsetX = 0;
			m_colorPass.OffsetY = 0;
			VkClearValue value;
			value.color = { 0.f, 0.f, 0.f };
			m_colorPass.ClearValues.push_back(value);
			value.depthStencil.depth = 1.f;
			m_colorPass.ClearValues.push_back(value);
		}

		// Depth RenderPass for shadow mapping
		{
			VkSubpassDependency dependencies[2];
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

			m_depthPass.RenderPass = RenderPassBuilder::Create()
				.AddDepthAttachmentDescription(FORMAT_D32, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
				.AddSubpass({}, 0, {})
				.AddDependencies(dependencies, 2)
				.Build(m_renderContext);
			check(m_depthPass.RenderPass != VK_NULL_HANDLE);
			m_shutdownStack.Add([this]() { vkDestroyRenderPass(m_renderContext.Device, m_depthPass.RenderPass, nullptr);  });

			m_depthPass.Width = m_window.Width;
			m_depthPass.Height = m_window.Height;
			m_depthPass.OffsetX = 0;
			m_depthPass.OffsetY = 0;
			VkClearValue value;
			value.depthStencil.depth = 1.f;
			m_depthPass.ClearValues.push_back(value);
		}

		return true;
	}

	bool VulkanRenderEngine::InitFramebuffers()
	{
		// Collect image in the swapchain
		const uint32_t swapchainImageCount = m_swapchain.GetImageCount();

		// One framebuffer for each of the swapchain image view.
		m_colorPass.FramebufferArray.resize(swapchainImageCount);
		m_depthPass.FramebufferArray.resize(swapchainImageCount);
		for (uint32_t i = 0; i < swapchainImageCount; ++i)
		{
			// Color Framebuffer
			m_colorPass.FramebufferArray[i] = Framebuffer::Builder::Create(m_renderContext, m_colorPass.Width, m_colorPass.Height)
				.AddAttachment(m_swapchain.GetImageViewAt(i))
				//.CreateColorAttachment()
				.CreateDepthStencilAttachment()
				.Build(m_colorPass.RenderPass);

			// Depth Framebuffer
			m_depthPass.FramebufferArray[i] = Framebuffer::Builder::Create(m_renderContext, m_depthPass.Width, m_depthPass.Height)
				.CreateDepthStencilAttachment()
				.Build(m_depthPass.RenderPass);

			// Destroy on shutdown
			m_shutdownStack.Add([this, i]()
				{
					m_colorPass.FramebufferArray[i].Destroy(m_renderContext);
					m_depthPass.FramebufferArray[i].Destroy(m_renderContext);
				});
		}
		return true;
	}

	bool VulkanRenderEngine::InitSync()
	{
		for (size_t i = 0; i < MaxOverlappedFrames; ++i)
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
		// Pipeline descriptors
		m_descriptorLayoutCache.Init(m_renderContext);
		m_descriptorAllocator.Init(m_renderContext, DescriptorPoolSizes::GetDefault());
		m_shutdownStack.Add([this]() mutable
			{
				m_descriptorAllocator.Destroy();
				m_descriptorLayoutCache.Destroy();
			});

		for (size_t i = 0; i < MaxOverlappedFrames; ++i)
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
			uint32_t cameraBufferOffset = frameContext.GlobalBuffer.AllocUniform(m_renderContext, "Camera", cameraBufferSize);
			VkDescriptorBufferInfo bufferInfo;
			bufferInfo.buffer = frameContext.GlobalBuffer.GetBuffer();
			bufferInfo.offset = cameraBufferOffset;
			bufferInfo.range = cameraBufferSize;
			DescriptorBuilder::Create(m_descriptorLayoutCache, m_descriptorAllocator)
				.BindBuffer(0, bufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
				.Build(m_renderContext, frameContext.CameraDescriptorSet, m_globalDescriptorLayout);
		}
		return true;
	}

	void RenderPass::BeginPass(VkCommandBuffer cmd, uint32_t framebufferIndex) const
	{
		VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr };
		renderPassInfo.renderPass = RenderPass;
		renderPassInfo.renderArea.offset = { .x = OffsetX, .y = OffsetY };
		renderPassInfo.renderArea.extent = { .width = Width, .height = Height };
		renderPassInfo.framebuffer = FramebufferArray[framebufferIndex].GetFramebufferHandle();
		renderPassInfo.clearValueCount = (uint32_t)ClearValues.size();
		renderPassInfo.pClearValues = ClearValues.data();
		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void RenderPass::EndPass(VkCommandBuffer cmd) const
	{
		vkCmdEndRenderPass(cmd);
	}

}