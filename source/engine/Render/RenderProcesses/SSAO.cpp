#include "SSAO.h"
#include <random>
#include <imgui/imgui.h>
#include <glm/geometric.hpp>
#include "Utils/GenericUtils.h"
#include "Render/VulkanRenderEngine.h"
#include "GBuffer.h"
#include "Application/Application.h"
#include "Core/Logger.h"
#include "Render/DebugRender.h"
#include "RenderSystem/RenderSystem.h"


#define SSAO_NOISE_SAMPLES 16

namespace Mist
{
	CFloatVar CVar_SSAORadius("r_ssaoRadius", 1.98f);
	CFloatVar CVar_SSAOBias("r_ssaoBias", 0.087f);
	CBoolVar CVar_SSAOHalfRes("r_ssaoHalfRes", false);


	SSAO::SSAO(Renderer* renderer, IRenderEngine* engine)
		: RenderProcess(renderer, engine), m_mode(SSAO_Enabled)
	{	}

	void SSAO::Init(rendersystem::RenderSystem* rs)
	{
		CreateResources(rs);

		// shaders
		{
            rendersystem::ShaderBuildDescription shaderDesc;
            shaderDesc.vsDesc.filePath = "shaders/quad.vert";
            shaderDesc.fsDesc.filePath = "shaders/ssao.frag";
            shaderDesc.fsDesc.options.PushMacroDefinition("KERNEL_SIZE", SSAO_KERNEL_SAMPLES);
            m_ssaoShader = rs->CreateShader(shaderDesc);
        }
		{
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.vsDesc.filePath = "shaders/quad.vert";
			shaderDesc.fsDesc.filePath = "shaders/ssaoblur.frag";
			m_blurShader = rs->CreateShader(shaderDesc);
		}
	}

	void SSAO::Destroy(rendersystem::RenderSystem* rs)
	{
		rs->DestroyShader(&m_blurShader);
		rs->DestroyShader(&m_ssaoShader);
		m_rt = nullptr;
		m_blurRT = nullptr;
	}

