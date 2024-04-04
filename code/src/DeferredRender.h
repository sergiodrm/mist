#pragma once

#include "RenderPass.h"
#include "Framebuffer.h"
#include "Texture.h"
#include "RenderTarget.h"

namespace vkmmc
{
	struct RenderContext;


	class GBuffer
	{
		struct FrameData
		{
			tArray<VkDescriptorSet, 4> DescriptorSetArray;
		};

		struct PassData
		{
			RenderTarget m_renderTarget;
			RenderPipeline m_pipeline;
			FrameData m_frameData[globals::MaxOverlappedFrames];

			inline void Destroy(const RenderContext& renderContext)
			{
				m_pipeline.Destroy(renderContext);
				m_renderTarget.Destroy(renderContext);
			}
		};

	public:

		enum EGBufferTarget
		{
			RT_POSITION,
			RT_NORMAL,
			RT_ALBEDO,
			RT_DEPTH
		};

		void Init(const RenderContext& renderContext);
		void Destroy(const RenderContext& renderContext);
		void InitFrameData(const RenderContext& renderContext, UniformBuffer* buffer, uint32_t frameIndex);

		void DrawPass(const RenderContext& renderContext, const RenderFrameContext& frameContext);		
		void ImGuiDraw();

		VkImageView GetRenderTarget(EGBufferTarget target) const;

	private:
		void InitPipeline(const RenderContext& renderContext);
	public:
		PassData m_mrt;
		PassData m_composition;

		// Show render targets in debug mode.
		Sampler m_sampler;
		VkDescriptorSet m_rtDescriptors[4];
	};
}