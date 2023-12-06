#include "VulkanRenderEngine.h"

#include <cstdio>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <string.h>

#include <vkbootstrap/VkBootstrap.h>
#include "InitVulkanTypes.h"
#include "Vertex.h"

#define ASSET_ROOT_PATH "../../assets/"
#define SHADER_ROOT_PATH ASSET_ROOT_PATH "shaders/"

namespace vkmmc_globals
{
	// Assets reference
	const char* BasicVertexShaders = SHADER_ROOT_PATH "basic.vert.spv";
	const char* BasicFragmentShaders = SHADER_ROOT_PATH "basic.frag.spv";
}

namespace vkmmc
{
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
		printf("Initialize render engine.\n");
		SDL_Init(SDL_INIT_VIDEO);
		m_window = Window::Create(spec.WindowWidth, spec.WindowHeight, spec.WindowTitle);

		// Init vulkan context
		vkmmc_check(InitVulkan());

		// Swapchain
		vkmmc_check(m_swapchain.Init(m_renderContext, { spec.WindowWidth, spec.WindowHeight }));
		m_shutdownStack.Add(
			[this]() 
			{
				m_swapchain.Destroy(m_renderContext);
			}
		);

		// Commands
		vkmmc_check(InitCommands());

		// RenderPass
		vkmmc_check(m_renderPass.Init(m_renderContext, { m_swapchain.GetImageFormat(), m_swapchain.GetDepthFormat() }));
		m_shutdownStack.Add([this]() 
			{
				m_renderPass.Destroy(m_renderContext);
			}
		);

		// Framebuffers
		vkmmc_check(InitFramebuffers());
		// Pipelines
		vkmmc_check(InitPipeline());

		// Init sync vars
		vkmmc_check(InitSync());
	
