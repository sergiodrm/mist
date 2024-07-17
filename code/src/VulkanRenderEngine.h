
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
#include "RenderProcesses/UIProcess.h"
#include "RendererBase.h"

#include <cstdio>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <string.h>
#include <chrono>
#include "Globals.h"
#include "RenderProcesses/GPUParticleSystem.h"

namespace Mist
{
	class Framebuffer;
	class IRendererBase;
	struct ShaderModuleLoadDescription;

	struct CameraData
	{
		glm::mat4 InvView;
		glm::mat4 Projection;
		glm::mat4 ViewProjection;
	};

	struct UBOTime
	{
		float DeltaTime;
		float TotalTime;
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
		// One render target per swapchain image.
		tDynArray<RenderTarget> RenderTargetArray;
		tArray<VkDescriptorSet, globals::MaxOverlappedFrames> QuadSets;
		tArray<VkDescriptorSet, globals::MaxOverlappedFrames> PresentTexSets;
		int32_t QuadIndex = -1;

		void Init(const RenderContext& context, const Swapchain& swapchain);
		void Destroy(const RenderContext& context);
	};

	struct CubemapPipeline
	{
		ShaderProgram* Shader = nullptr;
		tArray<VkDescriptorSet, globals::MaxOverlappedFrames> Sets;

		void Init(const RenderContext& context, const RenderTarget* rt);
		void Destroy(const RenderContext& context);
	};
	
	class VulkanRenderEngine : public IRenderEngine
	{
	public:
		/**
		 * IRenderEngine interface
		 */
		virtual bool Init(const Window& window) override;
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
		void WaitFences(VkFence* fence, uint32_t fenceCount, uint64_t timeoutSeconds = 1e9, bool waitAll = true);
		RenderFrameContext& GetFrameContext();


		// Initializations
		bool InitVulkan();
		bool InitCommands();
		bool InitSync();
		bool InitPipeline();

		void ForceSync();

	private:

		RenderContext m_renderContext;
		Swapchain m_swapchain;

		ScreenQuadPipeline m_screenPipeline;
		CubemapPipeline m_cubemapPipeline;
		Renderer m_renderer;

		RenderFrameContext m_frameContextArray[globals::MaxOverlappedFrames];
		uint32_t m_frameCounter;
		uint32_t m_currentSwapchainIndex;

		DescriptorAllocator m_descriptorAllocator;
		DescriptorLayoutCache m_descriptorLayoutCache;
		ShaderFileDB m_shaderDb;

		VkDescriptorSetLayout m_globalDescriptorLayout;

		Scene* m_scene = nullptr;
		CameraData m_cameraData;

		typedef std::function<void()> ImGuiCallback;
		std::vector<ImGuiCallback> m_imguiCallbackArray;
		typedef std::function<void(void*)> EventCallback;
		EventCallback m_eventCallback;

		GPUParticleSystem m_gpuParticleSystem;
	};

	extern RenderHandle GenerateRenderHandle();
}