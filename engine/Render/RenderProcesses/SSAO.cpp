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
		loginfo("SSAO Kernel samples:\n");
		for (uint32_t i = 0; i < SSAO_KERNEL_SAMPLES; ++i)
		{
			float scl = (float)i / (float)SSAO_KERNEL_SAMPLES;
			scl = math::Lerp(0.1f, 1.f, scl * scl);
			glm::vec3 kernel = glm::normalize(glm::vec3{ randomFloat(generator) * 2.f - 1.f, randomFloat(generator) * 2.f - 1.f, randomFloat(generator) });
			m_uboData.KernelSamples[i] = glm::vec4(kernel, 1.f);
			m_uboData.KernelSamples[i] *= randomFloat(generator);
			m_uboData.KernelSamples[i] *= scl;
			logfinfo("#%d: %.5f, %.5f, %.5f, %.5f\n", i, m_uboData.KernelSamples[i].x, m_uboData.KernelSamples[i].y, m_uboData.KernelSamples[i].z, m_uboData.KernelSamples[i].w);
		}
		m_uboData.Radius = 0.5f;
		m_uboData.Bias = 0.025f;

		glm::vec4 ssaoNoise[SSAO_NOISE_SAMPLES];
		loginfo("SSAO noise texture:\n");
		for (uint32_t i = 0; i < SSAO_NOISE_SAMPLES; ++i)
		{
			ssaoNoise[i] = { randomFloat(generator) * 2.f - 1.f, randomFloat(generator) * 2.f - 1.f, 0.f, 1.f };
			logfinfo("#%d: %.5f, %.5f, %.5f, %.5f\n", i, ssaoNoise[i].x, ssaoNoise[i].y, ssaoNoise[i].z, ssaoNoise[i].w);
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
		m_ssaoShader->SetupDescriptors(renderContext);

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
		m_blurShader->SetupDescriptors(renderContext);
	}

	void SSAO::Destroy(const RenderContext& renderContext)
	{
		cTexture::Destroy(renderContext, m_noiseTexture);
		m_rt.Destroy(renderContext);
		m_blurRT.Destroy(renderContext);
	}

	void SSAO::InitFrameData(const RenderContext& context, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
		const GBuffer* gbuffer = (GBuffer*)renderer.GetRenderProcess(RENDERPROCESS_GBUFFER);
		m_gbufferTextures[0] = gbuffer->GetRenderTarget()->GetAttachment(GBuffer::RT_POSITION).Tex;
		m_gbufferTextures[1] = gbuffer->GetRenderTarget()->GetAttachment(GBuffer::RT_NORMAL).Tex;
		m_gbufferTextures[2] = gbuffer->GetRenderTarget()->GetAttachment(GBuffer::RT_ALBEDO).Tex;
		m_gbufferTextures[3] = gbuffer->GetRenderTarget()->GetAttachment(GBuffer::RT_DEPTH).Tex;
	}

	void SSAO::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& frameContext)
	{
		m_uboData.Projection = frameContext.CameraData->Projection;
		m_uboData.InverseProjection = glm::inverse(m_uboData.Projection);
	}

	void SSAO::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		VkCommandBuffer cmd = frameContext.GraphicsCommandContext.CommandBuffer;
		BeginGPUEvent(renderContext, cmd, "SSAO");
		m_rt.BeginPass(renderContext, cmd);
		if (m_mode != SSAO_Disabled)
		{
			m_ssaoShader->UseProgram(renderContext);
			m_ssaoShader->SetBufferData(renderContext, "u_ssao", &m_uboData, sizeof(m_uboData));
			const cTexture* texArray[] = { m_gbufferTextures[0], m_gbufferTextures[1], m_gbufferTextures[2], m_gbufferTextures[3], m_noiseTexture };
			//m_ssaoShader->BindTextureArraySlot(renderContext, 1, texArray, sizeof(texArray)/sizeof(cTexture*));
			m_ssaoShader->BindTextureSlot(renderContext, 1, *m_gbufferTextures[0]);
			m_ssaoShader->BindTextureSlot(renderContext, 2, *m_gbufferTextures[3]);
			m_ssaoShader->BindTextureSlot(renderContext, 3, *m_gbufferTextures[1]);
			m_ssaoShader->BindTextureSlot(renderContext, 4, *m_noiseTexture);
			m_ssaoShader->FlushDescriptors(renderContext);
			CmdDrawFullscreenQuad(cmd);
		}
		m_rt.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);

		BeginGPUEvent(renderContext, cmd, "SSAOBlur");
		m_blurRT.BeginPass(renderContext, cmd);
		if (m_mode != SSAO_Disabled && m_mode != SSAO_NoBlur)
		{
			m_blurShader->UseProgram(renderContext);
			const cTexture* tex = m_rt.GetAttachment(0).Tex;
			m_blurShader->BindTextureSlot(renderContext, 0, *tex);
			m_ssaoShader->FlushDescriptors(renderContext);
			CmdDrawFullscreenQuad(cmd);
		}
		m_blurRT.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);
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