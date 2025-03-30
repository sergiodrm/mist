#pragma once

#include "Render/RenderEngine.h"
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


	struct UBOTime
	{
		float DeltaTime;
		float TotalTime;
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

		const RenderContext& GetContext() const { return m_renderContext; }

		virtual void ReloadShaders() override;
		void DumpShadersInfo();

	protected:
		void BeginFrame();
		void Draw();
		void ImGuiDraw();
		void DrawCubemap(const RenderContext& context, const cTexture& texture);
		RenderFrameContext& GetFrameContext();

		void ImGuiCVars();


		// Initializations
		bool InitVulkan();
		bool InitCommands();
		bool InitSync();
		bool InitPipeline();
	private:

		RenderContext m_renderContext;
		Swapchain m_swapchain;

		Renderer m_renderer;

		uint32_t m_currentSwapchainIndex;

		tArray<DescriptorAllocator, globals::MaxOverlappedFrames> m_descriptorAllocators;
		DescriptorLayoutCache m_descriptorLayoutCache;
		ShaderFileDB m_shaderDb;

		VkDescriptorSetLayout m_globalDescriptorLayout;

		Scene* m_scene = nullptr;
		CameraData m_cameraData;

		GPUParticleSystem m_gpuParticleSystem;
		Gol* m_gol;
	};

	extern void CmdDrawFullscreenQuad(CommandList* commandList);
	extern cTexture* GetTextureCheckerboard4x4(const RenderContext& context);
	extern cMaterial* GetDefaultMaterial(const RenderContext& context);
}