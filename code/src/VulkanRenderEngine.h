
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
#include <array>

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

	struct GPUObject
	{
		glm::mat4 ModelTransform;
	};

	struct RenderObjectContainer
	{
		static constexpr size_t MaxRenderObjects = 50000;
		std::array<RenderObjectTransform, MaxRenderObjects> Transforms;
		std::array<RenderObjectMesh, MaxRenderObjects> Meshes;
		uint32_t Counter = 0;

		uint32_t New() { vkmmc_check(Counter < MaxRenderObjects); return Counter++; }
		uint32_t Count() const { return Counter; }
	};

	class VulkanRenderEngine : public IRenderEngine
	{
		static constexpr size_t MaxOverlappedFrames = 2;
	public:
		virtual bool Init(const InitializationSpecs& initSpec) override;
		virtual void RenderLoop() override;
		virtual bool RenderProcess() override;
		virtual void Shutdown() override;

		virtual void UploadMesh(Mesh& mesh) override;

		virtual RenderObject NewRenderObject() override;
		virtual RenderObjectTransform* GetObjectTransform(RenderObject object) override;
		virtual RenderObjectMesh* GetObjectMesh(RenderObject object) override;
		virtual uint32_t GetObjectCount() const { return m_scene.Count(); }
		virtual void SetImGuiCallback(std::function<void()>&& fn) { m_imguiCallback = fn; }

	protected:
		void Draw();
		void DrawRenderables(VkCommandBuffer cmd, const RenderObjectTransform* transforms, const RenderObjectMesh* meshes, size_t count);
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
		RenderHandle RegisterPipeline(RenderPipeline pipeline);

	private:

		Window m_window;
		
		RenderContext m_renderContext;
		
		Swapchain m_swapchain;
		RenderPass m_renderPass;
		RenderHandle m_handleRenderPipeline;
		std::vector<VkFramebuffer> m_framebuffers;

		FrameContext m_frameContextArray[MaxOverlappedFrames];
		size_t m_frameCounter;

		VkDescriptorPool m_descriptorPool;
		VkDescriptorSetLayout m_globalDescriptorLayout;
		VkDescriptorSetLayout m_objectDescriptorLayout;

		std::unordered_map<uint32_t, AllocatedBuffer> m_buffers;
		std::unordered_map<uint32_t, RenderPipeline> m_pipelines;

		RenderObjectContainer m_scene;

		UploadContext m_immediateSubmitContext;

		FunctionStack m_shutdownStack;
		std::function<void()> m_imguiCallback;
	};
}