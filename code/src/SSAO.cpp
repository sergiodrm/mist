#include "SSAO.h"
#include <random>
#include <glm/geometric.hpp>
#include "GenericUtils.h"
#include "RenderContext.h"
#include "VulkanRenderEngine.h"


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
			m_uboData.KernelSamples[i] = glm::normalize(glm::vec3{ randomFloat(generator) * 2.f - 1.f, randomFloat(generator) * 2.f - 1.f, randomFloat(generator) });
			m_uboData.KernelSamples[i] *= randomFloat(generator);
			m_uboData.KernelSamples[i] *= scl;
		}
		m_uboData.NoiseScale = { (float)renderContext.Window->Width * 0.25f, (float)renderContext.Window->Height * 0.25f };
		m_uboData.Radius = 0.25f;

		glm::vec3 ssaoNoise[SSAO_NOISE_SAMPLES];
		for (uint32_t i = 0; i < SSAO_NOISE_SAMPLES; ++i)
			ssaoNoise[i] = { randomFloat(generator) * 2.f - 1.f, randomFloat(generator) * 2.f - 1.f, 0.f };
		TextureCreateInfo texInfo;
		texInfo.Width = 4;
		texInfo.Height = 4;
		texInfo.Depth = 1;
		texInfo.Format = FORMAT_R16G16B16A16_SFLOAT;
		texInfo.PixelCount = 4 * 4 * 3;
		texInfo.Pixels = &ssaoNoise[0];
		m_noiseTexture.Init(renderContext, texInfo);

		SamplerBuilder builder;
		builder.MinFilter = FILTER_NEAREST;
		builder.MagFilter = FILTER_NEAREST;
		m_noiseSampler = builder.Build(renderContext);

		// Render target
		RenderTargetDescription rtDesc;
		tClearValue value = { .color = {1.f, 0.f, 0.f, 1.f} };
		rtDesc.AddColorAttachment(FORMAT_R8_UNORM, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, value);
		rtDesc.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		rtDesc.RenderArea.offset = { 0, 0 };
		m_rt.Create(renderContext, rtDesc);

		// Shader
		ShaderProgramDescription shaderDesc;
		shaderDesc.VertexShaderFile = SHADER_FILEPATH("quad.vert");
		shaderDesc.FragmentShaderFile = SHADER_FILEPATH("ssao.frag");
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
	}

	void SSAO::Destroy(const RenderContext& renderContext)
	{
		m_screenQuadIB.Destroy(renderContext);
		m_screenQuadVB.Destroy(renderContext);
		m_noiseTexture.Destroy(renderContext);
		m_noiseSampler.Destroy(renderContext);
		m_rt.Destroy(renderContext);
	}

	void SSAO::InitFrameData(const RenderContext& context, UniformBuffer* buffer, uint32_t frameIndex, const GBuffer& gbuffer)
	{
		buffer->AllocUniform(context, "SSAOUBO", sizeof(SSAOUBO));
		VkDescriptorBufferInfo bufferInfo = buffer->GenerateDescriptorBufferInfo("SSAOUBO");

		VkDescriptorImageInfo imageInfo[4];
		imageInfo[0].sampler = m_noiseSampler.GetSampler();
		imageInfo[0].imageLayout = tovk::GetImageLayout(gbuffer.GetRenderTarget().GetDescription().ColorAttachmentDescriptions[GBuffer::EGBufferTarget::RT_POSITION].Layout);
		imageInfo[0].imageView = gbuffer.GetRenderTarget().GetRenderTarget(GBuffer::EGBufferTarget::RT_POSITION);
		imageInfo[1].sampler = m_noiseSampler.GetSampler();
		imageInfo[1].imageLayout = tovk::GetImageLayout(gbuffer.GetRenderTarget().GetDescription().ColorAttachmentDescriptions[GBuffer::EGBufferTarget::RT_NORMAL].Layout);
		imageInfo[1].imageView = gbuffer.GetRenderTarget().GetRenderTarget(GBuffer::EGBufferTarget::RT_NORMAL);
		imageInfo[2].sampler = m_noiseSampler.GetSampler();
		imageInfo[2].imageLayout = tovk::GetImageLayout(gbuffer.GetRenderTarget().GetDescription().ColorAttachmentDescriptions[GBuffer::EGBufferTarget::RT_ALBEDO].Layout);
		imageInfo[2].imageView = gbuffer.GetRenderTarget().GetRenderTarget(GBuffer::EGBufferTarget::RT_ALBEDO);
		imageInfo[3].sampler = m_noiseSampler.GetSampler();
		imageInfo[3].imageLayout = tovk::GetImageLayout(IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		imageInfo[3].imageView = m_noiseTexture.GetImageView();

		DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator)
			.BindBuffer(0, &bufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(1, &imageInfo[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(2, &imageInfo[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(3, &imageInfo[2], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(4, &imageInfo[3], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(context, m_frameData[frameIndex].SSAOSet);

	}

	void SSAO::PrepareFrame(const RenderContext& renderContext, RenderFrameContext& frameContext)
	{
		m_uboData.Projection = frameContext.CameraData->Projection;
		frameContext.GlobalBuffer.SetUniform(renderContext, "SSAOUBO", &m_uboData, sizeof(SSAOUBO));
	}

	void SSAO::DrawPass(const RenderContext& renderContext, const RenderFrameContext& frameContext)
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
	}

}