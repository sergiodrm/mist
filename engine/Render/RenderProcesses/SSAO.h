#pragma once

#include "Render/RenderTarget.h"
#include "Render/VulkanBuffer.h"
#include "Render/Texture.h"
#include <glm/glm.hpp>
#include "Render/Globals.h"
#include "Render/RenderContext.h"
#include "RenderProcess.h"

#define SSAO_KERNEL_SAMPLES 32

namespace Mist
{
	class ShaderProgram;
	class UniformBufferMemoryPool;
	class GBuffer;

	class SSAO : public RenderProcess
	{
		enum eSSAOMode
		{
			SSAO_Disabled,
			SSAO_Enabled,
			SSAO_NoBlur,
			SSAO_DebugView,
			SSAO_DebugBlurView,
			SSAO_DebugNoiseView,
		};
		static constexpr const char* const SSAOModeStr[] = {
			{"SSAO_Disabled"},
			{"SSAO_Enabled"},
			{"SSAO_NoBlur"},
			{"SSAO_DebugView"},
			{"SSAO_DebugBlurView"},
			{"SSAO_DebugNoiseView"}
		};
		struct SSAOUBO
		{
			float Radius;
			float Bias;
			float __padding[2];
			glm::mat4 Projection;
			glm::mat4 InverseProjection;
			glm::vec4 KernelSamples[SSAO_KERNEL_SAMPLES];
		};
	public:
		SSAO();
		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_SSAO; }
		virtual void Init(const RenderContext& renderContext) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void InitFrameData(const RenderContext& context, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer) override;
		virtual void UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext) override;
		virtual void Draw(const RenderContext& context, const RenderFrameContext& frameContext) override;
		virtual void ImGuiDraw() override;
		virtual void DebugDraw(const RenderContext& context) override;
		virtual const RenderTarget* GetRenderTarget(uint32_t index) const override { return m_blurRT; }
	private:
		ShaderProgram* m_ssaoShader;
		RenderTarget* m_rt;
		SSAOUBO m_uboData;
		cTexture* m_noiseTexture;
		const Renderer* m_renderer;

		ShaderProgram* m_blurShader;
		RenderTarget* m_blurRT;

		eSSAOMode m_mode;
	};

}