
#pragma once

#include "RenderEngine.h"
#include "RenderPass.h"
#include "RenderHandle.h"
#include "Shader.h"
#include "RenderObject.h"
#include "RenderContext.h"
#include "RenderDescriptor.h"
#include "RenderTarget.h"
#include "Texture.h"
#include "VulkanBuffer.h"
#include "Debug.h"
#include "Swapchain.h"
#include "Mesh.h"
#include "FunctionStack.h"
#include "Framebuffer.h"
#include "Scene.h"
#include "Renderers/DebugRenderer.h"
#include "Renderers/UIRenderer.h"

#include <cstdio>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <string.h>
#include <chrono>
#include "Globals.h"
#include "DeferredRender.h"

namespace Mist
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

	struct CameraData
	{
		glm::mat4 View;
		glm::mat4 Projection;
		glm::mat4 ViewProjection;
	};

	struct RenderPassAttachment
	{
		AllocatedImage Image;
		std::vector<Framebuffer*> FramebufferArray;

		void Destroy(const RenderContext& renderContext);
	};

	struct ScreenQuadPipeline
	{
		// Pipelines to render in screen framebuffer directly.
		ImGuiInstance  UIInstance;
		DebugPipeline DebugInstance;

		// Quad info
		VertexBuffer VB;
		IndexBuffer IB;
		ShaderProgram* Shader;
		tDynArray<RenderTarget> RenderTargetArray;
		tArray<VkDescriptorSet, globals::MaxOverlappedFrames> QuadSets;
		int32_t QuadIndex = -1;

		void Init(const RenderContext& context, const Swapchain& swapchain);
		void Destroy(const RenderContext& context);
	};
	
	class VulkanRenderEngine : public IRenderEngine
	{
	public:
		/**
		 * IRenderEngine interface
		 */
		virtual bool Init(const InitializationSpecs& initSpec) override;
		virtual bool RenderProcess() override;
		virtual void Shutdown() override;

		virtual void UpdateSceneView(const glm::mat4& view, const glm::mat4& projection) override;

		virtual IScene* GetScene() override;
		virtual const IScene* GetScene() const override;
		virtual void SetScene(IScene* scene);
		virtual void AddImGuiCallback(std::function<void()>&& fn) { m_imguiCallbackArray.push_back(fn); }
		virtual void SetAppEventCallback(std::function<void(void*)>&& fn) override { m_eventCallback = fn; }

		virtual RenderHandle GetDefaultTexture() const;
		virtual Material GetDefaultMaterial() const;
		const RenderContext& GetContext() const { return m_renderContext; }
		VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);
		inline DescriptorLayoutCache& GetDescriptorSetLayoutCache() { return m_descriptorLayoutCache; }
		inline DescriptorAllocator& GetDescriptorAllocator() { return m_descriptorAllocator; }
		inline uint32_t GetFrameIndex() const { return m_frameCounter % globals::MaxOverlappedFrames; }
		inline uint32_t GetFrameCounter() const { return m_frameCounter; }
	protected:
		void BeginFrame();
		void Draw();
		void ImGuiDraw();
		void WaitFence(VkFence fence, uint64_t timeoutSeconds = 1e9);
		RenderFrameContext& GetFrameContext();

		// Initializations
		bool InitVulkan();
		bool InitCommands();
		bool InitRenderPass();
		bool InitFramebuffers();
		bool InitSync();
		bool InitPipeline();

		void DrawPass(VkCommandBuffer cmd, uint32_t frameIndex, ERenderPassType passType);

	private:

		Window m_window;
		RenderContext m_renderContext;
		Swapchain m_swapchain;

		Sampler m_quadSampler;
		ScreenQuadPipeline m_screenPipeline;

		RenderFrameContext m_frameContextArray[globals::MaxOverlappedFrames];
		uint32_t m_frameCounter;
		uint32_t m_currentSwapchainIndex;

		DescriptorAllocator m_descriptorAllocator;
		DescriptorLayoutCache m_descriptorLayoutCache;
		ShaderFileDB m_shaderDb;

		VkDescriptorSetLayout m_globalDescriptorLayout;

		std::vector<IRendererBase*> m_renderers[RENDER_PASS_COUNT];
		GBuffer m_gbuffer;

		Scene* m_scene;
		CameraData m_cameraData;

		FunctionStack m_shutdownStack;
		typedef std::function<void()> ImGuiCallback;
		std::vector<ImGuiCallback> m_imguiCallbackArray;
		typedef std::function<void(void*)> EventCallback;
		EventCallback m_eventCallback;
	};

	extern RenderHandle GenerateRenderHandle();
}