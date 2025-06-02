#include "RendererBase.h"
#include "RenderProcesses/SSAO.h"
#include "RenderProcesses/GBuffer.h"
#include "RenderProcesses/DeferredLighting.h"
#include "RenderProcesses/ForwardLighting.h"
#include "RenderProcesses/ShadowMap.h"
#include "Core/SystemMemory.h"
#include "Swapchain.h"
#include "RenderContext.h"
#include "Application/Application.h"
#include "VulkanRenderEngine.h"
#include "CommandList.h"

#include "RenderSystem/RenderSystem.h"

namespace Mist
{
	void Renderer::Init(const RenderContext& context, RenderFrameContext* frameContextArray, uint32_t frameContextCount, const Swapchain& swapchain)
	{
		uint32_t width = context.Window->Width;
		uint32_t height = context.Window->Height;

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

		m_processArray[RENDERPROCESS_SSAO] = _new SSAO();
		m_processArray[RENDERPROCESS_GBUFFER] = _new GBuffer();
		m_processArray[RENDERPROCESS_LIGHTING] = _new DeferredLighting();
		m_processArray[RENDERPROCESS_FORWARD_LIGHTING] = _new ForwardLighting();
		m_processArray[RENDERPROCESS_SHADOWMAP] = _new ShadowMapProcess();

		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
			m_processArray[i]->Init(context);

		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
		{
			for (uint32_t j = 0; j < frameContextCount; ++j)
				m_processArray[i]->InitFrameData(context, *this, j, *frameContextArray[j].GlobalBuffer);
		}
	}

	void Renderer::Destroy(const RenderContext& context)
	{
		m_ldr = nullptr;

		// reverse order
		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
		{
			m_processArray[RENDERPROCESS_COUNT - 1 - i]->Destroy(context);
			delete m_processArray[RENDERPROCESS_COUNT - 1 - i];
		}
	}

	void Renderer::UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext)
	{
		{
			CPU_PROFILE_SCOPE(RendererUpdateRenderData);
			for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
				m_processArray[i]->UpdateRenderData(context, frameContext);
		}
		{
			CPU_PROFILE_SCOPE(RendererDebugDraw);
			for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
				m_processArray[i]->DebugDraw(context);
		}
	}

	void Renderer::Draw(const RenderContext& context, const RenderFrameContext& frameContext)
	{
		CPU_PROFILE_SCOPE(RendererDraw);
		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
			m_processArray[i]->Draw(context, frameContext);
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

	void Renderer::CopyRenderTarget(const CopyParams& params)
	{
		check(params.Dst && params.Src);
		g_render->SetRenderTarget(params.Dst);
		g_render->SetShader(m_copyProgram);
		g_render->SetBlendEnable(params.blendState.blendEnable);
		g_render->SetBlendFactor(params.blendState.srcBlend, params.blendState.dstBlend, params.blendState.blendOp);
		g_render->SetBlendAlphaState(params.blendState.srcAlphaBlend, params.blendState.dstAlphaBlend, params.blendState.alphaBlendOp);
		g_render->SetBlendWriteMask(params.blendState.colorWriteMask);
		g_render->SetTextureSlot("u_tex", params.Src->m_description.colorAttachments[0].texture);
		g_render->DrawFullscreenQuad();
		g_render->ClearState();
	}
}
