#pragma once

#include "RenderProcess.h"
#include "RenderTarget.h"
#include "Globals.h"

namespace Mist
{
	struct RenderContext;
	class ShaderProgram;


	class GBuffer : public RenderProcess
	{
		struct FrameData
		{
			tArray<VkDescriptorSet, 4> DescriptorSetArray;
		};

	public:

		enum EGBufferTarget
		{
			RT_POSITION,
			RT_NORMAL,
			RT_ALBEDO,
			RT_DEPTH
		};

		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_GBUFFER;  }
		virtual void Init(const RenderContext& renderContext) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer) override;
		virtual void UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext) override {}
		virtual void Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext) override;
		virtual void ImGuiDraw() override;
		virtual const RenderTarget* GetRenderTarget(uint32_t index = 0) const override;

	private:
		void InitPipeline(const RenderContext& renderContext);
	public:
		RenderTarget m_renderTarget;
		ShaderProgram* m_shader;
		FrameData m_frameData[globals::MaxOverlappedFrames];
	};
}