	void SSAO::Draw(rendersystem::RenderSystem* rs)
	{
		CPU_PROFILE_SCOPE(CpuSSAO);
		{
			m_ssaoParams.Projection = GetCameraData()->Projection;
			m_ssaoParams.InverseProjection = glm::inverse(m_ssaoParams.Projection);
			m_ssaoParams.Bias = CVar_SSAOBias.Get();
			m_ssaoParams.Radius = CVar_SSAORadius.Get();
			m_ssaoParams.Bypass = m_mode == SSAO_Disabled ? 0.f : 1.f;

			rs->SetDefaultGraphicsState();
			rs->BeginMarker("SSAO");
			rs->ClearState();
			rs->SetShader(m_ssaoShader);
			rs->SetRenderTarget(m_rt);
			rs->SetViewport(m_rt->m_info.GetViewport());
			rs->SetScissor(m_rt->m_info.GetScissor());

			rs->SetDepthEnable(false, false);
			rs->SetShaderProperty("u_ssao", &m_ssaoParams, sizeof(m_ssaoParams));

			const GBuffer* gbuffer = static_cast<const GBuffer*>(GetRenderer()->GetRenderProcess(RENDERPROCESS_GBUFFER));
			rs->SetTextureSlot("u_GBufferPosition", *gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::RT_POSITION].texture);
			rs->SetTextureSlot("u_GBufferNormal", gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::RT_NORMAL].texture);
			//rs->SetTextureSlot("u_GBufferDepth", gbuffer->GetRenderTarget()->m_description.depthStencilAttachment.texture);
			rs->SetTextureSlot("u_SSAONoise", m_noiseTexture);
			rs->DrawFullscreenQuad();
			rs->ClearState();
			rs->EndMarker();
		}

		{
			rs->BeginMarker("SSAO Blur");
			rs->SetShader(m_blurShader);
			rs->SetRenderTarget(m_blurRT);
			rs->SetViewport(m_blurRT->m_info.GetViewport());
			rs->SetScissor(m_blurRT->m_info.GetScissor());
			glm::vec4 blurParams = { 1.f / (float)m_rt->m_info.extent.width, 1.f / (float)m_rt->m_info.extent.height, 0.f, 0.f };
			rs->SetShaderProperty("u_data", &blurParams, sizeof(glm::vec4));
			rs->SetTextureSlot("u_ssaoTex", m_rt->m_description.colorAttachments[0].texture);
			rs->DrawFullscreenQuad();

			rs->ClearState();
			rs->EndMarker();
		}
	}

	void SSAO::ImGuiDraw()
	{
		ImGui::Begin("SSAO");
		ImGuiUtils::EditCFloatVar(CVar_SSAORadius);
		ImGuiUtils::EditCFloatVar(CVar_SSAOBias);
		if (ImGui::BeginCombo("Debug texture", SSAOModeStr[m_mode]))
		{
			for (uint32_t i = 0; i < CountOf(SSAOModeStr); ++i)
			{
				if (ImGui::Selectable(SSAOModeStr[i], i == m_mode))
					m_mode = (eSSAOMode)i;
			}
			ImGui::EndCombo();
		}
		if (ImGui::Button("Reset resources"))
		{
			CreateResources(g_render);
		}
		ImGui::End();
	}

	void SSAO::DebugDraw()
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

	void SSAO::CreateResources(rendersystem::RenderSystem* rs)
	{
		std::uniform_real_distribution<float> randomFloat(0.f, 1.f);
		std::default_random_engine generator;

		// generate kernel samples
		for (uint32_t i = 0; i < SSAO_KERNEL_SAMPLES; ++i)
		{
			float scl = (float)i / (float)SSAO_KERNEL_SAMPLES;
			scl = math::Lerp(0.1f, 1.f, scl * scl);
			glm::vec3 kernel = glm::normalize(glm::vec3{ randomFloat(generator) * 2.f - 1.f, randomFloat(generator) * 2.f - 1.f, randomFloat(generator) });
			m_ssaoParams.KernelSamples[i] = glm::vec4(kernel, 1.f);
			m_ssaoParams.KernelSamples[i] *= randomFloat(generator);
			m_ssaoParams.KernelSamples[i] *= scl;
		}

		// Generate noise texture
		typedef float noise_t;
		auto generateNoise = [&]()->noise_t { return (randomFloat(generator) * 2.f - 1.f); };
		const render::Format textureNoiseFormat = render::Format_R32G32B32A32_SFloat;

		noise_t ssaoNoise[4 * SSAO_NOISE_SAMPLES];
		for (uint32_t i = 0; i < SSAO_NOISE_SAMPLES; ++i)
		{
			ssaoNoise[(i * 4) + 0] = generateNoise();
			ssaoNoise[(i * 4) + 1] = generateNoise();
			ssaoNoise[(i * 4) + 2] = 0;
			ssaoNoise[(i * 4) + 3] = 1;
		}

		{
			render::TextureDescription noiseDesc;
			noiseDesc.extent = { 4,4,1 };
			noiseDesc.format = textureNoiseFormat;
			noiseDesc.isShaderResource = true;
			m_noiseTexture = g_device->CreateTexture(noiseDesc);
			render::utils::UploadContext upload(rs->GetDevice());
			upload.WriteTexture(m_noiseTexture, 0, 0, ssaoNoise, sizeof(noise_t) * CountOf(ssaoNoise));
			upload.Submit();
		}

		// SSAO render target
		uint32_t width = rs->GetRenderResolution().width;
		uint32_t height = rs->GetRenderResolution().height;
		if (CVar_SSAOHalfRes.Get())
		{
			width >>= 1;
			height >>= 1;
		}

		{
			render::TextureDescription texDesc;
			texDesc.extent = { .width = width, .height = height, .depth = 1 };
			texDesc.format = render::Format_R8_UNorm;
			texDesc.isShaderResource = true;
			texDesc.isRenderTarget = true;
			render::TextureHandle texture = g_device->CreateTexture(texDesc);

			render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			m_rt = g_device->CreateRenderTarget(rtDesc);

		}

		// Blur render target
		{
			render::TextureDescription texDesc;
			texDesc.extent = { .width = width, .height = height, .depth = 1 };
			texDesc.format = render::Format_R8_UNorm;
			texDesc.isShaderResource = true;
			texDesc.isRenderTarget = true;
			render::TextureHandle texture = g_device->CreateTexture(texDesc);

			render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			m_blurRT = g_device->CreateRenderTarget(rtDesc);
		}
	}

}