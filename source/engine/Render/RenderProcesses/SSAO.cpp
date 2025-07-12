#include "SSAO.h"
#include <random>
#include <imgui/imgui.h>
#include <glm/geometric.hpp>
#include "Utils/GenericUtils.h"
#include "Render/RenderContext.h"
#include "Render/VulkanRenderEngine.h"
#include "GBuffer.h"
#include "Application/Application.h"
#include "Core/Logger.h"
#include "Render/DebugRender.h"
#include "../CommandList.h"
#include "RenderSystem/RenderSystem.h"


#define SSAO_NOISE_SAMPLES 16

namespace Mist
{
	SSAO::SSAO()
		: m_mode(SSAO_Enabled)
	{	}

	void SSAO::Init(const RenderContext& renderContext)
	{
		std::uniform_real_distribution<float> randomFloat(0.f, 1.f);
		std::default_random_engine generator;
		for (uint32_t i = 0; i < SSAO_KERNEL_SAMPLES; ++i)
		{
			float scl = (float)i / (float)SSAO_KERNEL_SAMPLES;
			scl = math::Lerp(0.1f, 1.f, scl * scl);
			glm::vec3 kernel = glm::normalize(glm::vec3{ randomFloat(generator) * 2.f - 1.f, randomFloat(generator) * 2.f - 1.f, randomFloat(generator) });
			m_uboData.KernelSamples[i] = glm::vec4(kernel, 1.f);
			m_uboData.KernelSamples[i] *= randomFloat(generator);
			m_uboData.KernelSamples[i] *= scl;
		}
		m_uboData.Radius = 0.5f;
		m_uboData.Bias = 0.025f;

		glm::vec4 ssaoNoise[SSAO_NOISE_SAMPLES];
		for (uint32_t i = 0; i < SSAO_NOISE_SAMPLES; ++i)
		{
			ssaoNoise[i] = { randomFloat(generator) * 2.f - 1.f, randomFloat(generator) * 2.f - 1.f, 0.f, 1.f };
		}

        {
            render::TextureDescription noiseDesc;
            noiseDesc.extent = { 4,4,1 };
            noiseDesc.format = render::Format_R32G32B32A32_SFloat;
			noiseDesc.isShaderResource = true;
            m_noiseTexture = g_device->CreateTexture(noiseDesc);
            render::utils::UploadContext upload(g_device);
            upload.WriteTexture(m_noiseTexture, 0, 0, ssaoNoise, sizeof(glm::vec4) * SSAO_NOISE_SAMPLES);
            upload.Submit();

            render::TextureDescription texDesc;
            texDesc.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height, .depth = 1 };
            texDesc.format = render::Format_R8_UNorm;
            texDesc.isShaderResource = true;
            texDesc.isRenderTarget = true;
            render::TextureHandle texture = g_device->CreateTexture(texDesc);

            render::RenderTargetDescription rtDesc;
            rtDesc.AddColorAttachment(texture);
            m_rt = g_device->CreateRenderTarget(rtDesc);

            rendersystem::ShaderBuildDescription shaderDesc;
            shaderDesc.vsDesc.filePath = "shaders/quad.vert";
            shaderDesc.fsDesc.filePath = "shaders/ssao.frag";
            shaderDesc.fsDesc.options.PushMacroDefinition("KERNEL_SIZE", SSAO_KERNEL_SAMPLES);
            m_ssaoShader = g_render->CreateShader(shaderDesc);
        }

		{
			render::TextureDescription texDesc;
			texDesc.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height, .depth = 1 };
			texDesc.format = render::Format_R8_UNorm;
			texDesc.isShaderResource = true;
			texDesc.isRenderTarget = true;
			render::TextureHandle texture = g_device->CreateTexture(texDesc);

			render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			m_blurRT = g_device->CreateRenderTarget(rtDesc);

			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.vsDesc.filePath = "shaders/quad.vert";
			shaderDesc.fsDesc.filePath = "shaders/ssaoblur.frag";
			m_blurShader = g_render->CreateShader(shaderDesc);
		}
	}

	void SSAO::Destroy(const RenderContext& renderContext)
	{
		g_render->DestroyShader(&m_blurShader);
		g_render->DestroyShader(&m_ssaoShader);
		m_rt = nullptr;
		m_blurRT = nullptr;
	}

	void SSAO::InitFrameData(const RenderContext& context, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
		m_renderer = &renderer;
	}

	void SSAO::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& frameContext)
	{
	}

	void SSAO::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		CPU_PROFILE_SCOPE(CpuSSAO);
		m_uboData.Projection = GetCameraData()->Projection;
		m_uboData.InverseProjection = glm::inverse(m_uboData.Projection);

		g_render->SetDefaultState();
		g_render->ClearState();
		g_render->SetShader(m_ssaoShader);
		g_render->SetRenderTarget(m_rt);
		g_render->SetDepthEnable(false, false);
		m_uboData.Bypass = m_mode == SSAO_Disabled ? 0.f : 1.f;
		g_render->SetShaderProperty("u_ssao", &m_uboData, sizeof(m_uboData));

		const GBuffer* gbuffer = static_cast<const GBuffer*>(m_renderer->GetRenderProcess(RENDERPROCESS_GBUFFER));
		g_render->SetTextureSlot("u_GBufferPosition", *gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::RT_POSITION].texture);
		g_render->SetTextureSlot("u_GBufferNormal", gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::RT_NORMAL].texture);
		g_render->SetTextureSlot("u_SSAONoise", m_noiseTexture);
		g_render->DrawFullscreenQuad();
		g_render->ClearState();

		g_render->SetShader(m_blurShader);
		g_render->SetRenderTarget(m_blurRT);
		g_render->SetTextureSlot("u_ssaoTex", m_rt->m_description.colorAttachments[0].texture);
		g_render->DrawFullscreenQuad();
		g_render->ClearState();
	}

	void SSAO::ImGuiDraw()
	{
		ImGui::Begin("SSAO");
		ImGui::DragFloat("Radius", &m_uboData.Radius, 0.02f, 0.f, 5.f);
		ImGui::DragFloat("Bias", &m_uboData.Bias, 0.001f, 0.f, 0.05f);
		if (ImGui::BeginCombo("Debug texture", SSAOModeStr[m_mode]))
		{
			for (uint32_t i = 0; i < CountOf(SSAOModeStr); ++i)
			{
				if (ImGui::Selectable(SSAOModeStr[i], i == m_mode))
					m_mode = (eSSAOMode)i;
			}
			ImGui::EndCombo();
		}
		ImGui::End();
	}

	void SSAO::DebugDraw(const RenderContext& context)
	{
		render::TextureHandle tex = nullptr;
		float scale = 1.f;
		switch (m_mode)
		{
		case SSAO_Disabled: 
		case SSAO_Enabled: 
		case SSAO_NoBlur: 
			return;
		case SSAO_DebugView:
			tex = m_rt->m_description.colorAttachments[0].texture;
			break;
		case SSAO_DebugBlurView:
			tex = m_blurRT->m_description.colorAttachments[0].texture;
			break;
		case SSAO_DebugNoiseView:
			tex = m_noiseTexture;
			scale = 100.f;
			break;
		}
		DebugRender::DrawScreenQuad({ 0.f, 0.f },
			{ scale * (float)tex->m_description.extent.width, scale * (float)tex->m_description.extent.height }, tex);
	}

	render::RenderTarget* SSAO::GetRenderTarget(uint32_t index) const
	{
		if (m_mode == SSAO_NoBlur || m_mode == SSAO_Disabled)
			return m_rt.GetPtr();
		return m_blurRT.GetPtr();
	}

}