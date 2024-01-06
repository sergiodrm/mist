// Autogenerated code for vkmmc project
// Header file

#pragma once

#include "RendererBase.h"
#include "Mesh.h"

namespace vkmmc
{
	class PresentRenderer : public IRendererBase
	{
	public:
		virtual void Init(const RendererCreateInfo& info) override;
		virtual void Destroy(const RenderContext& renderContext) override;

		virtual void RecordCommandBuffer(const RenderFrameContext& renderFrameContext, const Model* models, uint32_t modelCount);

		void SetImageToRender(const RenderContext& renderContext, VkImageView imageView);
	private:
		RenderPass m_renderPass;
		std::vector<VkFramebuffer> m_framebufferArray;
		uint32_t m_extent[2];
		VkDescriptorSetLayout m_descriptorSetLayout;
		RenderPipeline m_pipeline;

		// Quad screen
		VertexBuffer m_quadVertexBuffer;
		IndexBuffer m_quadIndexBuffer;
		// Descriptor bound to screen image
		VkDescriptorSet m_descriptorSet;
		VkSampler m_sampler;
	};
}
