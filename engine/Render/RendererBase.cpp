#include "RendererBase.h"
#include "RenderProcesses/SSAO.h"
#include "RenderProcesses/GBuffer.h"
#include "RenderProcesses/Lighting.h"
#include "RenderProcesses/ShadowMap.h"
#include "RenderProcesses/DebugProcess.h"
#include "RenderProcesses/UIProcess.h"
#include "Core/SystemMemory.h"

namespace Mist
{
	void Renderer::Init(const RenderContext& context, RenderFrameContext* frameContextArray, uint32_t frameContextCount)
	{
		m_processArray[RENDERPROCESS_SSAO] = _new SSAO();
		m_processArray[RENDERPROCESS_GBUFFER] = _new GBuffer();
		m_processArray[RENDERPROCESS_LIGHTING] = _new DeferredLighting();
		m_processArray[RENDERPROCESS_SHADOWMAP] = _new ShadowMapProcess();
		m_processArray[RENDERPROCESS_DEBUG] = _new DebugProcess();
		m_processArray[RENDERPROCESS_UI] = _new UIProcess();

		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
			m_processArray[i]->Init(context);

		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
		{
			for (uint32_t j = 0; j < frameContextCount; ++j)
				m_processArray[i]->InitFrameData(context, *this, j, frameContextArray[j].GlobalBuffer);
		}
	}

	void Renderer::Destroy(const RenderContext& context)
	{
		// reverse order
		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
		{
			m_processArray[RENDERPROCESS_COUNT - 1 - i]->Destroy(context);
			delete m_processArray[RENDERPROCESS_COUNT - 1 - i];
		}
	}

	void Renderer::UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext)
	{
		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
			m_processArray[i]->UpdateRenderData(context, frameContext);
		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
			m_processArray[i]->DebugDraw(context);
	}

	void Renderer::Draw(const RenderContext& context, const RenderFrameContext& frameContext)
	{
		for (uint32_t i = 0; i < RENDERPROCESS_COUNT; ++i)
			m_processArray[i]->Draw(context, frameContext);
	}

	void Renderer::ImGuiDraw()
	{
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
