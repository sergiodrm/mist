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
#include "VertexInputDescription.h"
#include "Mesh.h"
#include "RenderDescriptor.h"
#include "Camera.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_sdl2.h>
#include "Logger.h"

#define ASSET_ROOT_PATH "../../assets/"
#define SHADER_ROOT_PATH ASSET_ROOT_PATH "shaders/"

namespace vkmmc_globals
{
	// Assets reference
	const char* BasicVertexShaders = SHADER_ROOT_PATH "basic.vert.spv";
	const char* BasicFragmentShaders = SHADER_ROOT_PATH "basic.frag.spv";

	struct CameraController
	{
		vkmmc::Camera Camera{};
		glm::vec3 Direction{ 0.f };
		float MaxSpeed = 1.f; // eu/s
		float MaxRotSpeed = 0.2f; // rad/s

		void ProcessInput(const SDL_Event& e)
		{
			switch (e.key.keysym.sym)
			{
			case SDLK_w:
				Direction += glm::vec3{ 0.f, 0.f, 1.f };
				break;
			case SDLK_a:
				Direction += glm::vec3{ 1.f, 0.f, 0.f };
				break;
			case SDLK_s:
				Direction += glm::vec3{ 0.f, 0.f, -1.f };
				break;
			case SDLK_d:
				Direction += glm::vec3{ -1.f, 0.f, 0.f };
				break;
			}
		}

		void Tick(float elapsedSeconds)
		{
			if (Direction != glm::vec3{0.f})
			{
				Direction = glm::normalize(Direction);
				glm::vec3 speed = Direction * MaxSpeed;
				Camera.SetPosition(Camera.GetPosition() + speed * elapsedSeconds);
				Direction = glm::vec3{ 0.f };
			}
		}

		void ImGuiDraw()
		{
			ImGui::Begin("Camera");
			if (ImGui::Button("Reset position"))
				Camera.SetPosition({ 0.f, 0.f, 0.f });
			glm::vec3 pos = Camera.GetPosition();
			if (ImGui::DragFloat3("Position", &pos[0], 0.5f))
				Camera.SetPosition(pos);
			glm::vec3 rot = Camera.GetRotation();
			if (ImGui::DragFloat3("Rotation", &rot[0], 0.1f))
				Camera.SetRotation(rot);
			ImGui::DragFloat("MaxSpeed", &MaxSpeed, 0.2f);
			ImGui::End();
		}
	} GCameraController{};
}

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
		return VK_FALSE;
	}

	void ImGuiDraw()
	{
	}
}

namespace vkmmc
{
	RenderHandle GenerateRenderHandle()
	{
		static RenderHandle h{ InvalidRenderHandle };
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
		Log(LogLevel::Info, "Initialize render engine.\n");
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

		// ImGui
		InitImGui();

		Log(LogLevel::Ok, "Window created successfully!\n");
		return true;
	}

	void VulkanRenderEngine::RenderLoop()
	{
		Log(LogLevel::Info, "Begin render loop.\n");

		SDL_Event e;
		bool shouldExit = false;
		while (!shouldExit)
		{
			while (SDL_PollEvent(&e))
			{
				ImGuiProcessEvent(e);
				switch (e.type)
				{
				case SDL_QUIT: shouldExit = true; break;
				default:
					vkmmc_globals::GCameraController.ProcessInput(e);
					break;
				}
			}
			ImGuiNewFrame();
			vkmmc_globals::GCameraController.Tick(0.033f);
			vkmmc_globals::GCameraController.ImGuiDraw();
			vkmmc_debug::ImGuiDraw();

			ImGui::Render();
			Draw();
		}
		Log(LogLevel::Info, "End render loop.\n");
	}

	void VulkanRenderEngine::Shutdown()
	{
		Log(LogLevel::Info, "Shutdown render engine.\n");

		for (size_t i = 0; i < MaxOverlappedFrames; ++i)
			WaitFence(m_frameContextArray[i].RenderFence);

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
	}

