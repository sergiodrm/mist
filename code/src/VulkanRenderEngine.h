
#pragma once
#include "RenderEngine.h"
#include "RenderPass.h"
#include "RenderHandle.h"
#include "RenderPipeline.h"
#include "RenderObject.h"
#include "Swapchain.h"
#include "Mesh.h"
#include "FunctionStack.h"
#include <cstdio>

#include <vk_mem_alloc.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <string.h>

namespace vkmmc
{
	struct RenderContext
	{
		VkInstance Instance;
		VkPhysicalDevice GPUDevice;
		VkSurfaceKHR Surface;
		VkDebugUtilsMessengerEXT DebugMessenger;
		VkPhysicalDeviceProperties GPUProperties;
		VkDevice Device;
		VmaAllocator Allocator;
		VkQueue GraphicsQueue;
		uint32_t GraphicsQueueFamily;
	};

	struct Window
	{
		SDL_Window* WindowInstance;
		uint32_t Width;
		uint32_t Height;
		char Title[32];

		Window() : WindowInstance(nullptr), Width(1920), Height(1080)
		{
			*Title = 0;
		}

		static Window Create(uint32_t width, uint32_t height, const char* title);
	};

	struct UploadContext
	{
		VkFence Fence;
		VkCommandPool CommandPool;
		VkCommandBuffer CommandBuffer;
	};

	struct GPUCamera
	{
		glm::mat4 View;
		glm::mat4 Projection;
		glm::mat4 ViewProjection;
	};

	class VulkanRenderEngine : public IRenderEngine
	{
		static constexpr size_t MaxOverlappedFrames = 2;
	public:
		virtual bool Init(const InitializationSpecs& initSpec) override;
		virtual void RenderLoop() override;
		virtual void Shutdown() override;

		virtual void UploadMesh(Mesh& mesh) override;

		virtual void AddRenderObject(RenderObject object) override;

	protected:
		void Draw();
		void DrawRenderables(VkCommandBuffer cmd, const RenderObject* renderables, size_t count);
		void WaitFence(VkFence fence, uint64_t timeoutSeconds = 1e9);
		FrameContext& GetFrameContext();
		void ImmediateSubmit(std::function<void(VkCommandBuffer)>&& fn);
		void ImGuiNewFrame();
		void ImGuiProcessEvent(const SDL_Event& e);

		// Initializations
		bool InitVulkan();
		bool InitCommands();
		bool InitFramebuffers();
		bool InitSync();
		bool InitPipeline();
		bool InitImGui();

		// Builder utils
		RenderPipeline CreatePipeline(
			const ShaderModuleLoadDescription* shaderStagesDescriptions,
			size_t shaderStagesCount,
			const VkPipelineLayoutCreateInfo& layoutInfo,
			const VertexInputDescription& inputDescription
		);

	private:

		Window m_window;
		
		RenderContext m_renderContext;
		
		Swapchain m_swapchain;
		RenderPass m_renderPass;
		RenderPipeline m_renderPipeline;
		std::vector<VkFramebuffer> m_framebuffers;

		FrameContext m_frameContextArray[MaxOverlappedFrames];
		size_t m_frameCounter;

		VkDescriptorPool m_descriptorPool;
		VkDescriptorSetLayout m_descriptorLayout;

		std::unordered_map<uint32_t, AllocatedBuffer> m_buffers;
		std::unordered_map<uint32_t, RenderPipeline> m_pipelines;

		std::vector<RenderObject> m_renderables;

		UploadContext m_immediateSubmitContext;

		FunctionStack m_shutdownStack;
	};
}