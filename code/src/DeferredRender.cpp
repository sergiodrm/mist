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
			m_mrt.m_renderTarget.Create(renderContext, description);
		}
		{
			tClearValue clearValue{ .color = {0.2f, 0.2f, 0.2f, 1.f} };
			RenderTargetDescription description;
			description.AddColorAttachment(GBUFFER_COMPOSITION_FORMAT, GBUFFER_COMPOSITION_LAYOUT, SAMPLE_COUNT_1_BIT, clearValue);
			description.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
			description.RenderArea.offset = { .x = 0, .y = 0 };
			m_composition.m_renderTarget.Create(renderContext, description);
		}
		InitPipeline(renderContext);

		// Binding to draw debug render target
		SamplerBuilder samplerBuilder;
		m_sampler = samplerBuilder.Build(renderContext);
		for (uint32_t i = 0; i < 4; i++)
		{
			VkDescriptorImageInfo imageInfo;
			imageInfo.imageLayout = i == 3 ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = m_mrt.m_renderTarget.GetRenderTarget(i);
			imageInfo.sampler = m_sampler.GetSampler();
			DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
				.BindImage(0, &imageInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.Build(renderContext, m_debugTexDescriptors[i]);
		}

		VkDescriptorImageInfo imageInfo;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = m_composition.m_renderTarget.GetRenderTarget(0);
		imageInfo.sampler = m_sampler.GetSampler();
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindImage(0, &imageInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(renderContext, m_debugTexDescriptors[4]);
	}

	void GBuffer::Destroy(const RenderContext& renderContext)
	{
		m_quadIB.Destroy(renderContext);
		m_quadVB.Destroy(renderContext);
		m_sampler.Destroy(renderContext);
		m_mrt.Destroy(renderContext);
		m_composition.Destroy(renderContext);
	}

	void GBuffer::InitFrameData(const RenderContext& renderContext, UniformBuffer* buffer, uint32_t frameIndex)
	{
		// MRT
		VkDescriptorBufferInfo modelSetInfo = buffer->GenerateDescriptorBufferInfo(UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY);
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindBuffer(0, &modelSetInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
			.Build(renderContext, m_mrt.m_frameData[frameIndex].DescriptorSetArray[0]);

		// Composition
		{
			VkDescriptorBufferInfo info = buffer->GenerateDescriptorBufferInfo(UNIFORM_ID_SCENE_ENV_DATA);
			DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
				.BindBuffer(0, &info, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.Build(renderContext, m_composition.m_frameData[frameIndex].DescriptorSetArray[0]);
		}
		{
			tArray<VkDescriptorImageInfo, 3> infoArray;
			for (uint32_t i = 0; i < 3; ++i)
			{
				infoArray[i].sampler = m_sampler.GetSampler();
				infoArray[i].imageLayout = tovk::GetImageLayout(m_mrt.m_renderTarget.GetDescription().ColorAttachmentDescriptions[i].Layout);
				infoArray[i].imageView = m_mrt.m_renderTarget.GetRenderTarget(i);
			}
			DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
				.BindImage(0, &infoArray[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.BindImage(1, &infoArray[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.BindImage(2, &infoArray[2], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.Build(renderContext, m_composition.m_frameData[frameIndex].DescriptorSetArray[1]);
		}
	}

	void GBuffer::DrawPass(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		VkCommandBuffer cmd = frameContext.GraphicsCommand;

		// MRT
		BeginGPUEvent(renderContext, cmd, "GBuffer_MRT", 0xffff00ff);
		m_mrt.m_renderTarget.BeginPass(cmd);
		m_mrt.m_shader->UseProgram(cmd);
		m_mrt.m_shader->BindDescriptorSets(cmd, &frameContext.CameraDescriptorSet, 1);
		frameContext.Scene->Draw(cmd, m_mrt.m_shader, 2, 1, m_mrt.m_frameData[frameContext.FrameIndex].DescriptorSetArray[0]);
		m_mrt.m_renderTarget.EndPass(frameContext.GraphicsCommand);
		EndGPUEvent(renderContext, cmd);

		// Composition
		BeginGPUEvent(renderContext, cmd, "GBuffer_Composition", 0xff00ffff);
		m_composition.m_renderTarget.BeginPass(cmd);
		m_composition.m_shader->UseProgram(cmd);
		auto& sets = m_composition.m_frameData[frameContext.FrameIndex].DescriptorSetArray;
		m_composition.m_shader->BindDescriptorSets(cmd, sets.data(), 2);
		m_quadVB.Bind(cmd);
		m_quadIB.Bind(cmd);
		vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
		m_composition.m_renderTarget.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);
	}

	void GBuffer::ImGuiDraw()
	{
		ImGui::Begin("GBuffer");
		static const char* targets[] = { "Position", "Normals", "Albedo", "Depth", "Composition"};
		static uint32_t rtindex = 0;
		if (ImGui::BeginCombo("Gbuffer target", targets[rtindex]))
		{
			for (uint32_t i = 0; i < 5; ++i)
			{
				bool selected = i == rtindex;
				if (ImGui::Selectable(targets[i], &selected))
				{
					rtindex = i;
					debugrender::SetDebugTexture(m_debugTexDescriptors[rtindex]);
					break;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::End();
	}

	VkImageView GBuffer::GetRenderTarget(EGBufferTarget target) const
	{
		return m_mrt.m_renderTarget.GetRenderTarget(target);
	}

	VkImageView GBuffer::GetComposition() const 
	{
		return m_composition.m_renderTarget.GetRenderTarget(0);
	}

	void GBuffer::InitPipeline(const RenderContext& renderContext)
	{
		// MRT pipeline
		{
			ShaderProgramDescription shaderDesc;
			shaderDesc.SetLayoutArray.resize(3);
			// Set Layout for per frame data (camera proj etc...)
			DescriptorSetLayoutBuilder::Create(*renderContext.LayoutCache)
				.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1)
				.Build(renderContext, &shaderDesc.SetLayoutArray[0]);
			// Set Layout for per mesh data (model mat, textures...)
			DescriptorSetLayoutBuilder::Create(*renderContext.LayoutCache)
				.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT, 1)
				.Build(renderContext, &shaderDesc.SetLayoutArray[1]);
			shaderDesc.SetLayoutArray[2] = MaterialRenderData::GetDescriptorSetLayout(renderContext, *renderContext.LayoutCache);

			shaderDesc.VertexShaderFile = SHADER_FILEPATH("mrt.vert");
			shaderDesc.FragmentShaderFile = SHADER_FILEPATH("mrt.frag");
			shaderDesc.RenderTarget = &m_mrt.m_renderTarget;
			shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
			m_mrt.m_shader = ShaderProgram::Create(renderContext, shaderDesc);
		}

		// Deferred pipeline
		{
			// Empty input layout
			ShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile = SHADER_FILEPATH("deferred.vert");
			shaderDesc.FragmentShaderFile = SHADER_FILEPATH("deferred.frag");
			shaderDesc.RenderTarget = &m_composition.m_renderTarget;
			shaderDesc.InputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float3, EAttributeType::Float2 });
			m_composition.m_shader = ShaderProgram::Create(renderContext, shaderDesc);

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
	}
}