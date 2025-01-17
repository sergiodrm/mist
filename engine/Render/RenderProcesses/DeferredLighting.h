#pragma once

#include "RenderProcess.h"
#include "Render/RenderTarget.h"
#include "Render/Globals.h"
#include "Render/VulkanBuffer.h"
#include "Render/Texture.h"
#include "Bloom.h"
#include <glm/glm.hpp>


namespace Mist
{
	struct RenderContext;
	class ShaderProgram;

	struct HDRParams
	{
		float GammaCorrection = 1.f;
		float Exposure = 1.f;
	};

	class DeferredLighting : public RenderProcess
	{
	public:
		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_LIGHTING; }
		virtual void Init(const RenderContext& renderContext) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer) override;
		virtual void UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& frameContext) override;
		virtual void Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext) override;
		virtual const RenderTarget* GetRenderTarget(uint32_t index = 0) const override{ return &m_hdrOutput; }
		virtual void ImGuiDraw() override;
		virtual void DebugDraw(const RenderContext& context) override;
	private:
		RenderTarget m_lightingOutput;
		ShaderProgram* m_lightingShader;
		ShaderProgram* m_lightingFogShader;
		ShaderProgram* m_skyboxShader;

		ShaderProgram* m_hdrShader;
		RenderTarget m_hdrOutput;
		HDRParams m_hdrParams;

		const RenderTarget* m_gbufferRenderTarget;
		tArray<const RenderTarget*, globals::MaxShadowMapAttachments> m_shadowMapRenderTargetArray;
		const RenderTarget* m_ssaoRenderTarget;

		BloomEffect m_bloomEffect;
	};

#if 0
	class ForwardLighting : public RenderProcess
	{
		struct FrameData
		{
			RenderTarget RT;

			// Camera, models and environment
			VkDescriptorSet PerFrameSet;
			VkDescriptorSet ModelSet;
		};
	public:
		ForwardLighting();
		virtual void Init(const RenderContext& renderContext) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void InitFrameData(const RenderContext& context, const Renderer& renderFrame, uint32_t frameIndex, UniformBufferMemoryPool& buffer) override;
		virtual void UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& renderFrameContext) override;
		virtual void Draw(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext) override;
		virtual void ImGuiDraw() override;
		virtual const RenderTarget* GetRenderTarget(uint32_t index) const override;
	protected:

	protected:
		// Render State
		ShaderProgram* m_shader;

		tArray<FrameData, globals::MaxOverlappedFrames> m_frameData;

		Sampler m_depthMapSampler;
		bool m_debugCameraDepthMapping;
	};
#endif // 0

}