#include "RenderPass.h"
#include <vector>
#include "VulkanRenderEngine.h"
#include "Logger.h"
#include "InitVulkanTypes.h"
#include "DeferredRender.h"
#include "SceneImpl.h"
#include "VulkanBuffer.h"
#include "RenderDescriptor.h"
#include "imgui_internal.h"
#include "Renderers/DebugRenderer.h"

#define GBUFFER_RT_FORMAT_POSITION FORMAT_R32G32B32A32_SFLOAT
#define GBUFFER_RT_FORMAT_NORMAL GBUFFER_RT_FORMAT_POSITION
#define GBUFFER_RT_FORMAT_ALBEDO FORMAT_R8G8B8A8_UNORM
#define GBUFFER_RT_FORMAT_DEPTH FORMAT_D32_SFLOAT


#define GBUFFER_RT_LAYOUT_POSITION IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
#define GBUFFER_RT_LAYOUT_NORMAL GBUFFER_RT_LAYOUT_POSITION
#define GBUFFER_RT_LAYOUT_ALBEDO GBUFFER_RT_LAYOUT_POSITION
#define GBUFFER_RT_LAYOUT_DEPTH IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL

#define GBUFFER_COMPOSITION_FORMAT FORMAT_R8G8B8A8_UNORM
#define GBUFFER_COMPOSITION_LAYOUT IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 


namespace Mist
{
	void GBuffer::Init(const RenderContext& renderContext)
	{
		tClearValue clearValue{ .color = {0.2f, 0.2f, 0.2f, 1.f} };
		RenderTargetDescription description;
		description.AddColorAttachment(GBUFFER_RT_FORMAT_POSITION, GBUFFER_RT_LAYOUT_POSITION, SAMPLE_COUNT_1_BIT, clearValue);
		description.AddColorAttachment(GBUFFER_RT_FORMAT_NORMAL, GBUFFER_RT_LAYOUT_NORMAL, SAMPLE_COUNT_1_BIT, clearValue);
		description.AddColorAttachment(GBUFFER_RT_FORMAT_ALBEDO, GBUFFER_RT_LAYOUT_ALBEDO, SAMPLE_COUNT_1_BIT, clearValue);
		description.DepthAttachmentDescription.Format = GBUFFER_RT_FORMAT_DEPTH;
		description.DepthAttachmentDescription.Layout = GBUFFER_RT_LAYOUT_DEPTH;
		description.DepthAttachmentDescription.MultisampledBit = SAMPLE_COUNT_1_BIT;
		clearValue.depthStencil.depth = 1.f;
		description.DepthAttachmentDescription.ClearValue = clearValue;
		description.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		description.RenderArea.offset = { .x = 0, .y = 0 };
		m_renderTarget.Create(renderContext, description);
		InitPipeline(renderContext);
	}

	void GBuffer::Destroy(const RenderContext& renderContext)
	{
		m_renderTarget.Destroy(renderContext);
	}

