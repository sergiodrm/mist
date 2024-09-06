#pragma once

#include "Render/RenderTarget.h"
#include "Render/VulkanBuffer.h"
#include "Render/Texture.h"
#include <glm/glm.hpp>
#include "Render/Globals.h"
#include "Render/RenderContext.h"
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
	public:
		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_SSAO; }
		virtual void Init(const RenderContext& renderContext) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void InitFrameData(const RenderContext& context, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer) override;
		virtual void UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext) override;
		virtual void Draw(const RenderContext& context, const RenderFrameContext& frameContext) override;
		virtual void ImGuiDraw() override;
		virtual void DebugDraw(const RenderContext& context) override;
		virtual const RenderTarget* GetRenderTarget(uint32_t index) const override { return &m_blurRT; }
	private:
		ShaderProgram* m_ssaoShader;
		RenderTarget m_rt;
		Texture* m_noiseTexture;
		Texture* m_gbufferTextures[4];
		SSAOUBO m_uboData;

		ShaderProgram* m_blurShader;
		RenderTarget m_blurRT;

		uint32_t m_debugTex = 0;
	};

}