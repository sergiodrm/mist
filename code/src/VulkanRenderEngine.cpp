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
		float MaxSpeed = 100.f; // eu/s
		float MaxRotSpeed = 1.f; // rad/s

		bool IsMotionControlActive = false;
		glm::vec2 MotionRotation{ 0.f };

		void ProcessInputKeyButton(const SDL_KeyboardEvent& e)
		{
			switch (e.keysym.sym)
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
			case SDLK_e:
				Direction += glm::vec3{ 0.f, 1.f, 0.f };
				break;
			case SDLK_q:
				Direction += glm::vec3{ 0.f, -1.f, 0.f };
				break;
			}
		}

		void ProcessInputMouseButton(const SDL_MouseButtonEvent& e)
		{
			switch (e.button)
			{
			case SDL_BUTTON_RIGHT:
				IsMotionControlActive = e.state == SDL_PRESSED;
				SDL_SetRelativeMouseMode(IsMotionControlActive ? SDL_TRUE : SDL_FALSE);
				break;
			}
		}

		void ProcessInputMouseMotion(const SDL_MouseMotionEvent& e)
		{
			if (IsMotionControlActive)
			{
				static constexpr float swidth = 1920.f;
				static constexpr float sheight = 1080.f;
				float yawdiff = (float)(e.xrel) / swidth;
				float pitchdiff = (float)(e.yrel) / sheight;
				MotionRotation += glm::vec2{ -pitchdiff, yawdiff };
			}
		}

		void ProcessInput(const SDL_Event& e)
		{
			switch (e.type)
			{
			case SDL_KEYDOWN:
				ProcessInputKeyButton(e.key);
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				ProcessInputMouseButton(e.button);
				break;
			case SDL_MOUSEMOTION:
				ProcessInputMouseMotion(e.motion);
				break;
			default:
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
			if (MotionRotation != glm::vec2{ 0.f })
			{
				Camera.SetRotation(Camera.GetRotation() + MaxRotSpeed * glm::vec3{ MotionRotation.x, MotionRotation.y, 0.f });
				MotionRotation = glm::vec2{ 0.f };
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
			ImGui::DragFloat("MaxRotSpeed", &MaxRotSpeed, 0.2f);
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

	bool VulkanRenderEngine::Init(const InitializationSpecs& spec)
	{
		PROFILE_SCOPE(Init);
		Log(LogLevel::Info, "Initialize render engine.\n");
		SDL_Init(SDL_INIT_VIDEO);
		m_window = Window::Create(spec.WindowWidth, spec.WindowHeight, spec.WindowTitle);

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
		check(m_renderPass.Init(m_renderContext, { m_swapchain.GetImageFormat(), m_swapchain.GetDepthFormat() }));
		m_shutdownStack.Add([this]() 
			{
				m_renderPass.Destroy(m_renderContext);
			}
		);

		// Framebuffers
		check(InitFramebuffers());
		// Pipelines
		check(InitPipeline());

		// Init sync vars
		check(InitSync());

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
			PROFILE_SCOPE(Loop);
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
			vkmmc_debug::GRenderStats.m_trianglesCount = 0;
			vkmmc_debug::GRenderStats.m_drawCalls = 0;

			ImGui::Render();
			Draw();
		}
		Log(LogLevel::Info, "End render loop.\n");
	}

	bool VulkanRenderEngine::RenderProcess()
	{
		PROFILE_SCOPE(Process);
		bool res = true;
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			ImGuiProcessEvent(e);
			switch (e.type)
			{
			case SDL_QUIT: res = false; break;
			default:
				vkmmc_globals::GCameraController.ProcessInput(e);
				break;
			}
		}
		ImGuiNewFrame();
		vkmmc_globals::GCameraController.Tick(0.033f);
		vkmmc_globals::GCameraController.ImGuiDraw();
		vkmmc_debug::ImGuiDraw();
		vkmmc_debug::GRenderStats.m_trianglesCount = 0;
		vkmmc_debug::GRenderStats.m_drawCalls = 0;
		if (m_imguiCallback)
			m_imguiCallback();

		ImGui::Render();
		Draw();
		return res;
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
		const uint32_t bufferSize = (uint32_t)mesh.GetVertexCount() * sizeof(Vertex);

		// Fill temporal buffer
		AllocatedBuffer stageBuffer = CreateBuffer(m_renderContext.Allocator,
			bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
		MemCopyDataToBuffer(m_renderContext.Allocator, stageBuffer.Alloc, mesh.GetVertices(), bufferSize);

		// Create vertex buffer
		VertexBuffer buffer;
		buffer.Init({
			.RContext = m_renderContext,
			.Type = EBufferType::Static,
			.Size = bufferSize
			});

		// Copy data from temporal buffer to gpu buffer
		ImmediateSubmit([=](VkCommandBuffer cmd) mutable
			{
				buffer.UpdateData(cmd, stageBuffer.Buffer, bufferSize);
			});
		// Delete temporal buffer
		DestroyBuffer(m_renderContext.Allocator, stageBuffer);
		
		// Register new buffer
		RenderHandle handle = GenerateRenderHandle();
		mesh.SetHandle(handle);
		m_vertexBuffers[handle] = buffer;

		// Stack buffer destruction
		m_shutdownStack.Add([this, buffer]() mutable
			{
				buffer.Destroy(m_renderContext);
			});
	}

	void VulkanRenderEngine::UploadMaterial(Material& material)
	{
		check(!material.GetHandle().IsValid());

		RenderHandle h = GenerateRenderHandle();
		m_materials[h] = MaterialTextureData();
		InitMaterial(material, m_materials[h]);
		material.SetHandle(h);
	}

	RenderHandle VulkanRenderEngine::LoadTexture(const char* filename)
	{
		// Load texture from file
		io::TextureRaw texData;
		if (!io::LoadTexture(filename, texData))
		{
			Logf(LogLevel::Error, "Failed to load texture from %s.\n", filename);
			return InvalidRenderHandle;
		}

		// Create gpu buffer with texture specifications
		RenderTextureCreateInfo createInfo
		{
			.RContext = m_renderContext,
			.Raw = texData,
			.RecordCommandRutine = [this](auto fn) { ImmediateSubmit(std::move(fn)); }
		};
		RenderTexture texture;
		texture.Init(createInfo);
		RenderHandle h = GenerateRenderHandle();
		m_textures[h] = texture;
		m_shutdownStack.Add([this, texture]() mutable
			{
				texture.Destroy(m_renderContext);
			});

		// Free raw texture data
		io::FreeTexture(texData.Pixels);
		return h;
	}

	RenderObject VulkanRenderEngine::NewRenderObject()
	{
		RenderObject r;
		r.Id = m_scene.New();
		return r;
	}

	RenderObjectTransform* VulkanRenderEngine::GetObjectTransform(RenderObject object)
	{
		check(object.Id < m_scene.Count());
		return &m_scene.Transforms[object.Id];
	}

	RenderObjectMesh* VulkanRenderEngine::GetObjectMesh(RenderObject object)
	{
		check(object.Id < m_scene.Count());
		return &m_scene.Meshes[object.Id];
	}

	void VulkanRenderEngine::Draw()
	{
		PROFILE_SCOPE(Draw);
		FrameContext& frameContext = GetFrameContext();
		WaitFence(frameContext.RenderFence);

		VkCommandBuffer cmd = frameContext.GraphicsCommand;
		uint32_t swapchainImageIndex;
		{
			PROFILE_SCOPE(PrepareFrame);
			// Acquire render image from swapchain
			vkcheck(vkAcquireNextImageKHR(m_renderContext.Device, m_swapchain.GetSwapchainHandle(), 1000000000, frameContext.PresentSemaphore, nullptr, &swapchainImageIndex));

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
		}

		DrawRenderables(cmd, m_scene.Transforms.data(), m_scene.Meshes.data(), m_scene.Count());

		// ImGui render
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

		// End render pass
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

	void VulkanRenderEngine::DrawRenderables(VkCommandBuffer cmd, const RenderObjectTransform* transforms, const RenderObjectMesh* meshes, size_t count)
	{
		check(count < RenderObjectContainer::MaxRenderObjects);
		{
			PROFILE_SCOPE(UpdateBuffers);
			// Update descriptor set buffer
			GPUCamera camera;
			camera.View = vkmmc_globals::GCameraController.Camera.GetView();
			camera.Projection = vkmmc_globals::GCameraController.Camera.GetProjection();
			camera.ViewProjection = camera.Projection * camera.View;

			MemCopyDataToBuffer(m_renderContext.Allocator, GetFrameContext().CameraDescriptorSetBuffer.Alloc, &camera, sizeof(GPUCamera));
			static std::vector<GPUObject> objects(RenderObjectContainer::MaxRenderObjects);
			for (uint32_t i = 0; i < count; ++i)
			{
				objects[i].ModelTransform = CalculateTransform(transforms[i]);
			}
			MemCopyDataToBuffer(m_renderContext.Allocator, GetFrameContext().ObjectDescriptorSetBuffer.Alloc, objects.data(), sizeof(GPUObject) * RenderObjectContainer::MaxRenderObjects);
		}

		{
			PROFILE_SCOPE(DrawScene);
			RenderPipeline pipeline = m_pipelines[m_handleRenderPipeline];
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipelineHandle());
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeline.GetPipelineLayoutHandle(), 0, 1,
				&GetFrameContext().GlobalDescriptorSet, 0, nullptr);

			auto bindTexture = [this](VkCommandBuffer cmd, RenderPipeline pipeline, RenderHandle texHandle)
				{
					if (texHandle.IsValid())
					{
						const MaterialTextureData& texData = m_materials[texHandle];
						vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
							pipeline.GetPipelineLayoutHandle(), DESCRIPTOR_SET_TEXTURE_LAYOUT, 1, &texData.Set,
							0, nullptr);
					}
				};

			Material lastMaterial;
			Mesh lastMesh;
			for (uint32_t i = 0; i < count; ++i)
			{
				const RenderObjectMesh& renderable = meshes[i];
				if (renderable.Mtl != lastMaterial || !lastMaterial.GetHandle().IsValid())
				{
					lastMaterial = renderable.Mtl;
					
					bindTexture(cmd, pipeline, renderable.Mtl.GetHandle());
					//lastMaterial = renderable.m_material;
					//RenderPipeline pipeline = renderable.m_material.GetHandle().IsValid()
					//	? m_pipelines[renderable.m_material.GetHandle()]
					//	: m_pipelines[m_handleRenderPipeline];
					//vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipelineHandle());
					//vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
					//	pipeline.GetPipelineLayoutHandle(), 0, 1,
					//	&GetFrameContext().GlobalDescriptorSet, 0, nullptr);
					//vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
					//	pipeline.GetPipelineLayoutHandle(), 1, 1,
					//	&GetFrameContext().ObjectDescriptorSet, 0, nullptr);
				}


				if (lastMesh != meshes[i].StaticMesh)
				{
					VertexBuffer buffer = m_vertexBuffers[meshes[i].StaticMesh.GetHandle()];
					buffer.Bind(cmd);
					lastMesh = meshes[i].StaticMesh;
				}
				vkCmdDraw(cmd, (uint32_t)meshes[i].StaticMesh.GetVertexCount(), 1, 0, i);
				vkmmc_debug::GRenderStats.m_drawCalls++;
				vkmmc_debug::GRenderStats.m_trianglesCount += (uint32_t)meshes[i].StaticMesh.GetVertexCount() / 3;
			}
		}
	}

	void VulkanRenderEngine::WaitFence(VkFence fence, uint64_t timeoutSeconds)
	{
		vkcheck(vkWaitForFences(m_renderContext.Device, 1, &fence, false, timeoutSeconds));
		vkcheck(vkResetFences(m_renderContext.Device, 1, &fence));
	}

	FrameContext& VulkanRenderEngine::GetFrameContext()
	{
		return m_frameContextArray[m_frameCounter % MaxOverlappedFrames];
	}

	void VulkanRenderEngine::ImmediateSubmit(std::function<void(VkCommandBuffer)>&& fn)
	{
		// Begin command buffer recording.
		VkCommandBufferBeginInfo beginInfo = CommandBufferBeginInfo(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		vkcheck(vkBeginCommandBuffer(m_immediateSubmitContext.CommandBuffer, &beginInfo));
		// Call to extern code to record commands.
		fn(m_immediateSubmitContext.CommandBuffer);
		// Finish recording.
		vkcheck(vkEndCommandBuffer(m_immediateSubmitContext.CommandBuffer));

		VkSubmitInfo info = SubmitInfo(&m_immediateSubmitContext.CommandBuffer);
		vkcheck(vkQueueSubmit(m_renderContext.GraphicsQueue, 1, &info, m_immediateSubmitContext.Fence));
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

	RenderPipeline VulkanRenderEngine::CreatePipeline(const ShaderModuleLoadDescription* shaderStages, size_t shaderStagesCount, const VkPipelineLayoutCreateInfo& layoutInfo, const VertexInputLayout& inputDescription)
	{
		check(shaderStages && shaderStagesCount > 0);
		RenderPipelineBuilder builder;
		// Input configuration
		builder.InputDescription = inputDescription;
		// Shader stages
		std::vector<VkShaderModule> modules(shaderStagesCount);
		ShaderCompiler compiler(shaderStages, (uint32_t)shaderStagesCount);

		compiler.ProcessReflectionProperties();

		compiler.Compile(m_renderContext, modules);
		for (size_t i = 0; i < shaderStagesCount; ++i)
		{
			check(modules[i] != VK_NULL_HANDLE);
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
		//builder.ColorBlendAttachment.blendEnable = VK_TRUE;
		//builder.ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		//builder.ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		//builder.ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		//builder.ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		//builder.ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		//builder.ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_SUBTRACT;
		// Disable multisampling by default
		builder.Multisampler = PipelineMultisampleStateCreateInfo();
		// Pass layout info
		builder.LayoutInfo = layoutInfo;

		// Build the new pipeline
		RenderPipeline renderPipeline = builder.Build(m_renderContext.Device, m_renderPass.GetRenderPassHandle());;

		// Destroy resources
		m_shutdownStack.Add([this, renderPipeline]() mutable
			{
				renderPipeline.Destroy(m_renderContext);
			});

		// Vulkan modules destruction
		for (size_t i = 0; i < shaderStagesCount; ++i)
			vkDestroyShaderModule(m_renderContext.Device, modules[i], nullptr);
		modules.clear();

		return renderPipeline;
	}

	RenderHandle VulkanRenderEngine::RegisterPipeline(RenderPipeline pipeline)
	{
		RenderHandle h = GenerateRenderHandle();
		m_pipelines[h] = pipeline;
		return h;
	}

	bool VulkanRenderEngine::InitMaterial(const Material& materialHandle, MaterialTextureData& material)
	{
		check(material.Set == VK_NULL_HANDLE);
		m_descriptorAllocator.Allocate(&material.Set, m_descriptorLayouts[DESCRIPTOR_SET_TEXTURE_LAYOUT]);
		
		auto submitTexture = [this](RenderHandle texHandle, MaterialTextureData& mtl, MaterialTextureData::ESamplerIndex index)
			{
				if (texHandle.IsValid())
				{
					RenderTexture texture = m_textures[texHandle];
					SubmitMaterialTexture(mtl, index, texture.GetImageView());
				}
			};
		submitTexture(materialHandle.GetDiffuseTexture(), material, MaterialTextureData::SAMPLER_INDEX_DIFFUSE);
		submitTexture(materialHandle.GetNormalTexture(), material, MaterialTextureData::SAMPLER_INDEX_NORMAL);
		submitTexture(materialHandle.GetSpecularTexture(), material, MaterialTextureData::SAMPLER_INDEX_SPECULAR);

		return true;
	}

	void VulkanRenderEngine::SubmitMaterialTexture(MaterialTextureData& material, MaterialTextureData::ESamplerIndex sampler, const VkImageView& imageView)
	{
		if (material.ImageSamplers[sampler] != VK_NULL_HANDLE)
		{
			vkDestroySampler(m_renderContext.Device, material.ImageSamplers[sampler], nullptr);
			material.ImageSamplers[sampler] = VK_NULL_HANDLE;
		}

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
		vkcheck(vkCreateSampler(m_renderContext.Device, &samplerCreateInfo, nullptr, &material.ImageSamplers[sampler]));

		VkDescriptorImageInfo imageInfo
		{
			.sampler = material.ImageSamplers[sampler],
			.imageView = imageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		VkWriteDescriptorSet writeSet
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = material.Set,
			.dstBinding = 0, // TODO: work out generic binding texture slot
			.dstArrayElement = (uint32_t)sampler,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &imageInfo
		};
		vkUpdateDescriptorSets(m_renderContext.Device, 1, &writeSet, 0, nullptr);
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
			vkcheck(vkCreateCommandPool(m_renderContext.Device, &poolInfo, nullptr, &frameContext.GraphicsCommandPool));
			VkCommandBufferAllocateInfo allocInfo = CommandBufferCreateAllocateInfo(frameContext.GraphicsCommandPool);
			vkcheck(vkAllocateCommandBuffers(m_renderContext.Device, &allocInfo, &frameContext.GraphicsCommand));
			m_shutdownStack.Add([this, i]()
				{
					vkDestroyCommandPool(m_renderContext.Device, m_frameContextArray[i].GraphicsCommandPool, nullptr);
				}
			);
		}

		vkcheck(vkCreateCommandPool(m_renderContext.Device, &poolInfo, nullptr, &m_immediateSubmitContext.CommandPool));
		VkCommandBufferAllocateInfo allocInfo = CommandBufferCreateAllocateInfo(m_immediateSubmitContext.CommandPool, 1);
		vkcheck(vkAllocateCommandBuffers(m_renderContext.Device, &allocInfo, &m_immediateSubmitContext.CommandBuffer));
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
			vkcheck(vkCreateFramebuffer(m_renderContext.Device, &fbInfo, nullptr, &m_framebuffers[i]));

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
			vkcheck(vkCreateFence(m_renderContext.Device, &fenceInfo, nullptr, &frameContext.RenderFence));
			// Render semaphore
			VkSemaphoreCreateInfo semaphoreInfo = SemaphoreCreateInfo();
			vkcheck(vkCreateSemaphore(m_renderContext.Device, &semaphoreInfo, nullptr, &frameContext.RenderSemaphore));
			// Present semaphore
			vkcheck(vkCreateSemaphore(m_renderContext.Device, &semaphoreInfo, nullptr, &frameContext.PresentSemaphore));
			m_shutdownStack.Add([this, i]()
				{
					Logf(LogLevel::Info, "Destroy fences and semaphores [#%Id].\n", i);
					vkDestroyFence(m_renderContext.Device, m_frameContextArray[i].RenderFence, nullptr);
					vkDestroySemaphore(m_renderContext.Device, m_frameContextArray[i].RenderSemaphore, nullptr);
					vkDestroySemaphore(m_renderContext.Device, m_frameContextArray[i].PresentSemaphore, nullptr);
				});
		}

		VkFenceCreateInfo info = FenceCreateInfo();
		vkcheck(vkCreateFence(m_renderContext.Device, &info, nullptr, &m_immediateSubmitContext.Fence));
		m_shutdownStack.Add([this]()
			{
				vkDestroyFence(m_renderContext.Device, m_immediateSubmitContext.Fence, nullptr);
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
				Log(LogLevel::Info, "Destroy descriptor pool.\n");
				m_descriptorAllocator.Destroy();
				m_descriptorLayoutCache.Destroy();
			});
	
		VkDescriptorSetLayout globalLayout;
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
				sizeof(GPUObject) * RenderObjectContainer::MaxRenderObjects,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
			m_shutdownStack.Add([this, i]()
				{
					Logf(LogLevel::Info, "Destroy buffer for descriptor set [#%Id].\n", i);
					DestroyBuffer(m_renderContext.Allocator, m_frameContextArray[i].CameraDescriptorSetBuffer);
					DestroyBuffer(m_renderContext.Allocator, m_frameContextArray[i].ObjectDescriptorSetBuffer);
				});


			// Let vulkan know about these descriptors
			VkDescriptorBufferInfo cameraBufferInfo = {};
			cameraBufferInfo.buffer = m_frameContextArray[i].CameraDescriptorSetBuffer.Buffer;
			cameraBufferInfo.offset = 0;
			cameraBufferInfo.range = sizeof(GPUCamera);
			VkDescriptorBufferInfo objectBufferInfo = {};
			objectBufferInfo.buffer = m_frameContextArray[i].ObjectDescriptorSetBuffer.Buffer;
			objectBufferInfo.offset = 0;
			objectBufferInfo.range = sizeof(GPUObject) * RenderObjectContainer::MaxRenderObjects;


			DescriptorBuilder::Create(m_descriptorLayoutCache, m_descriptorAllocator)
				.BindBuffer(0, cameraBufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
				.BindBuffer(1, objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
				.Build(m_renderContext, m_frameContextArray[i].GlobalDescriptorSet, globalLayout);
		}
		m_descriptorLayouts[DESCRIPTOR_SET_GLOBAL_LAYOUT] = globalLayout;

		VkDescriptorSetLayout textureLayout{ VK_NULL_HANDLE };
		DescriptorSetLayoutBuilder::Create(m_descriptorLayoutCache)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, MaterialTextureData::SAMPLER_INDEX_COUNT)
			//.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4)
			.Build(m_renderContext, &textureLayout);
		m_descriptorLayouts[DESCRIPTOR_SET_TEXTURE_LAYOUT] = textureLayout;

		// Create layout info
		VkPipelineLayoutCreateInfo layoutInfo = PipelineLayoutCreateInfo();
		layoutInfo.setLayoutCount = DESCRIPTOR_SET_COUNT;
		layoutInfo.pSetLayouts = m_descriptorLayouts;

		ShaderModuleLoadDescription shaderStageDescs[] =
		{
			{.ShaderFilePath = vkmmc_globals::BasicVertexShaders, .Flags = VK_SHADER_STAGE_VERTEX_BIT},
			{.ShaderFilePath = vkmmc_globals::BasicFragmentShaders, .Flags = VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		const size_t shaderCount = sizeof(shaderStageDescs) / sizeof(ShaderModuleLoadDescription);
		RenderPipeline pipeline = CreatePipeline(shaderStageDescs, shaderCount, layoutInfo, GetStaticMeshVertexLayout());
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
		vkcheck(vkCreateDescriptorPool(m_renderContext.Device, &poolInfo, nullptr, &imguiPool));

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