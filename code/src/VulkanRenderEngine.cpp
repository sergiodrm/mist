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

	class GUILogger
	{
		struct MessageItem
		{
			uint32_t Counter = 0;
			std::string Severity;
			std::string Message;
		};
	public:

		void Add(int32_t msgId, const char* severity, const char* msg)
		{
			if (m_messages.contains(msgId))
			{
				MessageItem& it = m_messages[msgId];
				++it.Counter;
			}
			else
			{
				m_messages[msgId] = {1, severity, msg};
			}
		}

		void ImGuiDraw()
		{
			ImGui::Begin("Vulkan validation layer");
			ImGui::Checkbox("Print to console", &m_shouldPrintToLog);
			ImGui::Columns(3);
			ImGui::Text("Severity");
			ImGui::NextColumn();
			ImGui::Text("Count");
			ImGui::NextColumn();
			ImGui::Text("Message");
			ImGui::NextColumn();
			ImGui::Separator();
			for (const auto& it : m_messages)
			{
				ImGui::Text("%s", it.second.Severity.c_str());
				ImGui::NextColumn();
				ImGui::Text("%ld", it.second.Counter);
				ImGui::NextColumn();
				ImGui::Text("%s", it.second.Message.c_str());
				ImGui::NextColumn();
			}
			ImGui::Columns();
			ImGui::End();
		}

		bool m_shouldPrintToLog = true;
	private:
		std::unordered_map<int32_t, MessageItem> m_messages;
	} GGuiLogger;

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
		GGuiLogger.Add(callbackData->messageIdNumber, LogLevelToStr(level), callbackData->pMessage);
		if (GGuiLogger.m_shouldPrintToLog)
			Logf(level, "\nValidation layer\n> Message: %s\n\n", callbackData->pMessage);
#if defined(_DEBUG)
		if (level == vkmmc::LogLevel::Error)
		{
			PrintCallstack();
		}
#endif
		return VK_FALSE;
	}

	struct ProfilingTimer
	{
		std::chrono::high_resolution_clock::time_point m_start;

		void Start()
		{
			m_start = std::chrono::high_resolution_clock::now();
		}

		double Stop()
		{
			auto stop = std::chrono::high_resolution_clock::now();
			auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - m_start);
			double s = (double)diff.count() * 1e-6;
			return s;
		}
	};

	struct ProfilerItem
	{
		double m_elapsed;
	};

	struct Profiler
	{
		std::unordered_map<std::string, ProfilerItem> m_items;
	};

	struct ScopedTimer
	{
		ScopedTimer(const char* nameId, Profiler* profiler)
			: m_nameId(nameId), m_profiler(profiler)
		{
			m_timer.Start();
		}

		~ScopedTimer()
		{
			m_profiler->m_items[m_nameId] = ProfilerItem{ m_timer.Stop() };
		}

		std::string m_nameId;
		Profiler* m_profiler;
		ProfilingTimer m_timer;
	};

	struct RenderStats
	{
		Profiler m_profiler;
		size_t m_trianglesCount{ 0 };
		size_t m_drawCalls{ 0 };
	} GRenderStats;

	struct DebugShaderConstants
	{
		enum EDrawDebug : int32_t
		{
			Disabled, Normals, TexCoords, InColor, Count
		};

		int32_t ForcedIndexTexture{ -1 };
		int32_t DrawDebug{Disabled};
		int32_t EnableLighting{ 1 };
	} GDebugShaderConstants;

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
		for (auto item : GRenderStats.m_profiler.m_items)
		{
			ImGui::Text("%s: %.4f ms", item.first.c_str(), item.second.m_elapsed);
		}
		ImGui::Separator();
		ImGui::Text("Draw calls: %zd", GRenderStats.m_drawCalls);
		ImGui::Text("Triangles:  %zd", GRenderStats.m_trianglesCount);
		ImGui::End();
		ImGui::PopStyleColor();

		ImGui::Begin("Debug shader constants");
		ImGui::SliderInt("Forced texture index", &GDebugShaderConstants.ForcedIndexTexture, -1, 3);
		ImGui::SliderInt("Draw debug mode", &GDebugShaderConstants.DrawDebug, 0, DebugShaderConstants::Count);
		bool lightOn = GDebugShaderConstants.EnableLighting;
		if (ImGui::Checkbox("Enable lighting", &lightOn))
			GDebugShaderConstants.EnableLighting = lightOn ? 1 : 0;
		ImGui::End();

		GGuiLogger.ImGuiDraw();
	}
}