	void VulkanRenderEngine::UploadMesh(Mesh& mesh)
	{
		// Prepare buffer creation
		const size_t bufferSize = mesh.GetVertexCount() * sizeof(Vertex);
		// Create allocated buffer
		AllocatedBuffer buffer = CreateBuffer(m_renderContext.Allocator, bufferSize,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		
		// Fill buffer
		MemCopyDataToBuffer(m_renderContext.Allocator, buffer.Alloc, mesh.GetVertices(), bufferSize);
		
		// Register new buffer
		RenderHandle handle = GenerateRenderHandle();
		mesh.SetHandle(handle);
		m_buffers[handle] = buffer;

		// Stack buffer destruction
		m_shutdownStack.Add([this, buffer]() 
			{
				DestroyBuffer(m_renderContext.Allocator, buffer);
			});
	}

	void VulkanRenderEngine::AddRenderObject(RenderObject object)
	{
		m_renderables[m_renderablesCounter] = object;
		++m_renderablesCounter;
	}

	void VulkanRenderEngine::Draw()
	{
		FrameContext& frameContext = GetFrameContext();
		WaitFence(frameContext.RenderFence);

		// Acquire render image from swapchain
		uint32_t swapchainImageIndex;
		vkmmc_vkcheck(vkAcquireNextImageKHR(m_renderContext.Device, m_swapchain.GetSwapchainHandle(), 1000000000, frameContext.PresentSemaphore, nullptr, &swapchainImageIndex));

		// Reset command buffer
		vkmmc_vkcheck(vkResetCommandBuffer(frameContext.GraphicsCommand, 0));

		// Prepare command buffer
		VkCommandBuffer cmd = frameContext.GraphicsCommand;
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

		// Draw scene
		DrawRenderables(cmd, m_renderables.data(), m_renderablesCounter);

		// ImGui render
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

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
		submitInfo.pWaitSemaphores = &frameContext.PresentSemaphore;
		// Make wait present process until this Queue has finished.
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &frameContext.RenderSemaphore;
		// The command buffer will be procesed
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;
		vkmmc_vkcheck(vkQueueSubmit(m_renderContext.GraphicsQueue, 1, &submitInfo, frameContext.RenderFence));

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
		vkmmc_vkcheck(vkQueuePresentKHR(m_renderContext.GraphicsQueue, &presentInfo));
	}

	void VulkanRenderEngine::DrawRenderables(VkCommandBuffer cmd, const RenderObject* renderables, size_t count)
	{
		// Update descriptor set buffer
		GPUCamera camera;
		camera.View = vkmmc_globals::GCameraController.Camera.GetView();
		camera.Projection = vkmmc_globals::GCameraController.Camera.GetProjection();
		camera.ViewProjection = camera.Projection * camera.View;

		MemCopyDataToBuffer(m_renderContext.Allocator, GetFrameContext().CameraDescriptorSetBuffer.Alloc, &camera, sizeof(GPUCamera));
		static std::vector<GPUObject> objects(MaxRenderObjects);
		for (uint32_t i = 0; i < count; ++i)
		{
			objects[i].ModelTransform = renderables[i].GetTransform();
		}
		MemCopyDataToBuffer(m_renderContext.Allocator, GetFrameContext().ObjectDescriptorSetBuffer.Alloc, objects.data(), sizeof(GPUObject) * MaxRenderObjects);

		Material lastMaterial;
		Mesh lastMesh;
		for (uint32_t i = 0; i < count; ++i)
		{
			const RenderObject& renderable = renderables[i];
			if (renderable.m_material != lastMaterial || !lastMaterial.GetHandle().IsValid())
			{
				lastMaterial = renderable.m_material;
				RenderPipeline pipeline = renderable.m_material.GetHandle().IsValid()
					? m_pipelines[renderable.m_material.GetHandle()]
					: m_pipelines[m_handleRenderPipeline];
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipelineHandle());
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
					pipeline.GetPipelineLayoutHandle(), 0, 1,
					&GetFrameContext().GlobalDescriptorSet, 0, nullptr);
				//vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
				//	pipeline.GetPipelineLayoutHandle(), 1, 1,
				//	&GetFrameContext().ObjectDescriptorSet, 0, nullptr);
			}


			if (lastMesh != renderable.m_mesh)
			{
				VkDeviceSize offset = 0;
				AllocatedBuffer buffer = m_buffers[renderable.m_mesh.GetHandle()];
				vkCmdBindVertexBuffers(cmd, 0, 1, &buffer.Buffer, &offset);
				lastMesh = renderable.m_mesh;
			}
			vkCmdDraw(cmd, (uint32_t)renderable.m_mesh.GetVertexCount(), 1, 0, i);
		}
	}

	void VulkanRenderEngine::WaitFence(VkFence fence, uint64_t timeoutSeconds)
	{
		vkmmc_vkcheck(vkWaitForFences(m_renderContext.Device, 1, &fence, false, timeoutSeconds));
		vkmmc_vkcheck(vkResetFences(m_renderContext.Device, 1, &fence));
	}

	FrameContext& VulkanRenderEngine::GetFrameContext()
	{
		return m_frameContextArray[m_frameCounter % MaxOverlappedFrames];
	}

	void VulkanRenderEngine::ImmediateSubmit(std::function<void(VkCommandBuffer)>&& fn)
	{
		// Begin command buffer recording.
		VkCommandBufferBeginInfo beginInfo = CommandBufferBeginInfo(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		vkmmc_vkcheck(vkBeginCommandBuffer(m_immediateSubmitContext.CommandBuffer, &beginInfo));
		// Call to extern code to record commands.
		fn(m_immediateSubmitContext.CommandBuffer);
		// Finish recording.
		vkmmc_vkcheck(vkEndCommandBuffer(m_immediateSubmitContext.CommandBuffer));

		VkSubmitInfo info = SubmitInfo(&m_immediateSubmitContext.CommandBuffer);
		vkmmc_vkcheck(vkQueueSubmit(m_renderContext.GraphicsQueue, 1, &info, m_immediateSubmitContext.Fence));
		WaitFence(m_immediateSubmitContext.Fence);
		vkResetCommandPool(m_renderContext.Device, m_immediateSubmitContext.CommandPool, 0);
	}

	void VulkanRenderEngine::ImGuiNewFrame()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(m_window.WindowInstance);
		ImGui::NewFrame();
	}

	void VulkanRenderEngine::ImGuiProcessEvent(const SDL_Event& e)
	{
		ImGui_ImplSDL2_ProcessEvent(&e);
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

	RenderHandle VulkanRenderEngine::RegisterPipeline(RenderPipeline pipeline)
	{
		RenderHandle h = GenerateRenderHandle();
		m_pipelines[h] = pipeline;
		return h;
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
				Log(LogLevel::Info, "Delete Allocator.\n");
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
		VkCommandPoolCreateInfo poolInfo = CommandPoolCreateInfo(m_renderContext.GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		for (size_t i = 0; i < MaxOverlappedFrames; ++i)
		{
			FrameContext& frameContext = m_frameContextArray[i];
			vkmmc_vkcheck(vkCreateCommandPool(m_renderContext.Device, &poolInfo, nullptr, &frameContext.GraphicsCommandPool));
			VkCommandBufferAllocateInfo allocInfo = CommandBufferCreateAllocateInfo(frameContext.GraphicsCommandPool);
			vkmmc_vkcheck(vkAllocateCommandBuffers(m_renderContext.Device, &allocInfo, &frameContext.GraphicsCommand));
			m_shutdownStack.Add([this, i]()
				{
					vkDestroyCommandPool(m_renderContext.Device, m_frameContextArray[i].GraphicsCommandPool, nullptr);
				}
			);
		}

		vkmmc_vkcheck(vkCreateCommandPool(m_renderContext.Device, &poolInfo, nullptr, &m_immediateSubmitContext.CommandPool));
		VkCommandBufferAllocateInfo allocInfo = CommandBufferCreateAllocateInfo(m_immediateSubmitContext.CommandPool, 1);
		vkmmc_vkcheck(vkAllocateCommandBuffers(m_renderContext.Device, &allocInfo, &m_immediateSubmitContext.CommandBuffer));
		m_shutdownStack.Add([this]()
			{
				vkDestroyCommandPool(m_renderContext.Device, m_immediateSubmitContext.CommandPool, nullptr);
			});
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
					Logf(LogLevel::Info, "Destroy framebuffer and imageview [#%Id].\n", i);
					vkDestroyFramebuffer(m_renderContext.Device, m_framebuffers[i], nullptr);
				});
		}

		return true;
	}

	bool VulkanRenderEngine::InitSync()
	{
		for (size_t i = 0; i < MaxOverlappedFrames; ++i)
		{
			FrameContext& frameContext = m_frameContextArray[i];
			// Render fence
			VkFenceCreateInfo fenceInfo = FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
			vkmmc_vkcheck(vkCreateFence(m_renderContext.Device, &fenceInfo, nullptr, &frameContext.RenderFence));
			// Render semaphore
			VkSemaphoreCreateInfo semaphoreInfo = SemaphoreCreateInfo();
			vkmmc_vkcheck(vkCreateSemaphore(m_renderContext.Device, &semaphoreInfo, nullptr, &frameContext.RenderSemaphore));
			// Present semaphore
			vkmmc_vkcheck(vkCreateSemaphore(m_renderContext.Device, &semaphoreInfo, nullptr, &frameContext.PresentSemaphore));
			m_shutdownStack.Add([this, i]()
				{
					Logf(LogLevel::Info, "Destroy fences and semaphores [#%Id].\n", i);
					vkDestroyFence(m_renderContext.Device, m_frameContextArray[i].RenderFence, nullptr);
					vkDestroySemaphore(m_renderContext.Device, m_frameContextArray[i].RenderSemaphore, nullptr);
					vkDestroySemaphore(m_renderContext.Device, m_frameContextArray[i].PresentSemaphore, nullptr);
				});
		}

		VkFenceCreateInfo info = FenceCreateInfo();
		vkmmc_vkcheck(vkCreateFence(m_renderContext.Device, &info, nullptr, &m_immediateSubmitContext.Fence));
		m_shutdownStack.Add([this]()
			{
				vkDestroyFence(m_renderContext.Device, m_immediateSubmitContext.Fence, nullptr);
			});
		return true;
	}

	bool VulkanRenderEngine::InitPipeline()
	{
		// Pipeline descriptors
		VkDescriptorPoolSize poolSizes[] =
		{
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MaxOverlappedFrames * 2 },
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MaxOverlappedFrames * 2 },
		};
		vkmmc_check(CreateDescriptorPool(m_renderContext.Device, poolSizes, sizeof(poolSizes) / sizeof(VkDescriptorPoolSize), &m_descriptorPool));
		m_shutdownStack.Add([this]() 
			{
				Log(LogLevel::Info, "Destroy descriptor pool.\n");
				DestroyDescriptorPool(m_renderContext.Device, m_descriptorPool);
			});
		{
			// Descriptor layout
			VkDescriptorSetLayoutBinding layoutBindings[] = { {}, {} };
			const uint32_t layoutBindingCount = sizeof(layoutBindings) / sizeof(VkDescriptorSetLayoutBinding);
			for (uint32_t i = 0; i < layoutBindingCount; ++i)
			{
				layoutBindings[i].binding = i;
				layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				layoutBindings[i].descriptorCount = 1;
				layoutBindings[i].pImmutableSamplers = nullptr;
				layoutBindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			}
			layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			VkDescriptorSetLayoutCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			createInfo.bindingCount = layoutBindingCount;
			createInfo.pBindings = layoutBindings;
			vkmmc_vkcheck(vkCreateDescriptorSetLayout(m_renderContext.Device, &createInfo, nullptr, &m_globalDescriptorLayout));
			m_shutdownStack.Add([this]()
				{
					Log(LogLevel::Info, "Destroy descriptor layout.\n");
					vkDestroyDescriptorSetLayout(m_renderContext.Device, m_globalDescriptorLayout, nullptr);
				});
		}
		//{
		//	VkDescriptorSetLayoutBinding layoutBinding =
		//	{
		//		.binding = 0,
		//		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		//		.descriptorCount = 1,
		//		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		//		.pImmutableSamplers = nullptr,
		//	};
		//	VkDescriptorSetLayoutCreateInfo createInfo =
		//	{
		//		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		//		.bindingCount = 1,
		//		.pBindings = &layoutBinding
		//	};
		//	vkmmc_vkcheck(vkCreateDescriptorSetLayout(m_renderContext.Device, &createInfo, nullptr, &m_objectDescriptorLayout));
		//	m_shutdownStack.Add([this]() 
		//		{
		//			Log(LogLevel::Info, "Destroy object descriptor set layout.\n");
		//			vkDestroyDescriptorSetLayout(m_renderContext.Device, m_objectDescriptorLayout, nullptr);
		//		});
		//}

		// There is one descriptor set per overlapped frame.
		for (size_t i = 0; i < MaxOverlappedFrames; ++i)
		{
			// Create buffers for the descriptors
			m_frameContextArray[i].CameraDescriptorSetBuffer = CreateBuffer(
				m_renderContext.Allocator,
				sizeof(GPUCamera),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
			m_frameContextArray[i].ObjectDescriptorSetBuffer = CreateBuffer(
				m_renderContext.Allocator,
				sizeof(GPUObject) * MaxRenderObjects,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
			m_shutdownStack.Add([this, i]()
				{
					Logf(LogLevel::Info, "Destroy buffer for descriptor set [#%Id].\n", i);
					DestroyBuffer(m_renderContext.Allocator, m_frameContextArray[i].CameraDescriptorSetBuffer);
					DestroyBuffer(m_renderContext.Allocator, m_frameContextArray[i].ObjectDescriptorSetBuffer);
				});

			// Allocate descriptor sets on pool
			AllocateDescriptorSet(m_renderContext.Device, m_descriptorPool, &m_globalDescriptorLayout, 1, &m_frameContextArray[i].GlobalDescriptorSet);
			//AllocateDescriptorSet(m_renderContext.Device, m_descriptorPool, &m_objectDescriptorLayout, 1, &m_frameContextArray[i].ObjectDescriptorSet);

			// Let vulkan know about these descriptors
			VkDescriptorBufferInfo cameraBufferInfo = {};
			cameraBufferInfo.buffer = m_frameContextArray[i].CameraDescriptorSetBuffer.Buffer;
			cameraBufferInfo.offset = 0;
			cameraBufferInfo.range = sizeof(GPUCamera);
			VkDescriptorBufferInfo objectBufferInfo = {};
			objectBufferInfo.buffer = m_frameContextArray[i].ObjectDescriptorSetBuffer.Buffer;
			objectBufferInfo.offset = 0;
			objectBufferInfo.range = sizeof(GPUObject) * MaxRenderObjects;

			VkWriteDescriptorSet cameraWriteDescriptor = {};
			cameraWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			cameraWriteDescriptor.dstBinding = 0;
			cameraWriteDescriptor.dstArrayElement = 0;
			cameraWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			cameraWriteDescriptor.descriptorCount = 1;
			cameraWriteDescriptor.dstSet = m_frameContextArray[i].GlobalDescriptorSet;
			cameraWriteDescriptor.pBufferInfo = &cameraBufferInfo;
			VkWriteDescriptorSet objectWriteDescriptor = {};
			objectWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			objectWriteDescriptor.dstBinding = 1;
			objectWriteDescriptor.dstArrayElement = 0;
			objectWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			objectWriteDescriptor.descriptorCount = 1;
			objectWriteDescriptor.dstSet = m_frameContextArray[i].GlobalDescriptorSet;
			objectWriteDescriptor.pBufferInfo = &objectBufferInfo;
			VkWriteDescriptorSet writeDescriptors[] = { cameraWriteDescriptor, objectWriteDescriptor };
			uint32_t writeDescriptorCount = sizeof(writeDescriptors) / sizeof(VkWriteDescriptorSet);
			vkUpdateDescriptorSets(m_renderContext.Device, writeDescriptorCount, writeDescriptors, 0, nullptr);
		}

		// Create layout info
		VkDescriptorSetLayout layouts[] = { m_globalDescriptorLayout, m_objectDescriptorLayout };
		VkPipelineLayoutCreateInfo layoutInfo = PipelineLayoutCreateInfo();
		layoutInfo.setLayoutCount = 1;
		layoutInfo.pSetLayouts = layouts;

		ShaderModuleLoadDescription shaderStageDescs[] =
		{
			{.ShaderFilePath = vkmmc_globals::BasicVertexShaders, .Flags = VK_SHADER_STAGE_VERTEX_BIT},
			{.ShaderFilePath = vkmmc_globals::BasicFragmentShaders, .Flags = VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		const size_t shaderCount = sizeof(shaderStageDescs) / sizeof(ShaderModuleLoadDescription);
		RenderPipeline pipeline = CreatePipeline(shaderStageDescs, shaderCount, layoutInfo, GetDefaultVertexDescription());
		m_handleRenderPipeline = RegisterPipeline(pipeline);
		return true;
	}

	bool VulkanRenderEngine::InitImGui()
	{
		// Create descriptor pool for imgui
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.pNext = nullptr;
		poolInfo.maxSets = 1000;
		poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(VkDescriptorPoolSize);
		poolInfo.pPoolSizes = poolSizes;

		VkDescriptorPool imguiPool;
		vkmmc_vkcheck(vkCreateDescriptorPool(m_renderContext.Device, &poolInfo, nullptr, &imguiPool));

		// Init imgui lib
		ImGui::CreateContext();
		ImGui_ImplSDL2_InitForVulkan(m_window.WindowInstance);
		ImGui_ImplVulkan_InitInfo initInfo
		{
			.Instance = m_renderContext.Instance,
			.PhysicalDevice = m_renderContext.GPUDevice,
			.Device = m_renderContext.Device,
			.Queue = m_renderContext.GraphicsQueue,
			.DescriptorPool = imguiPool,
			.MinImageCount = 3,
			.ImageCount = 3,
			.MSAASamples = VK_SAMPLE_COUNT_1_BIT
		};
		ImGui_ImplVulkan_Init(&initInfo, m_renderPass.GetRenderPassHandle());

		// Execute gpu command to upload imgui font textures
		ImmediateSubmit([&](VkCommandBuffer cmd)
			{
				ImGui_ImplVulkan_CreateFontsTexture(cmd);
			});
		ImGui_ImplVulkan_DestroyFontUploadObjects();
		m_shutdownStack.Add([this, imguiPool]()
			{
				vkDestroyDescriptorPool(m_renderContext.Device, imguiPool, nullptr);
				ImGui_ImplVulkan_Shutdown();
			});
		return false;
	}
	
}