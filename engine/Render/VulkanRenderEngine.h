
#pragma once

#include "Render/RenderEngine.h"
#include "RenderPass.h"
#include "Render/Shader.h"
#include "Render/RenderContext.h"
#include "Render/RenderDescriptor.h"
#include "Render/RenderTarget.h"
#include "Render/Texture.h"
#include "Render/VulkanBuffer.h"
#include "Core/Debug.h"
#include "Swapchain.h"
#include "Render/Mesh.h"
#include "Utils/FunctionStack.h"
#include "Framebuffer.h"
#include "Scene/Scene.h"
#include "RenderProcesses/UIProcess.h"
#include "RendererBase.h"

#include <cstdio>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <string.h>
#include <chrono>
#include "Render/Globals.h"
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

		virtual Scene* GetScene() override;
		virtual const Scene* GetScene() const override;
		virtual void SetScene(Scene* scene);
		virtual void AddImGuiCallback(std::function<void()>&& fn) { m_imguiCallbackArray.push_back(fn); }

		const RenderContext& GetContext() const { return m_renderContext; }

		void ReloadShaders();
		void DumpShadersInfo();

	protected:
		void BeginFrame();
		void Draw();
		void ImGuiDraw();
		RenderFrameContext& GetFrameContext();


		// Initializations
		bool InitVulkan();
		bool InitCommands();
		bool InitSync();
		bool InitPipeline();
	private:

		RenderContext m_renderContext;
		Swapchain m_swapchain;

		ScreenQuadPipeline m_screenPipeline;
		CubemapPipeline m_cubemapPipeline;
		Renderer m_renderer;

		uint32_t m_currentSwapchainIndex;

		tArray<DescriptorAllocator, globals::MaxOverlappedFrames> m_descriptorAllocators;
		DescriptorLayoutCache m_descriptorLayoutCache;
		ShaderFileDB m_shaderDb;

		VkDescriptorSetLayout m_globalDescriptorLayout;

		Scene* m_scene = nullptr;
		CameraData m_cameraData;

		typedef std::function<void()> ImGuiCallback;
		tDynArray<ImGuiCallback> m_imguiCallbackArray;

		GPUParticleSystem m_gpuParticleSystem;
	};

	extern void CmdDrawFullscreenQuad(CommandBuffer cmd);
	extern cTexture* GetTextureCheckerboard4x4(const RenderContext& context);
}