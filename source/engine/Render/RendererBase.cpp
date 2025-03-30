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

namespace Mist
{
	void Renderer::Init(const RenderContext& context, RenderFrameContext* frameContextArray, uint32_t frameContextCount, const Swapchain& swapchain)
	{
		uint32_t swapchainCount = swapchain.GetImageCount();
		uint32_t renderTargetCount = __min(swapchainCount, m_presentRenderTargets.GetCapacity());
		m_presentRenderTargets.Resize(renderTargetCount);
		check(renderTargetCount > 0);
		VkRect2D renderArea =
		{
			.offset = {0, 0},
			.extent = {.width = context.Window->Width, .height = context.Window->Height }
		};
		for (uint32_t i = 0; i < renderTargetCount; ++i)
		{
			RenderTargetDescription rtDesc;
			rtDesc.RenderArea = renderArea;
			//rtDesc.AddColorAttachment(swapchain.GetImageFormat(), IMAGE_LAYOUT_PRESENT_SRC_KHR, SAMPLE_COUNT_1_BIT, { .color = {0.2f, 0.4f, 0.1f, 0.f} });
			RenderTargetAttachmentDescription attachmentDesc{ SAMPLE_COUNT_1_BIT, swapchain.GetImageFormat(), IMAGE_LAYOUT_PRESENT_SRC_KHR, {0.2f, 0.4f, 0.1f, 0.f}, 0, swapchain.GetImageViewAt(i) };
			rtDesc.AddColorAttachment(attachmentDesc);
			//rtDesc.AddExternalAttachment(swapchain.GetImageViewAt(i), swapchain.GetImageFormat(), IMAGE_LAYOUT_PRESENT_SRC_KHR, SAMPLE_COUNT_1_BIT, { 0.2f, 0.4f, 0.1f, 0.f });
			rtDesc.ResourceName.Fmt("Swapchaing_%d_RT", i);
			m_presentRenderTargets[i] = RenderTarget::Create(context, rtDesc);
		}

		{
			RenderTargetDescription desc;
			desc.RenderArea = renderArea;
			desc.AddColorAttachment(FORMAT_R8G8B8A8_UNORM, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, {}, IMAGE_USAGE_TRANSFER_SRC_BIT);
			desc.ResourceName = "RT_LDR";
			desc.ClearOnLoad = false;
			m_ldr = RenderTarget::Create(context, desc);
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
		RenderTarget::Destroy(context, m_ldr);

		for (uint32_t i = 0; i < m_presentRenderTargets.GetSize(); ++i)
			RenderTarget::Destroy(context, m_presentRenderTargets[i]);

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

	const RenderTarget& Renderer::GetPresentRenderTarget(uint32_t index) const
	{
		return *m_presentRenderTargets[index];
	}

	RenderTarget& Renderer::GetPresentRenderTarget(uint32_t index)
	{
		return *m_presentRenderTargets[index];
	}

	void Renderer::CopyRenderTarget(const tCopyParams& params)
	{
		check(params.Context && params.Dst && params.Src);
		tCopyShaderKey key{ params.Dst->GetDescription(), params.BlendState };
		if (!m_copyPrograms.contains(key))
		{
			tShaderProgramDescription desc;
			desc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			desc.FragmentShaderFile.Filepath = SHADER_FILEPATH("postprocess_quad.frag");
			desc.RenderTarget = params.Dst;
			desc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			m_copyPrograms[key] = ShaderProgram::Create(*params.Context, desc);
		}
		ShaderProgram* copyShader = m_copyPrograms[key];
		check(copyShader);
		//VkCommandBuffer cmd = params.Context->GetFrameContext().GraphicsCommandContext.CommandBuffer;
        CommandList* commandList = params.Context->CmdList;
        commandList->BeginMarker("CopyRT");
		//BeginGPUEvent(*params.Context, cmd, "CopyRT");
		//params.Dst->BeginPass(*params.Context, cmd);
		//copyShader->UseProgram(*params.Context);
        GraphicsState state = {};
        state.Program = copyShader;
        state.Rt = params.Dst;
        commandList->SetGraphicsState(state);
		copyShader->BindSampledTexture(*params.Context, "u_tex", *params.Src->GetTexture());
		//copyShader->FlushDescriptors(*params.Context);
		commandList->BindProgramDescriptorSets();
		CmdDrawFullscreenQuad(commandList);
		//params.Dst->EndPass(cmd);
		//EndGPUEvent(*params.Context, cmd);
		commandList->EndMarker();
	}
}
