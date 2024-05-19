#pragma once

#include "RenderTarget.h"
#include "VulkanBuffer.h"
#include "Texture.h"
#include <glm/glm.hpp>
#include "Globals.h"
#include "RenderContext.h"
#include "RenderProcess.h"

#define SSAO_KERNEL_SAMPLES 64

namespace Mist
{
	class ShaderProgram;
	class UniformBufferMemoryPool;
	class GBuffer;

	class SSAO : public RenderProcess
	{
		struct SSAOUBO
		{
			float Radius;
			float Bias;
			float __padding[2];
			glm::mat4 Projection;
			glm::vec4 KernelSamples[SSAO_KERNEL_SAMPLES];
		};

		struct FrameData
		{
			VkDescriptorSet SSAOSet;
			VkDescriptorSet SSAOBlurSet;

			VkDescriptorSet SSAOTex;
			VkDescriptorSet SSAOBlurTex;
		};
	public:
		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_SSAO; }
		virtual void Init(const RenderContext& renderContext) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void InitFrameData(const RenderContext& context, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer) override;
		virtual void UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext) override;
		virtual void Draw(const RenderContext& context, const RenderFrameContext& frameContext) override;
		virtual void ImGuiDraw() override;
		virtual const RenderTarget* GetRenderTarget(uint32_t index) const override { return &m_blurRT; }
	private:
		ShaderProgram* m_ssaoShader;
		RenderTarget m_rt;
		Texture m_noiseTexture;
		Sampler m_noiseSampler;
		FrameData m_frameData[globals::MaxOverlappedFrames];
		SSAOUBO m_uboData;
		VertexBuffer m_screenQuadVB;
		IndexBuffer m_screenQuadIB;

		ShaderProgram* m_blurShader;
		RenderTarget m_blurRT;

		uint32_t m_debugTex = 0;
	};

}