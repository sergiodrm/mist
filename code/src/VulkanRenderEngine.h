
#pragma once

#include "RenderEngine.h"
#include "RenderPass.h"
#include "RenderHandle.h"
#include "RenderPipeline.h"
#include "RenderObject.h"
#include "Texture.h"
#include "VulkanBuffer.h"
#include "Debug.h"
#include "Swapchain.h"
#include "Mesh.h"
#include "FunctionStack.h"
#include "Framebuffer.h"
#include "Scene.h"
#include <cstdio>

#include <vk_mem_alloc.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <string.h>

namespace vkmmc
{
	class Framebuffer;
	class IRendererBase;
	struct ShaderModuleLoadDescription;

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


	struct GPUCamera
	{
		glm::mat4 View;
		glm::mat4 Projection;
		glm::mat4 ViewProjection;
	};

	class VulkanRenderEngine : public IRenderEngine
	{
		static constexpr size_t MaxOverlappedFrames = 2;
		static constexpr size_t MaxRenderObjects = 1000;
	public:
		/**
		 * IRenderEngine interface
		 */
		virtual bool Init(const InitializationSpecs& initSpec) override;
		virtual bool RenderProcess() override;
		virtual void Shutdown() override;

		virtual void UpdateSceneView(const glm::mat4& view, const glm::mat4& projection) override;

		virtual IScene* GetScene() override { return m_scene; }
		virtual const IScene* GetScene() const override { return m_scene; }
		virtual void SetScene(IScene* scene) { m_scene = scene; }
		virtual void AddImGuiCallback(std::function<void()>&& fn) { m_imguiCallbackArray.push_back(fn); }

		virtual RenderHandle GetDefaultTexture() const;
		virtual Material GetDefaultMaterial() const;
		const RenderContext& GetContext() const { return m_renderContext; }
		VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);
		inline DescriptorLayoutCache& GetDescriptorSetLayoutCache() { return m_descriptorLayoutCache; }
		inline DescriptorAllocator& GetDescriptorAllocator() { return m_descriptorAllocator; }
	protected:
		void Draw();
		void WaitFence(VkFence fence, uint64_t timeoutSeconds = 1e9);
		RenderFrameContext& GetFrameContext();

		// Initializations
		bool InitVulkan();
		bool InitCommands();
		bool InitFramebuffers();
		bool InitSync();
		bool InitPipeline();

	private:

		Window m_window;
		
		RenderContext m_renderContext;
		
		Swapchain m_swapchain;
		VkRenderPass m_renderPass;

		std::vector<VkFramebuffer> m_framebuffers;
		std::vector<Framebuffer> m_framebufferArray;

		RenderFrameContext m_frameContextArray[MaxOverlappedFrames];
		size_t m_frameCounter;

		DescriptorAllocator m_descriptorAllocator;
		DescriptorLayoutCache m_descriptorLayoutCache;

		VkDescriptorSetLayout m_globalDescriptorLayout;

		template <typename RenderResourceType>
		using ResourceMap = std::unordered_map<RenderHandle, RenderResourceType, RenderHandle::Hasher>;
		ResourceMap<Texture> m_textures;
		ResourceMap<MeshRenderData> m_meshRenderData;
		ResourceMap<MaterialRenderData> m_materials;

		std::vector<IRendererBase*> m_renderers;

		IScene* m_scene;
		bool m_dirtyCachedCamera;
		GPUCamera m_cachedCameraData;

		FunctionStack m_shutdownStack;
		typedef std::function<void()> ImGuiCallback;
		std::vector<ImGuiCallback> m_imguiCallbackArray;
	};

	extern RenderHandle GenerateRenderHandle();
}