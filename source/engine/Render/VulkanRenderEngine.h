#pragma once

#include "Render/RenderEngine.h"
#include "Core/Debug.h"
#include "Render/Mesh.h"
#include "Utils/FunctionStack.h"
#include "Scene/Scene.h"
#include "RendererBase.h"

#include <cstdio>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <string.h>
#include <chrono>
#include "Render/Globals.h"
#include "RenderProcesses/GPUParticleSystem.h"

#define RENDER_BACKEND_TEST

namespace render
{
	class Device;
}

namespace rendersystem
{
	class RenderSystem;
}

namespace Mist
{
	class IRendererBase;


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

		const Renderer* GetRenderer() const { return &m_renderer; }

		virtual void ReloadShaders() override;
		void DumpShadersInfo();

	protected:
		void BeginFrame();
		void Draw();
		void ImGuiDraw();

		// Initializations
		bool InitVulkan();
		bool InitCommands();
		bool InitSync();
		bool InitPipeline();
	private:

		Renderer m_renderer;

		uint32_t m_currentSwapchainIndex;

		Scene* m_scene = nullptr;

		GPUParticleSystem m_gpuParticleSystem;
#if 0
		Gol* m_gol;
#endif // 0


		rendersystem::RenderSystem* m_renderSystem;
	};

	extern cMaterial* GetDefaultMaterial();
	const CameraData* GetCameraData();

	extern ::render::Device* g_device;
	extern ::rendersystem::RenderSystem* g_render;
}