		printf("Window created successfully!\n");
		return true;
	}

	void VulkanRenderEngine::RenderLoop()
	{
		printf("Begin render loop.\n");

		SDL_Event e;
		bool shouldExit = false;
		while (!shouldExit)
		{
			while (SDL_PollEvent(&e))
			{
				switch (e.type)
				{
				case SDL_QUIT: shouldExit = true; break;
				default:
					break;
				}
			}
			Draw();
		}
		printf("End render loop.\n");
	}

	void VulkanRenderEngine::Shutdown()
	{
		printf("Shutdown render engine.\n");

		WaitFence(m_renderFence);

		m_shutdownStack.Flush();
		vkDestroyDevice(m_renderContext.Device, nullptr);
		vkDestroySurfaceKHR(m_renderContext.Instance, m_renderContext.Surface, nullptr);
		vkb::destroy_debug_utils_messenger(m_renderContext.Instance,
			m_renderContext.DebugMessenger);
		vkDestroyInstance(m_renderContext.Instance, nullptr);
		SDL_DestroyWindow(m_window.WindowInstance);

		printf("Render engine terminated.\n");
	}

	void VulkanRenderEngine::Draw()
	{
		WaitFence(m_renderFence);

		// Acquire render image from swapchain
		uint32_t swapchainImageIndex;
		vkmmc_vkcheck(vkAcquireNextImageKHR(m_renderContext.Device, m_swapchain.GetSwapchainHandle(), 1e9, m_presentSemaphore, nullptr, &swapchainImageIndex));

		// Reset command buffer
		vkmmc_vkcheck(vkResetCommandBuffer(m_graphicsCommandBuffer, 0));

		// Prepare command buffer
		VkCommandBuffer cmd = m_graphicsCommandBuffer;
		VkCommandBufferBeginInfo cmdBeginInfo = {};
		cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBeginInfo.pNext = nullptr;
		cmdBeginInfo.pInheritanceInfo = nullptr;
		cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		// Begin command buffer
		vkmmc_vkcheck(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

		// Prepare render pass
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.pNext = nullptr;
		renderPassInfo.renderPass = m_renderPass.GetRenderPassHandle();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { .width = m_window.Width, .height = m_window.Height };
		renderPassInfo.framebuffer = m_framebuffers[swapchainImageIndex];

		// Clear values
		VkClearValue clearValues[2];
		clearValues[0].color = { 0.2f, 0.2f, 0.f, 1.f };
		clearValues[1].depthStencil.depth = 1.f;
		renderPassInfo.clearValueCount = sizeof(clearValues) / sizeof(VkClearValue);
		renderPassInfo.pClearValues = clearValues;
		// Begin render pass
		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// End render pass
		vkCmdEndRenderPass(cmd);

		// Terminate command buffer
		vkmmc_vkcheck(vkEndCommandBuffer(cmd));

		// Submit command buffer
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submitInfo.pWaitDstStageMask = &waitStage;
		// Wait for last frame terminates present image
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &m_presentSemaphore;
		// Make wait present process until this Queue has finished.
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_renderSemaphore;
		// The command buffer will be procesed
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;
		vkmmc_vkcheck(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_renderFence));

		// Present
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		VkSwapchainKHR swapchain = m_swapchain.GetSwapchainHandle();
		presentInfo.pSwapchains = &swapchain;
		presentInfo.swapchainCount = 1;
		presentInfo.pWaitSemaphores = &m_renderSemaphore;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pImageIndices = &swapchainImageIndex;
		vkmmc_vkcheck(vkQueuePresentKHR(m_graphicsQueue, &presentInfo));
	}

	void VulkanRenderEngine::WaitFence(VkFence fence, uint64_t timeoutSeconds)
	{
		vkmmc_vkcheck(vkWaitForFences(m_renderContext.Device, 1, &fence, false, timeoutSeconds));
		vkmmc_vkcheck(vkResetFences(m_renderContext.Device, 1, &fence));
	}

	RenderPipeline VulkanRenderEngine::CreatePipeline(const ShaderModuleLoadDescription* shaderStages, size_t shaderStagesCount, const VkPipelineLayoutCreateInfo& layoutInfo, const VertexInputDescription& inputDescription)
	{
		vkmmc_check(shaderStages && shaderStagesCount > 0);
		RenderPipelineBuilder builder;
		// Input configuration
		builder.InputDescription = GetDefaultVertexDescription();
		// Shader stages
		VkShaderModule* modules = new VkShaderModule[shaderStagesCount];
		for (size_t i = 0; i < shaderStagesCount; ++i)
		{
			modules[i] = LoadShaderModule(m_renderContext.Device, shaderStages[i].ShaderFilePath.c_str());
			vkmmc_check(modules[i] != VK_NULL_HANDLE);
			builder.ShaderStages.push_back(PipelineShaderStageCreateInfo(shaderStages[i].Flags, modules[i]));
		}
		// Configure viewport settings.
		builder.Viewport.x = 0.f;
		builder.Viewport.y = 0.f;
		builder.Viewport.width = (float)m_window.Width;
		builder.Viewport.height = (float)m_window.Height;
		builder.Viewport.minDepth = 0.f;
		builder.Viewport.maxDepth = 1.f;
		builder.Scissor.offset = { 0, 0 };
		builder.Scissor.extent = { .width = m_window.Width, .height = m_window.Height };
		// Depth testing
		builder.DepthStencil = PipelineDepthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
		// Rasterization: draw filled triangles
		builder.Rasterizer = PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
		// Single blenc attachment without blending and writing RGBA
		builder.ColorBlendAttachment = PipelineColorBlendAttachmentState();
		// Disable multisampling by default
		builder.Multisampler = PipelineMultisampleStateCreateInfo();
		// Pass layout info
		builder.LayoutInfo = layoutInfo;

		// Build the new pipeline
		RenderPipeline renderPipeline = builder.Build(m_renderContext.Device, m_renderPass.GetRenderPassHandle());;

		// Destroy resources
		struct  
		{
			RenderContext context;
			RenderPipeline pipeline;

			void operator()()
			{
				pipeline.Destroy(context);
			}
		} pipelineDestroyer{m_renderContext, renderPipeline};
		m_shutdownStack.Add(pipelineDestroyer);

		// Vulkan modules destruction
		for (size_t i = 0; i < shaderStagesCount; ++i)
			vkDestroyShaderModule(m_renderContext.Device, modules[i], nullptr);
		delete[] modules;
		modules = nullptr;

		return renderPipeline;
	}

	bool VulkanRenderEngine::InitVulkan()
	{
		// Create Vulkan instance
		vkb::InstanceBuilder builder;
		vkb::Result<vkb::Instance> instanceReturn = builder
			.set_app_name("Vulkan renderer")
			.request_validation_layers(true)
			.require_api_version(1, 1, 0)
			.use_default_debug_messenger()
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
		vkb::Device device = deviceBuilder.build().value();
		m_renderContext.Device = device.device;
		m_renderContext.GPUDevice = physicalDevice.physical_device;

		// Graphics queue from device
		m_graphicsQueue = device.get_queue(vkb::QueueType::graphics).value();
		m_graphicsQueueFamily = device.get_queue_index(vkb::QueueType::graphics).value();

		// Dump physical device info
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(m_renderContext.GPUDevice, &deviceProperties);
		printf("Physical device:\n\t- %s\n\t- Id: %d\n\t- VendorId: %d\n",
			deviceProperties.deviceName, deviceProperties.deviceID, deviceProperties.vendorID);

		// Init memory allocator
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = m_renderContext.GPUDevice;
		allocatorInfo.device = m_renderContext.Device;
		allocatorInfo.instance = m_renderContext.Instance;
		vmaCreateAllocator(&allocatorInfo, &m_renderContext.Allocator);
		m_shutdownStack.Add([this]() 
			{
				printf("Delete Allocator.\n");
				vmaDestroyAllocator(m_renderContext.Allocator);
			});
		m_renderContext.GPUProperties = device.physical_device.properties;
		printf("GPU has minimum buffer alignment of %Id bytes.\n",
			m_renderContext.GPUProperties.limits.minUniformBufferOffsetAlignment);
		printf("GPU max bound descriptor sets: %d\n",
			m_renderContext.GPUProperties.limits.maxBoundDescriptorSets);
		return true;
	}

	bool VulkanRenderEngine::InitCommands()
	{
		VkCommandPoolCreateInfo poolInfo = CommandPoolCreateInfo(m_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		vkmmc_vkcheck(vkCreateCommandPool(m_renderContext.Device, &poolInfo, nullptr, &m_mainCommandPool));
		VkCommandBufferAllocateInfo allocInfo = CommandBufferCreateAllocateInfo(m_mainCommandPool);
		vkmmc_vkcheck(vkAllocateCommandBuffers(m_renderContext.Device, &allocInfo, &m_graphicsCommandBuffer));
		m_shutdownStack.Add([this]()
			{
				vkDestroyCommandPool(m_renderContext.Device, m_mainCommandPool, nullptr);
			}
		);
		return true;
	}

	bool VulkanRenderEngine::InitFramebuffers()
	{
		// Create framebuffer for the swapchain images. This will connect render pass with the images for rendering.
		VkFramebufferCreateInfo fbInfo = {};
		fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbInfo.pNext = nullptr;
		fbInfo.renderPass = m_renderPass.GetRenderPassHandle();
		fbInfo.attachmentCount = 1;
		fbInfo.width = m_window.Width;
		fbInfo.height = m_window.Height;
		fbInfo.layers = 1;

		// Collect image in the swapchain
		const uint64_t swapchainImageCount = m_swapchain.GetImageCount();
		m_framebuffers = std::vector<VkFramebuffer>(swapchainImageCount);

		// One framebuffer for each of the swapchain image view.
		for (uint64_t i = 0; i < swapchainImageCount; ++i)
		{
			VkImageView attachments[2];
			attachments[0] = m_swapchain.GetImageViewAt(i);
			attachments[1] = m_swapchain.GetDepthImageView();

			fbInfo.pAttachments = attachments;
			fbInfo.attachmentCount = 2;
			vkmmc_vkcheck(vkCreateFramebuffer(m_renderContext.Device, &fbInfo, nullptr, &m_framebuffers[i]));

			m_shutdownStack.Add([this, i]
				{
					printf("Destroy framebuffer and imageview [#%Id].\n", i);
					vkDestroyFramebuffer(m_renderContext.Device, m_framebuffers[i], nullptr);
				});
		}

		return true;
	}

	bool VulkanRenderEngine::InitSync()
	{
		{
			// Render fence
			VkFenceCreateInfo info = FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
			vkmmc_vkcheck(vkCreateFence(m_renderContext.Device, &info, nullptr, &m_renderFence));
		}

		{
			// Render semaphore
			VkSemaphoreCreateInfo info = SemaphoreCreateInfo();
			vkmmc_vkcheck(vkCreateSemaphore(m_renderContext.Device, &info, nullptr, &m_renderSemaphore));
			// Present semaphore
			vkmmc_vkcheck(vkCreateSemaphore(m_renderContext.Device, &info, nullptr, &m_presentSemaphore));
		}
		m_shutdownStack.Add([this]() 
			{
				printf("Destroy fences and semaphores.\n");
				vkDestroyFence(m_renderContext.Device, m_renderFence, nullptr);
				vkDestroySemaphore(m_renderContext.Device, m_renderSemaphore, nullptr);
				vkDestroySemaphore(m_renderContext.Device, m_presentSemaphore, nullptr);
			}
		);
		return true;
	}

	bool VulkanRenderEngine::InitPipeline()
	{
		// Create layout info
		VkPipelineLayoutCreateInfo layoutInfo = PipelineLayoutCreateInfo();
		// TODO push constants, descriptors...

		ShaderModuleLoadDescription shaderStageDescs[] =
		{
			{.ShaderFilePath = vkmmc_globals::BasicVertexShaders, .Flags = VK_SHADER_STAGE_VERTEX_BIT},
			{.ShaderFilePath = vkmmc_globals::BasicFragmentShaders, .Flags = VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		const size_t shaderCount = sizeof(shaderStageDescs) / sizeof(ShaderModuleLoadDescription);
		RenderPipeline pipeline = CreatePipeline(shaderStageDescs, shaderCount, layoutInfo, GetDefaultVertexDescription());

		return true;
	}
}