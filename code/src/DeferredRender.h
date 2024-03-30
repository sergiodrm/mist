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
			VkDescriptorSet ModelSet;
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
		void InitRenderPass(const RenderContext& renderContext);
		void InitFramebuffer(const RenderContext& renderContext);
		void InitPipeline(const RenderContext& renderContext);
	public:
		RenderPass m_renderPass;
		Framebuffer m_framebuffer;
		RenderPipeline m_pipeline;
		RenderTarget m_renderTarget;


		FrameData m_frameData[globals::MaxOverlappedFrames];

		// Show render targets in debug mode.
		Sampler m_sampler;
		VkDescriptorSet m_rtDescriptors[4];
	};
}