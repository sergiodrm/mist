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

		// init quad
		float vertices[] =
		{
			// vkscreencoords	// uvs
			-1.f, -1.f, 0.f,	0.f, 0.f,
			1.f, -1.f, 0.f,		1.f, 0.f,
			1.f, 1.f, 0.f,		1.f, 1.f,
			-1.f, 1.f, 0.f,		0.f, 1.f
		};
		uint32_t indices[] = { 0, 2, 1, 0, 3, 2 };
		BufferCreateInfo bufferInfo;
		bufferInfo.Data = vertices;
		bufferInfo.Size = sizeof(vertices);
		m_screenQuadVB.Init(renderContext, bufferInfo);
		bufferInfo.Data = indices;
		bufferInfo.Size = sizeof(indices);
		m_screenQuadIB.Init(renderContext, bufferInfo);

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
	}

	void SSAO::Destroy(const RenderContext& renderContext)
	{
		m_screenQuadIB.Destroy(renderContext);
		m_screenQuadVB.Destroy(renderContext);
		Texture::Destroy(renderContext, m_noiseTexture);
		m_rt.Destroy(renderContext);
		m_blurRT.Destroy(renderContext);
	}

	void SSAO::InitFrameData(const RenderContext& context, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
		buffer.AllocUniform(context, "SSAOUBO", sizeof(SSAOUBO));
		VkDescriptorBufferInfo bufferInfo = buffer.GenerateDescriptorBufferInfo("SSAOUBO");

		Sampler sampler = CreateSampler(context, FILTER_NEAREST, FILTER_NEAREST);

		tViewDescription viewDesc;
		const RenderProcess* gbuffer = renderer.GetRenderProcess(RENDERPROCESS_GBUFFER);
		const RenderTarget& rt = *gbuffer->GetRenderTarget();
		VkDescriptorImageInfo imageInfo[5];
		imageInfo[0].sampler = sampler;
		imageInfo[0].imageLayout = tovk::GetImageLayout(rt.GetDescription().ColorAttachmentDescriptions[GBuffer::RT_POSITION].Layout);
		imageInfo[0].imageView = rt.GetRenderTarget(GBuffer::RT_POSITION);
		imageInfo[1].sampler = sampler;
		imageInfo[1].imageLayout = tovk::GetImageLayout(rt.GetDescription().ColorAttachmentDescriptions[GBuffer::RT_NORMAL].Layout);
		imageInfo[1].imageView = rt.GetRenderTarget(GBuffer::RT_NORMAL);
		imageInfo[2].sampler = sampler;
		imageInfo[2].imageLayout = tovk::GetImageLayout(rt.GetDescription().ColorAttachmentDescriptions[GBuffer::RT_ALBEDO].Layout);
		imageInfo[2].imageView = rt.GetRenderTarget(GBuffer::RT_ALBEDO);
		imageInfo[3].sampler = sampler;
		imageInfo[3].imageLayout = tovk::GetImageLayout(rt.GetDescription().DepthAttachmentDescription.Layout);
		imageInfo[3].imageView = rt.GetRenderTarget(GBuffer::RT_DEPTH);
		imageInfo[4].sampler = sampler;
		imageInfo[4].imageLayout = tovk::GetImageLayout(IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		imageInfo[4].imageView = m_noiseTexture->CreateView(context, viewDesc);

		DescriptorBuilder builder = DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator);
		builder.BindBuffer(0, &bufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		for (uint32_t i = 0; i < sizeof(imageInfo) / sizeof(VkDescriptorImageInfo); ++i)
			builder.BindImage(i + 1, &imageInfo[i], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		builder.Build(context, m_frameData[frameIndex].SSAOSet);

		VkDescriptorImageInfo ssaoImageInfo;
		ssaoImageInfo.sampler = sampler;
		ssaoImageInfo.imageLayout = tovk::GetImageLayout(m_rt.GetDescription().ColorAttachmentDescriptions[0].Layout);
		ssaoImageInfo.imageView = m_rt.GetRenderTarget(0);
		DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator)
			.BindImage(0, &ssaoImageInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(context, m_frameData[frameIndex].SSAOBlurSet);

		VkDescriptorImageInfo ssaoBlurImageInfo;
		ssaoBlurImageInfo.sampler = sampler;
		ssaoBlurImageInfo.imageLayout = tovk::GetImageLayout(m_blurRT.GetDescription().ColorAttachmentDescriptions[0].Layout);
		ssaoBlurImageInfo.imageView = m_blurRT.GetRenderTarget(0);
		DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator)
			.BindImage(0, &ssaoBlurImageInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(context, m_frameData[frameIndex].SSAOBlurTex);
		m_frameData[frameIndex].SSAOTex = m_frameData[frameIndex].SSAOBlurSet;
	}

	void SSAO::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& frameContext)
	{
		m_uboData.Projection = frameContext.CameraData->Projection;
		frameContext.GlobalBuffer.SetUniform(renderContext, "SSAOUBO", &m_uboData, sizeof(SSAOUBO));
		switch (m_debugTex)
		{
		case 0: break;
		case 1: frameContext.PresentTex = m_frameData[frameContext.FrameIndex].SSAOTex; break;
		case 2: frameContext.PresentTex = m_frameData[frameContext.FrameIndex].SSAOBlurTex; break;
		}
	}

	void SSAO::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		VkCommandBuffer cmd = frameContext.GraphicsCommand;
		BeginGPUEvent(renderContext, cmd, "SSAO");
		m_rt.BeginPass(cmd);
		m_ssaoShader->UseProgram(cmd);
		m_ssaoShader->BindDescriptorSets(cmd, &m_frameData[frameContext.FrameIndex].SSAOSet, 1);
		m_screenQuadVB.Bind(cmd);
		m_screenQuadIB.Bind(cmd);
		vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
		m_rt.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);

		BeginGPUEvent(renderContext, cmd, "SSAOBlur");
		m_blurRT.BeginPass(cmd);
		m_blurShader->UseProgram(cmd);
		m_blurShader->BindDescriptorSets(cmd, &m_frameData[frameContext.FrameIndex].SSAOBlurSet, 1);
		m_screenQuadIB.Bind(cmd);
		m_screenQuadVB.Bind(cmd);
		vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
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

}