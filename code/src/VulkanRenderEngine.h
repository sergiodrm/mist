
#pragma once
#include "RenderEngine.h"
#include "RenderPass.h"
#include "RenderPipeline.h"
#include "Swapchain.h"
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

	class VulkanRenderEngine : public IRenderEngine
	{
	public:
		virtual bool Init(const InitializationSpecs& initSpec) override;
		virtual void RenderLoop() override;
		virtual void Shutdown() override;
	protected:
		void Draw();
		void WaitFence(VkFence fence, uint64_t timeoutSeconds = 1e9);

		// Initializations
		bool InitVulkan();
		bool InitCommands();
		bool InitFramebuffers();
		bool InitSync();
		bool InitPipeline();

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
		VkQueue m_graphicsQueue;
		uint32_t m_graphicsQueueFamily;
		
		Swapchain m_swapchain;
		RenderPass m_renderPass;
		RenderPipeline m_renderPipeline;
		std::vector<VkFramebuffer> m_framebuffers;

		VkFence m_renderFence;
		VkSemaphore m_renderSemaphore;
		VkSemaphore m_presentSemaphore;

		VkCommandPool m_mainCommandPool;
		VkCommandBuffer m_graphicsCommandBuffer;

		FunctionStack m_shutdownStack;
	};
}