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
		tImageDescription imageDesc;
		imageDesc.Width = 4;
		imageDesc.Height = 4;
		imageDesc.Depth = 1;
		imageDesc.Format = FORMAT_R32G32B32A32_SFLOAT;
		imageDesc.Flags = 0;
		imageDesc.Layers = 1;
		imageDesc.MipLevels = 1;
		imageDesc.SampleCount = SAMPLE_COUNT_1_BIT;
		imageDesc.SamplerDesc.MinFilter = FILTER_NEAREST;
		imageDesc.SamplerDesc.MagFilter = FILTER_NEAREST;
		imageDesc.SamplerDesc.AddressModeU = SAMPLER_ADDRESS_MODE_REPEAT;
		imageDesc.SamplerDesc.AddressModeV = SAMPLER_ADDRESS_MODE_REPEAT;
		imageDesc.SamplerDesc.AddressModeW = SAMPLER_ADDRESS_MODE_REPEAT;
		m_noiseTexture = cTexture::Create(renderContext, imageDesc);
		const uint8_t* pixels = (uint8_t*)ssaoNoise;
		m_noiseTexture->SetImageLayers(renderContext, &pixels, 1);

		tViewDescription viewDesc;
		m_noiseTexture->CreateView(renderContext, viewDesc);

		// Render target
		RenderTargetDescription rtDesc;
		tClearValue value = { .color = {1.f, 0.f, 0.f, 1.f} };
		rtDesc.AddColorAttachment(FORMAT_R8_UNORM, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, value);
		rtDesc.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		rtDesc.RenderArea.offset = { 0, 0 };
		rtDesc.ResourceName = "SSAO_RT";
		m_rt.Create(renderContext, rtDesc);

		// Shader
		tShaderProgramDescription shaderDesc;
		shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
		shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("ssao.frag");

		tCompileMacroDefinition macroDef("KERNEL_SIZE");
		macroDef.Value.Fmt("%d", SSAO_KERNEL_SAMPLES);
		shaderDesc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back(macroDef);

		shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
		shaderDesc.RenderTarget = &m_rt;
		m_ssaoShader = ShaderProgram::Create(renderContext, shaderDesc);

		// Blur RT
		RenderTargetDescription blurRtDesc;
		blurRtDesc.AddColorAttachment(FORMAT_R8_UNORM, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, value);
		blurRtDesc.RenderArea = rtDesc.RenderArea;
		blurRtDesc.ResourceName = "SSAO_BLUR_RT";
		m_blurRT.Create(renderContext, blurRtDesc);

		// Blur shader
		tShaderProgramDescription blurShaderDesc;
		blurShaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
		blurShaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("ssaoblur.frag");
		blurShaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
		blurShaderDesc.RenderTarget = &m_blurRT;
		m_blurShader = ShaderProgram::Create(renderContext, blurShaderDesc);
	}

	void SSAO::Destroy(const RenderContext& renderContext)
	{
		cTexture::Destroy(renderContext, m_noiseTexture);
		m_rt.Destroy(renderContext);
		m_blurRT.Destroy(renderContext);
	}

	void SSAO::InitFrameData(const RenderContext& context, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
		m_renderer = &renderer;
	}

	void SSAO::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& frameContext)
	{
		m_uboData.Projection = frameContext.CameraData->Projection;
		m_uboData.InverseProjection = glm::inverse(m_uboData.Projection);
	}

	void SSAO::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		CPU_PROFILE_SCOPE(CpuSSAO);
        CommandList* commandList = renderContext.CmdList;
        commandList->BeginMarker("SSAO");
		GpuProf_Begin(renderContext, "SSAO");
		if (m_mode != SSAO_Disabled)
		{
			commandList->SetGraphicsState({.Program = m_ssaoShader, .Rt = &m_rt });
			m_ssaoShader->SetBufferData(renderContext, "u_ssao", &m_uboData, sizeof(m_uboData));

			const GBuffer* gbuffer = static_cast<const GBuffer*>(m_renderer->GetRenderProcess(RENDERPROCESS_GBUFFER));
			m_ssaoShader->BindSampledTexture(renderContext, "u_GBufferPosition", *gbuffer->GetRenderTarget()->GetAttachment(GBuffer::RT_POSITION).Tex);
			m_ssaoShader->BindSampledTexture(renderContext, "u_GBufferNormal", *gbuffer->GetRenderTarget()->GetAttachment(GBuffer::RT_NORMAL).Tex);
			m_ssaoShader->BindSampledTexture(renderContext, "u_SSAONoise", *m_noiseTexture);
			commandList->BindProgramDescriptorSets();
			CmdDrawFullscreenQuad(commandList);
		}
		GpuProf_End(renderContext);
		commandList->EndMarker();

        commandList->BeginMarker("SSAOBlur");
		GpuProf_Begin(renderContext, "SSAO Blur");
		if (m_mode != SSAO_Disabled && m_mode != SSAO_NoBlur)
		{
            commandList->SetGraphicsState({ .Program = m_blurShader, .Rt = &m_blurRT });
			const cTexture* tex = m_rt.GetAttachment(0).Tex;
			m_blurShader->BindSampledTexture(renderContext, "u_ssaoTex", *tex);
			commandList->BindProgramDescriptorSets();
			CmdDrawFullscreenQuad(commandList);
		}
		GpuProf_End(renderContext);
        commandList->EndMarker();
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
		const cTexture* tex = nullptr;
		float scale = 1.f;
		switch (m_mode)
		{
		case SSAO_Disabled: 
		case SSAO_Enabled: 
		case SSAO_NoBlur: 
			return;
		case SSAO_DebugView:
			tex = m_rt.GetAttachment(0).Tex;
			break;
		case SSAO_DebugBlurView:
			tex = m_blurRT.GetAttachment(0).Tex;
			break;
		case SSAO_DebugNoiseView:
			tex = m_noiseTexture;
			scale = 100.f;
			break;
		}
		DebugRender::DrawScreenQuad({ 0.f, 0.f },
			{ scale * (float)tex->GetDescription().Width, scale * (float)tex->GetDescription().Height }, *tex);
	}

}