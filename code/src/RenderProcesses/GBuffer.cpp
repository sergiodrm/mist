#include "GBuffer.h"
#include "VulkanBuffer.h"
#include "RenderContext.h"
#include "VulkanRenderEngine.h"
#include "SceneImpl.h"
#include <imgui/imgui.h>


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

		Sampler sampler = CreateSampler(renderContext);

		for (uint32_t i = 0; i < 3; ++i)
		{
			VkDescriptorImageInfo info;
			info.imageLayout = tovk::GetImageLayout(description.ColorAttachmentDescriptions[i].Layout);
			info.imageView = m_renderTarget.GetRenderTarget(i);
			info.sampler = sampler;
			DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
				.BindImage(0, &info, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.Build(renderContext, m_debugTexDescriptors[i]);
		}
		VkDescriptorImageInfo info;
		info.imageLayout = tovk::GetImageLayout(description.DepthAttachmentDescription.Layout);
		info.imageView = m_renderTarget.GetDepthBuffer();
		info.sampler = sampler;
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindImage(0, &info, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(renderContext, m_debugTexDescriptors[3]);
	}

	void GBuffer::Destroy(const RenderContext& renderContext)
	{
		m_renderTarget.Destroy(renderContext);
	}

	void GBuffer::InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
		// MRT
		VkDescriptorBufferInfo modelSetInfo = buffer.GenerateDescriptorBufferInfo(UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY);
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindBuffer(0, &modelSetInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
			.Build(renderContext, m_frameData[frameIndex].DescriptorSetArray[0]);
		static int c = 0;
		char buff[64];
		sprintf_s(buff, "DescriptorSet_GBuffer_%d", c++);
		SetVkObjectName(renderContext, &m_frameData[frameIndex].DescriptorSetArray[0], VK_OBJECT_TYPE_DESCRIPTOR_SET, buff);
	}

	void GBuffer::UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext)
	{
		switch (m_debugTex)
		{
		case RT_POSITION: 
		case RT_NORMAL:
		case RT_ALBEDO:
		case RT_DEPTH:
			frameContext.PresentTex = m_debugTexDescriptors[m_debugTex]; 
				break;
		default:
			break;

		}
	}

	void GBuffer::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
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
		ImGui::Begin("GBuffer");
		static const char* rts[] = { "None", "Position", "Normal", "Albedo", "Depth" };
		static int index = 0;
		if (ImGui::BeginCombo("GBuffer tex", rts[index]))
		{
			for (uint32_t i = 0; i < 5; ++i)
			{
				if (ImGui::Selectable(rts[i], i == index))
				{
					index = i;
					m_debugTex = (EGBufferTarget)(index ? (index - 1) : -1);
				}
			}
			ImGui::EndCombo();
		}
		ImGui::End();
	}

	const RenderTarget* GBuffer::GetRenderTarget(uint32_t index) const
	{
		return &m_renderTarget;
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
}