#define PROFILE_SCOPE(name) vkmmc_debug::ScopedTimer __timer##name(#name, &vkmmc_debug::GRenderStats.m_profiler)

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

	void MaterialRenderData::Init(const RenderContext& renderContext, DescriptorAllocator& descAllocator, DescriptorLayoutCache& layoutCache)
	{
		if (Layout == VK_NULL_HANDLE)
		{
			DescriptorSetLayoutBuilder::Create(layoutCache)
				.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3)
				.Build(renderContext, &Layout);
		}
		if (Set == VK_NULL_HANDLE)
		{
			descAllocator.Allocate(&Set, Layout);
		}
		if (Sampler == VK_NULL_HANDLE)
		{
			VkSamplerCreateInfo samplerCreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.pNext = nullptr,
				.magFilter = VK_FILTER_NEAREST,
				.minFilter = VK_FILTER_NEAREST,
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT
			};
			vkcheck(vkCreateSampler(renderContext.Device, &samplerCreateInfo, nullptr, &Sampler));
		}
	}

	void MaterialRenderData::Destroy(const RenderContext& renderContext)
	{
		vkDestroySampler(renderContext.Device, Sampler, nullptr);
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

		m_renderPass = RenderPassBuilder::Create()
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
				vkDestroyRenderPass(m_renderContext.Device, m_renderPass, nullptr);
			}
		);
		check(m_renderPass != VK_NULL_HANDLE);

		// Framebuffers
		check(InitFramebuffers());
		// Pipelines
		check(InitPipeline());

		// Init sync vars
		check(InitSync());

		RendererCreateInfo rendererCreateInfo;
		rendererCreateInfo.RContext = m_renderContext;
		rendererCreateInfo.RenderPass = m_renderPass;
		rendererCreateInfo.DescriptorAllocator = &m_descriptorAllocator;
		rendererCreateInfo.LayoutCache = &m_descriptorLayoutCache;
		for (uint32_t i = 0; i < MaxOverlappedFrames; ++i)
			rendererCreateInfo.FrameUniformBufferArray.push_back(&m_frameContextArray[i].GlobalBuffer);
		VkPushConstantRange pcr;
		pcr.offset = 0;
		pcr.size = sizeof(vkmmc_debug::DebugShaderConstants);
		pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		rendererCreateInfo.ConstantRange = &pcr;
		rendererCreateInfo.ConstantRangeCount = 1; 

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
		for (IRendererBase* it : m_renderers)
			it->BeginFrame(m_renderContext);

		for (auto& fn : m_imguiCallbackArray)
			fn();

		// Reset stats
		vkmmc_debug::GRenderStats.m_trianglesCount = 0;
		vkmmc_debug::GRenderStats.m_drawCalls = 0;
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
			// Update global buffer
			// Camera: binding 0
			//Memory::MemCopy(m_renderContext.Allocator,
			//	GetFrameContext().GlobalBuffer.Alloc,
			//	&m_globalData.Camera,
			//	sizeof(CameraData));
			frameContext.GlobalBuffer.SetUniform(m_renderContext, "Camera", &m_cameraData, sizeof(CameraData));

			// Enviro: binding 2, calculate offset padding
			//uint32_t offset = Memory::PadOffsetAlignment((uint32_t)m_renderContext.GPUProperties.limits.minUniformBufferOffsetAlignment, sizeof(CameraData));
			//Memory::MemCopy(m_renderContext.Allocator, frameContext.GlobalBuffer.Alloc, &m_globalData.Environment, sizeof(EnvironmentData), offset);
			//frameContext.GlobalBuffer.SetUniform(m_renderContext, "Environment", &m_globalData.Environment, sizeof(EnvironmentData));

			// Models transforms: binding 1, accumulate previous offsets
			//offset += Memory::PadOffsetAlignment((uint32_t)m_renderContext.GPUProperties.limits.minUniformBufferOffsetAlignment, sizeof(EnvironmentData));
			//const void* transformsData = frameContext.Scene->GetRawGlobalTransforms();
			//Memory::MemCopy(m_renderContext.Allocator,
			//	GetFrameContext().GlobalBuffer.Alloc,
			//	transformsData,
			//	sizeof(glm::mat4) * frameContext.Scene->Count(), offset);
			//frameContext.GlobalBuffer.SetUniform(m_renderContext, "Models", transformsData, sizeof(glm::mat4) * frameContext.Scene->Count());

			frameContext.PushConstantData = &vkmmc_debug::GDebugShaderConstants;
			frameContext.PushConstantSize = sizeof(vkmmc_debug::DebugShaderConstants);
		}

		VkCommandBuffer cmd = frameContext.GraphicsCommand;
		uint32_t swapchainImageIndex;
		{
			PROFILE_SCOPE(PrepareFrame);
			// Acquire render image from swapchain
			vkcheck(vkAcquireNextImageKHR(m_renderContext.Device, m_swapchain.GetSwapchainHandle(), 1000000000, frameContext.PresentSemaphore, nullptr, &swapchainImageIndex));
			frameContext.Framebuffer = m_framebufferArray[swapchainImageIndex].GetFramebufferHandle();

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

			// Begin render pass
			// Clear values
			VkClearValue clearValues[2];
			clearValues[0].color = { 0.2f, 0.2f, 0.f, 1.f };
			//clearValues[1].color = { 0.2f, 0.2f, 0.f, 1.f };
			clearValues[1].depthStencil.depth = 1.f;
			
			VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr};
			renderPassInfo.renderPass = m_renderPass;
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = { .width = m_framebufferArray[swapchainImageIndex].GetWidth(), .height = m_framebufferArray[swapchainImageIndex].GetHeight()};
			renderPassInfo.framebuffer = frameContext.Framebuffer;
			renderPassInfo.clearValueCount = sizeof(clearValues) / sizeof(VkClearValue);
			renderPassInfo.pClearValues = clearValues;
			vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		}

		// Record command buffers from renderers
		for (IRendererBase* renderer : m_renderers)
			renderer->RecordCommandBuffer(m_renderContext, frameContext);
		

		// Terminate render pass
		vkCmdEndRenderPass(cmd);

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

	bool VulkanRenderEngine::InitFramebuffers()
	{
		// Collect image in the swapchain
		const uint32_t swapchainImageCount = m_swapchain.GetImageCount();
		m_framebuffers = std::vector<VkFramebuffer>(swapchainImageCount);

		// One framebuffer for each of the swapchain image view.
		m_framebufferArray.resize(swapchainImageCount);
		for (uint32_t i = 0; i < swapchainImageCount; ++i)
		{
			m_framebufferArray[i] = Framebuffer::Builder::Create(m_renderContext, m_window.Width, m_window.Height)
				.AddAttachment(m_swapchain.GetImageViewAt(i))
				//.CreateColorAttachment()
				.CreateDepthStencilAttachment()
				.Build(m_renderPass);
			m_shutdownStack.Add([this, i]()
				{
					m_framebufferArray[i].Destroy(m_renderContext);
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

		// Init descriptor set layouts
		// Global
		//DescriptorSetLayoutBuilder::Create(m_descriptorLayoutCache)
		//	.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1) // Camera buffer
		//	.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1) // Model transform storage buffer
		//	.AddBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1) // Light info for fragment shader
		//	.Build(m_renderContext, &m_globalDescriptorLayout);
		
	
		for (size_t i = 0; i < MaxOverlappedFrames; ++i)
		{
			// Create buffers for the descriptors
			// Calculate buffer size with padding
			//const uint32_t minAlignment = (uint32_t)m_renderContext.GPUProperties.limits.minUniformBufferOffsetAlignment;
			//const uint32_t cameraSize = Memory::PadOffsetAlignment(minAlignment, sizeof(CameraData));
			//const uint32_t enviroSize = Memory::PadOffsetAlignment(minAlignment, sizeof(EnvironmentData));
			//const uint32_t modelsSize = sizeof(glm::mat4) * MaxRenderObjects;
			//const uint32_t globalBufferSize = cameraSize + enviroSize + modelsSize;
			//m_frameContextArray[i].GlobalBuffer = Memory::CreateBuffer(
			//	m_renderContext.Allocator,
			//	globalBufferSize,
			//	VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU
			//);

			// Size for uniform frame buffer
			uint32_t size = 1024 * 1024; // 1MB
			m_frameContextArray[i].GlobalBuffer.Init(m_renderContext, size, BUFFER_USAGE_UNIFORM | BUFFER_USAGE_STORAGE);

			m_shutdownStack.Add([this, i]()
				{
					//Memory::DestroyBuffer(m_renderContext.Allocator, m_frameContextArray[i].GlobalBuffer);
					m_frameContextArray[i].GlobalBuffer.Destroy(m_renderContext);
				});

			// Update global descriptors
			uint32_t cameraBufferSize = sizeof(CameraData);
			uint32_t cameraBufferOffset = m_frameContextArray[i].GlobalBuffer.AllocUniform(m_renderContext, "Camera", cameraBufferSize);
			VkDescriptorBufferInfo bufferInfo;
			bufferInfo.buffer = m_frameContextArray[i].GlobalBuffer.GetBuffer();
			bufferInfo.offset = cameraBufferOffset;
			bufferInfo.range = cameraBufferSize;

			
			/*VkDescriptorBufferInfo cameraBufferInfo = {};
			cameraBufferInfo.buffer = m_frameContextArray[i].GlobalBuffer.GetBuffer();
			cameraBufferInfo.offset = cameraInfo.Offset;
			cameraBufferInfo.range = sizeof(CameraData);
			VkDescriptorBufferInfo envBufferInfo = {};
			envBufferInfo.buffer = m_frameContextArray[i].GlobalBuffer.GetBuffer();
			envBufferInfo.offset = cameraSize;
			envBufferInfo.range = sizeof(EnvironmentData);
			VkDescriptorBufferInfo objectBufferInfo = {};
			objectBufferInfo.buffer = m_frameContextArray[i].GlobalBuffer.GetBuffer();
			objectBufferInfo.offset = cameraSize + enviroSize;
			objectBufferInfo.range = sizeof(glm::mat4) * MaxRenderObjects;*/
			DescriptorBuilder::Create(m_descriptorLayoutCache, m_descriptorAllocator)
				.BindBuffer(0, bufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
				.Build(m_renderContext, m_frameContextArray[i].CameraDescriptorSet, m_globalDescriptorLayout);
		}
		return true;
	}	
	
}