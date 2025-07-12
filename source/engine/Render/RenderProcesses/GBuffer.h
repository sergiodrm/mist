#pragma once

#include "RenderProcess.h"
#include "Render/RenderTarget.h"
#include "Render/Globals.h"
#include "Render/Texture.h"
#include "RenderAPI/Device.h"

namespace rendersystem
{
    class ShaderProgram;
}

namespace Mist
{
	struct RenderContext;
	class ShaderProgram;
	class cModel;

	class GBuffer : public RenderProcess
	{
		enum EDebugMode
		{
			DEBUG_NONE,
			DEBUG_POSITION,
			DEBUG_NORMAL,
			DEBUG_ALBEDO,
			DEBUG_DEPTH,
			DEBUG_ALL
		};

	public:

		enum EGBufferTarget
		{
			RT_POSITION,
			RT_NORMAL,
			RT_ALBEDO,
			RT_EMISSIVE,
			RT_DEPTH_STENCIL,

			RT_COUNT
		};

		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_GBUFFER;  }
		virtual void Init(const RenderContext& renderContext) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer) override;
		virtual void UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext) override;
		virtual void Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext) override;
		virtual void ImGuiDraw() override;
		virtual render::RenderTarget* GetRenderTarget(uint32_t index = 0) const override;

		static render::RenderTarget* GetGBuffer();

		static render::Format GetGBufferFormat(EGBufferTarget target);

	private:
		void InitPipeline(const RenderContext& renderContext);
		virtual void DebugDraw(const RenderContext& context) override;
	public:
		render::RenderTargetHandle m_renderTarget;
		rendersystem::ShaderProgram* m_gbufferShader;
		EDebugMode m_debugMode = DEBUG_NONE;
	};
}