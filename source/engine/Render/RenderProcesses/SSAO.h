#pragma once

#include <glm/glm.hpp>
#include "Render/Globals.h"
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
			float Bypass;
			float __padding;
			glm::mat4 Projection;
			glm::mat4 InverseProjection;
			glm::vec4 KernelSamples[SSAO_KERNEL_SAMPLES];
		};
	public:
		SSAO(Renderer* renderer, IRenderEngine* engine);
		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_SSAO; }
		virtual void Init(rendersystem::RenderSystem* rs) override;
		virtual void Destroy(rendersystem::RenderSystem* rs) override;
		virtual void Draw(rendersystem::RenderSystem* rs) override;
		virtual void ImGuiDraw() override;
		virtual void DebugDraw() override;
		virtual render::RenderTarget* GetRenderTarget(uint32_t index = 0) const override;
	private:
		rendersystem::ShaderProgram* m_ssaoShader;
		render::RenderTargetHandle m_rt;
		render::TextureHandle m_noiseTexture;

		SSAOUBO m_ssaoParams;

		rendersystem::ShaderProgram* m_blurShader;
		render::RenderTargetHandle m_blurRT;

		eSSAOMode m_mode;
	};

}