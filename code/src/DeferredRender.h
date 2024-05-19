#pragma once

#include "RenderPass.h"
#include "Framebuffer.h"
#include "Texture.h"
#include "RenderTarget.h"

namespace Mist
{
#if 0
	struct RenderContext;


	class GBuffer
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

		void Init(const RenderContext& renderContext);
		void Destroy(const RenderContext& renderContext);
		void InitFrameData(const RenderContext& renderContext, UniformBuffer* buffer, uint32_t frameIndex);

		void DrawPass(const RenderContext& renderContext, const RenderFrameContext& frameContext);
		void ImGuiDraw();

		const RenderTarget& GetRenderTarget() const;

	private:
		void InitPipeline(const RenderContext& renderContext);
	public:
		RenderTarget m_renderTarget;
		ShaderProgram* m_shader;
		FrameData m_frameData[globals::MaxOverlappedFrames];
	};

	class DeferredLighting
	{
		struct FrameData
		{
			VkDescriptorSet Set;
		};
	public:
		void Init(const RenderContext& renderContext);
		void Destroy(const RenderContext& renderContext);
		void InitFrameData(const RenderContext& renderContext, UniformBuffer* buffer, uint32_t frameIndex, const GBuffer& gbuffer);
		void DrawPass(const RenderContext& renderContext, const RenderFrameContext& frameContext);
		const RenderTarget& GetRenderTarget() const { return m_renderTarget; }

	private:
		RenderTarget m_renderTarget;
		ShaderProgram* m_shader;
		FrameData m_frameData[globals::MaxOverlappedFrames];
		VertexBuffer m_quadVB;
		IndexBuffer m_quadIB;
		Sampler m_sampler;
	};
#endif // 0

}