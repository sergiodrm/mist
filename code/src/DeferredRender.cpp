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

#define GBUFFER_RT_FORMAT_POSITION FORMAT_R16G16B16A16_SFLOAT
#define GBUFFER_RT_FORMAT_NORMAL GBUFFER_RT_FORMAT_POSITION
#define GBUFFER_RT_FORMAT_ALBEDO FORMAT_R8G8B8A8_UNORM
#define GBUFFER_RT_FORMAT_DEPTH FORMAT_D32_SFLOAT


#define ASSET_ROOT_PATH "../../assets/"
#define SHADER_ROOT_PATH ASSET_ROOT_PATH "shaders/compiled/"

namespace vkmmc
{
	void GBuffer::Init(const RenderContext& renderContext)
	{
		tClearValue clearValue{ .color = {1.f, 1.f, 1.f, 1.f} };
		RenderTargetDescription description;
		description.AddColorAttachment(GBUFFER_RT_FORMAT_POSITION, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValue);
		description.AddColorAttachment(GBUFFER_RT_FORMAT_NORMAL, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValue);
		description.AddColorAttachment(GBUFFER_RT_FORMAT_ALBEDO, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValue);
		description.DepthAttachmentDescription.Format = GBUFFER_RT_FORMAT_DEPTH;
		description.DepthAttachmentDescription.Layout = IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
		description.DepthAttachmentDescription.MultisampledBit = SAMPLE_COUNT_1_BIT;
		description.DepthAttachmentDescription.ClearValue = clearValue;
		description.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		description.RenderArea.offset = { .x = 0, .y = 0 };
		m_renderTarget.Create(renderContext, description);

		InitRenderPass(renderContext);
		InitFramebuffer(renderContext);
		InitPipeline(renderContext);
	}

	void GBuffer::Destroy(const RenderContext& renderContext)
	{
		m_sampler.Destroy(renderContext);
		m_pipeline.Destroy(renderContext);
		m_framebuffer.Destroy(renderContext);
		m_renderPass.Destroy(renderContext);
		m_renderTarget.Destroy(renderContext);
	}

