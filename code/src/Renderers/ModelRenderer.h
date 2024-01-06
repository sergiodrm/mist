// Autogenerated code for vkmmc project
// Header file

#pragma once
#include "RendererBase.h"

namespace vkmmc
{
	class ModelRenderer : public IRendererBase
	{
	public:
		virtual void Init(const RendererCreateInfo& info) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void RecordCommandBuffer(const RenderFrameContext& renderFrameContext,
			const Model* models, uint32_t modelCount) override;

		VkImageView GetRenderedImage() const;
	private:
		void BeginRenderPass(const RenderFrameContext& renderFrameContext);
		void EndRenderPass(const RenderFrameContext& renderFrameContext);


	protected:
		RenderPass m_renderPass;
		Framebuffer m_framebuffer;

		// Render State
		VkDescriptorSetLayout m_descriptorSetLayout;
		RenderPipeline m_renderPipeline;
	};
}
