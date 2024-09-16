#include "SSAO.h"
#include <random>
#include <imgui/imgui.h>
#include <glm/geometric.hpp>
#include "Utils/GenericUtils.h"
#include "Render/RenderContext.h"
#include "Render/VulkanRenderEngine.h"
#include "GBuffer.h"
#include "Application/Application.h"


#define SSAO_NOISE_SAMPLES 16

namespace Mist
{
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
			ssaoNoise[i] = { randomFloat(generator) * 2.f - 1.f, randomFloat(generator) * 2.f - 1.f, 0.f, 1.f };
		tImageDescription imageDesc;
		imageDesc.Width = 4;
		imageDesc.Height = 4;
		imageDesc.Depth = 1;
		imageDesc.Format = FORMAT_R8G8B8A8_UNORM;
		imageDesc.Flags = 0;
		imageDesc.Layers = 1;
		imageDesc.MipLevels = 1;
		imageDesc.SampleCount = SAMPLE_COUNT_1_BIT;
		m_noiseTexture = Texture::Create(renderContext, imageDesc);
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
		m_rt.Create(renderContext, rtDesc);

		// Shader
		GraphicsShaderProgramDescription shaderDesc;
		shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
		shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("ssao.frag");
		shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
		shaderDesc.RenderTarget = &m_rt;
		m_ssaoShader = ShaderProgram::Create(renderContext, shaderDesc);
		m_ssaoShader->SetupDescriptors(renderContext);

		// Blur RT
		RenderTargetDescription blurRtDesc;
		blurRtDesc.AddColorAttachment(FORMAT_R8_UNORM, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, value);
		blurRtDesc.RenderArea = rtDesc.RenderArea;
		m_blurRT.Create(renderContext, blurRtDesc);

		// Blur shader
		GraphicsShaderProgramDescription blurShaderDesc;
		blurShaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
		blurShaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("ssaoblur.frag");
		blurShaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
		blurShaderDesc.RenderTarget = &m_blurRT;
		m_blurShader = ShaderProgram::Create(renderContext, blurShaderDesc);
		m_blurShader->SetupDescriptors(renderContext);
	}

	void SSAO::Destroy(const RenderContext& renderContext)
	{
		Texture::Destroy(renderContext, m_noiseTexture);
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
		m_rt.BeginPass(cmd);
		m_ssaoShader->UseProgram(renderContext);
		m_ssaoShader->SetBufferData(renderContext, "u_ssao", &m_uboData, sizeof(m_uboData));
		const Texture* texArray[] = { m_gbufferTextures[0], m_gbufferTextures[1], m_gbufferTextures[2], m_gbufferTextures[3], m_noiseTexture };
		m_ssaoShader->BindTextureArraySlot(renderContext, 1, texArray, sizeof(texArray)/sizeof(Texture*));
		m_ssaoShader->FlushDescriptors(renderContext);
		CmdDrawFullscreenQuad(cmd);
		m_rt.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);

		BeginGPUEvent(renderContext, cmd, "SSAOBlur");
		m_blurRT.BeginPass(cmd);
		m_blurShader->UseProgram(renderContext);
		const Texture* tex = m_rt.GetAttachment(0).Tex;
		m_blurShader->BindTextureSlot(renderContext, 0, *tex);
		m_ssaoShader->FlushDescriptors(renderContext);
		CmdDrawFullscreenQuad(cmd);
		m_blurRT.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);
	}

	void SSAO::ImGuiDraw()
	{
		ImGui::Begin("SSAO");
		ImGui::DragFloat("Radius", &m_uboData.Radius, 0.02f, 0.f, 5.f);
		ImGui::DragFloat("Bias", &m_uboData.Bias, 0.001f, 0.f, 0.05f);
		static const char* debugModes[] = { "None", "SSAO", "SSAO Blur" };
		if (ImGui::BeginCombo("Debug texture", debugModes[m_debugTex]))
		{
			for (uint32_t i = 0; i < 3; ++i)
			{
				if (ImGui::Selectable(debugModes[i], i == m_debugTex))
					m_debugTex = i;
			}
			ImGui::EndCombo();
		}
		ImGui::End();
	}

	void SSAO::DebugDraw(const RenderContext& context)
	{
		switch (m_debugTex)
		{
		case 0: break;
		case 1:
			DebugRender::DrawScreenQuad({ 0.75f * 1920.f, 0.f }, { 0.25f * 1920.f, 0.25f * 1080.f }, m_rt.GetAttachment(0).Tex->GetView(0), IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			break;
		case 2:
			DebugRender::DrawScreenQuad({ 0.75f * 1920.f, 0.f }, { 0.25f * 1920.f, 0.25f * 1080.f }, m_blurRT.GetAttachment(0).Tex->GetView(0), IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			break;
		}
	}

}