	void GBuffer::InitFrameData(const RenderContext& renderContext, UniformBuffer* buffer, uint32_t frameIndex)
	{
		VkDescriptorBufferInfo modelSetInfo = buffer->GenerateDescriptorBufferInfo(UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY);
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindBuffer(0, &modelSetInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
			.Build(renderContext, m_frameData[frameIndex].ModelSet);
	}

	void GBuffer::DrawPass(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		VkCommandBuffer cmd = frameContext.GraphicsCommand;
		//m_renderPass.BeginPass(frameContext.GraphicsCommand, m_framebuffer.GetFramebufferHandle());
		m_renderTarget.Bind(cmd);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetPipelineHandle());

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetPipelineLayoutHandle(),
			0, 1, &frameContext.CameraDescriptorSet, 0, nullptr);
		frameContext.Scene->Draw(cmd, m_pipeline.GetPipelineLayoutHandle(), 2, 1, m_frameData[frameContext.FrameIndex].ModelSet);
		m_renderPass.EndPass(frameContext.GraphicsCommand);
	}

	void GBuffer::ImGuiDraw()
	{
		ImGui::Begin("GBuffer");
		static bool debugEnabled = false;
		static int rtindex = 0;
		if (ImGui::Checkbox("Enable debug gbuffer", &debugEnabled))
		{
			debugrender::SetDebugTexture(m_rtDescriptors[rtindex]);
		}
		if (debugEnabled && ImGui::SliderInt("GBuffer target", &rtindex, 0, 2))
		{
			debugrender::SetDebugTexture(m_rtDescriptors[rtindex]);
		}
		ImGui::End();
	}

	VkImageView GBuffer::GetRenderTarget(EGBufferTarget target) const
	{
		return m_framebuffer.GetImageViewAt(target);
	}

	void GBuffer::InitRenderPass(const RenderContext& renderContext)
	{
		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		m_renderPass.RenderPass = RenderPassBuilder::Create()
			.AddAttachment(GBUFFER_RT_FORMAT_POSITION, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			.AddAttachment(GBUFFER_RT_FORMAT_NORMAL, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			.AddAttachment(GBUFFER_RT_FORMAT_ALBEDO, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			.AddAttachment(GBUFFER_RT_FORMAT_DEPTH, IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
			.AddSubpass({ 0, 1, 2 }, 3, {})
			.AddDependencies(dependencies.data(), (uint32_t)dependencies.size())
			.Build(renderContext);
		m_renderPass.OffsetX = 0;
		m_renderPass.OffsetY = 0;
		m_renderPass.Width = renderContext.Window->Width;
		m_renderPass.Height = renderContext.Window->Height;
		VkClearValue clearValue;
		clearValue.color = { 0.f, 0.f, 0.f };
		m_renderPass.ClearValues.resize(3, clearValue);
		clearValue.depthStencil.depth = 1.f;
		m_renderPass.ClearValues.push_back(clearValue);
	}

	void GBuffer::InitFramebuffer(const RenderContext& renderContext)
	{
		Framebuffer::Builder builder(renderContext, { .width = m_renderPass.Width, .height = m_renderPass.Height, .depth = 1 });
		builder.CreateAttachment(GBUFFER_RT_FORMAT_POSITION, IMAGE_USAGE_COLOR_ATTACHMENT_BIT | IMAGE_USAGE_SAMPLED_BIT);
		builder.CreateAttachment(GBUFFER_RT_FORMAT_NORMAL, IMAGE_USAGE_COLOR_ATTACHMENT_BIT | IMAGE_USAGE_SAMPLED_BIT);
		builder.CreateAttachment(GBUFFER_RT_FORMAT_ALBEDO, IMAGE_USAGE_COLOR_ATTACHMENT_BIT | IMAGE_USAGE_SAMPLED_BIT);
		builder.CreateAttachment(GBUFFER_RT_FORMAT_DEPTH, IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | IMAGE_USAGE_SAMPLED_BIT);
		builder.Build(m_framebuffer, m_renderPass.RenderPass);

		// Binding to draw debug render target
		SamplerBuilder samplerBuilder;
		m_sampler = samplerBuilder.Build(renderContext);
		for (uint32_t i = 0; i < 3; i++)
		{
			VkDescriptorImageInfo imageInfo;
			imageInfo.imageLayout = i == 3 ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			//imageInfo.imageView = m_framebuffer.GetImageViewAt(i);
			imageInfo.imageView = m_renderTarget.GetRenderTarget(i);
			imageInfo.sampler = m_sampler.GetSampler();
			DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
				.BindImage(0, &imageInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.Build(renderContext, m_rtDescriptors[i]);
		}
	}

	void GBuffer::InitPipeline(const RenderContext& renderContext)
	{
		// MRT pipeline

		{
			std::array<VkDescriptorSetLayout, 3> layouts;
			// Set Layout for per frame data (camera proj etc...)
			DescriptorSetLayoutBuilder::Create(*renderContext.LayoutCache)
				.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1)
				.Build(renderContext, &layouts[0]);
			// Set Layout for per mesh data (model mat, textures...)
			DescriptorSetLayoutBuilder::Create(*renderContext.LayoutCache)
				.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT, 1)
				.Build(renderContext, &layouts[1]);
			layouts[2] = MaterialRenderData::GetDescriptorSetLayout(renderContext, *renderContext.LayoutCache);

			ShaderDescription descs[2] =
			{
				{.Filepath = SHADER_ROOT_PATH "mrt.vert.spv", .Stage = VK_SHADER_STAGE_VERTEX_BIT},
				{.Filepath = SHADER_ROOT_PATH "mrt.frag.spv", .Stage = VK_SHADER_STAGE_FRAGMENT_BIT}
			};

			m_pipeline = RenderPipeline::Create(renderContext, m_renderTarget.GetRenderPass(), 0,
				descs, 2, layouts.data(), (uint32_t)layouts.size(),
				nullptr, 0, VertexInputLayout::GetStaticMeshVertexLayout(),
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 3);
		}

		// Deferred pipeline
		{
		}
	}
}