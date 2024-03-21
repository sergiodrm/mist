#pragma once

#include "RenderPass.h"
#include "Framebuffer.h"

namespace vkmmc
{
	struct RenderContext;

	class GBuffer
	{
		struct FrameData
		{
			VkDescriptorSet ModelSet;
		};
	public:

		void Init(const RenderContext& renderContext);
		void Destroy(const RenderContext& renderContext);

		void InitFrameData(const RenderContext& renderContext, UniformBuffer* buffer, uint32_t frameIndex);

		void DrawPass(const RenderContext& renderContext, const RenderFrameContext& frameContext);

	private:
		void InitRenderPass(const RenderContext& renderContext);
		void InitFramebuffer(const RenderContext& renderContext);
		void InitPipeline(const RenderContext& renderContext);
	public:
		RenderPass m_renderPass;
		Framebuffer m_framebuffer;
		RenderPipeline m_pipeline;
		FrameData m_frameData[globals::MaxOverlappedFrames];
	};
}