	void GBuffer::InitFrameData(const RenderContext& renderContext, UniformBuffer* buffer, uint32_t frameIndex)
	{
		// MRT
		VkDescriptorBufferInfo modelSetInfo = buffer->GenerateDescriptorBufferInfo(UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY);
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindBuffer(0, &modelSetInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
			.Build(renderContext, m_frameData[frameIndex].DescriptorSetArray[0]);
	}

	void GBuffer::DrawPass(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		VkCommandBuffer cmd = frameContext.GraphicsCommand;

		// MRT
		BeginGPUEvent(renderContext, cmd, "GBuffer_MRT", 0xffff00ff);
		m_renderTarget.BeginPass(cmd);
		m_shader->UseProgram(cmd);
		m_shader->BindDescriptorSets(cmd, &frameContext.CameraDescriptorSet, 1);
		frameContext.Scene->Draw(cmd, m_shader, 2, 1, m_frameData[frameContext.FrameIndex].DescriptorSetArray[0]);
		m_renderTarget.EndPass(frameContext.GraphicsCommand);
		EndGPUEvent(renderContext, cmd);
	}

	void GBuffer::ImGuiDraw()
	{
	}

	const RenderTarget& GBuffer::GetRenderTarget() const
	{
		return m_renderTarget;
	}

	void GBuffer::InitPipeline(const RenderContext& renderContext)
	{
		// MRT pipeline
		ShaderProgramDescription shaderDesc;
		shaderDesc.DynamicBuffers.push_back("u_model");
		shaderDesc.VertexShaderFile = SHADER_FILEPATH("mrt.vert");
		shaderDesc.FragmentShaderFile = SHADER_FILEPATH("mrt.frag");
		shaderDesc.RenderTarget = &m_renderTarget;
		shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
		m_shader = ShaderProgram::Create(renderContext, shaderDesc);
	}

	void DeferredLighting::Init(const RenderContext& renderContext)
	{
		tClearValue clearValue{ .color = {0.2f, 0.2f, 0.2f, 1.f} };
		RenderTargetDescription description;
		description.AddColorAttachment(GBUFFER_COMPOSITION_FORMAT, GBUFFER_COMPOSITION_LAYOUT, SAMPLE_COUNT_1_BIT, clearValue);
		description.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		description.RenderArea.offset = { .x = 0, .y = 0 };
		m_renderTarget.Create(renderContext, description);

		// Deferred pipeline
		ShaderProgramDescription shaderDesc;
		shaderDesc.VertexShaderFile = SHADER_FILEPATH("quad.vert");
		shaderDesc.FragmentShaderFile = SHADER_FILEPATH("deferred.frag");
		shaderDesc.RenderTarget = &m_renderTarget;
		shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
		m_shader = ShaderProgram::Create(renderContext, shaderDesc);

		SamplerBuilder builder;
		m_sampler = builder.Build(renderContext);

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
		m_quadVB.Init(renderContext, bufferInfo);
		bufferInfo.Data = indices;
		bufferInfo.Size = sizeof(indices);
		m_quadIB.Init(renderContext, bufferInfo);
	}

	void DeferredLighting::Destroy(const RenderContext& renderContext)
	{
		m_renderTarget.Destroy(renderContext);
		m_quadIB.Destroy(renderContext);
		m_quadVB.Destroy(renderContext);
		m_sampler.Destroy(renderContext);
	}

	void DeferredLighting::InitFrameData(const RenderContext& renderContext, UniformBuffer* buffer, uint32_t frameIndex, const GBuffer& gbuffer)
	{
		// Composition
		VkDescriptorBufferInfo info = buffer->GenerateDescriptorBufferInfo(UNIFORM_ID_SCENE_ENV_DATA);


		GBuffer::EGBufferTarget rts[3] = { GBuffer::EGBufferTarget::RT_POSITION, GBuffer::EGBufferTarget::RT_NORMAL, GBuffer::EGBufferTarget::RT_ALBEDO };
		tArray<VkDescriptorImageInfo, 3> infoArray;
		for (uint32_t i = 0; i < 3; ++i)
		{
			infoArray[i].sampler = m_sampler.GetSampler();
			infoArray[i].imageLayout = tovk::GetImageLayout(gbuffer.GetRenderTarget().GetDescription().ColorAttachmentDescriptions[rts[i]].Layout);
			infoArray[i].imageView = gbuffer.GetRenderTarget().GetRenderTarget(rts[i]);
		}
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindBuffer(0, &info, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(1, &infoArray[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(2, &infoArray[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(3, &infoArray[2], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(renderContext, m_frameData[frameIndex].Set);
	}

	void DeferredLighting::DrawPass(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		VkCommandBuffer cmd = frameContext.GraphicsCommand;
		// Composition
		BeginGPUEvent(renderContext, cmd, "Deferred lighting", 0xff00ffff);
		m_renderTarget.BeginPass(cmd);
		m_shader->UseProgram(cmd);
		m_shader->BindDescriptorSets(cmd, &m_frameData[frameContext.FrameIndex].Set, 1);
		m_quadVB.Bind(cmd);
		m_quadIB.Bind(cmd);
		vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
		m_renderTarget.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);
	}
}