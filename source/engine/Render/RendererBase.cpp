#include "RendererBase.h"
#include "RenderProcesses/SSAO.h"
#include "RenderProcesses/GBuffer.h"
#include "RenderProcesses/DeferredLighting.h"
#include "RenderProcesses/ForwardLighting.h"
#include "RenderProcesses/Preprocesses.h"
#include "RenderProcesses/ShadowMap.h"
#include "Core/SystemMemory.h"
#include "Application/Application.h"
#include "VulkanRenderEngine.h"

#include "RenderSystem/RenderSystem.h"

namespace Mist
{
	void Renderer::Init(rendersystem::RenderSystem* rs, IRenderEngine* engine)
	{
		uint32_t width = rs->GetRenderResolution().width;
		uint32_t height = rs->GetRenderResolution().height;

		{
			render::TextureDescription texDesc;
			texDesc.extent = { width, height, 1 };
			texDesc.format = render::Format_R8G8B8A8_UNorm;
			texDesc.isRenderTarget = true;
			render::TextureHandle texture = g_device->CreateTexture(texDesc);

			render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			m_ldr = g_device->CreateRenderTarget(rtDesc);
		}

		m_processArray[RENDERPROCESS_SSAO] = _new SSAO(this, engine);
		m_processArray[RENDERPROCESS_GBUFFER] = _new GBuffer(this, engine);
		m_processArray[RENDERPROCESS_LIGHTING] = _new DeferredLighting(this, engine);
		m_processArray[RENDERPROCESS_FORWARD_LIGHTING] = _new ForwardLighting(this, engine);
		m_processArray[RENDERPROCESS_SHADOWMAP] = _new ShadowMapProcess(this, engine);
		m_processArray[RENDERPROCESS_PREPROCESSES] = _new Preprocess(this, engine);

		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
			m_processArray[i]->Init(rs);
	}

	void Renderer::Destroy(rendersystem::RenderSystem* rs)
	{
		m_ldr = nullptr;

		// reverse order
		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
		{
			m_processArray[RENDERPROCESS_COUNT - 1 - i]->Destroy(rs);
			delete m_processArray[RENDERPROCESS_COUNT - 1 - i];
		}
	}

	void Renderer::Draw(rendersystem::RenderSystem* rs)
	{
		CPU_PROFILE_SCOPE(RendererDraw);
		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
		{
			g_render->BeginMarker(RenderProcessNames[i]);
			m_processArray[i]->Draw(rs);
			g_render->EndMarker();
		}
	}

	void Renderer::DebugRender()
	{
		CPU_PROFILE_SCOPE(RendererDebugDraw);
		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
			m_processArray[i]->DebugDraw();
	}

	void Renderer::ImGuiDraw()
	{
		CPU_PROFILE_SCOPE(RendererImGui);
		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
			m_processArray[i]->ImGuiDraw();
	}

	const RenderProcess* Renderer::GetRenderProcess(RenderProcessType type) const
	{
		check(m_processArray[type] && m_processArray[type]->GetProcessType() == type);
		return m_processArray[type];
	}

	RenderProcess* Renderer::GetRenderProcess(RenderProcessType type)
	{
		check(m_processArray[type] && m_processArray[type]->GetProcessType() == type);
		return m_processArray[type];
	}
}
