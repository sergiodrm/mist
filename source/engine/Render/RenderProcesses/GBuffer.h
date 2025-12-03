#pragma once

#include "RenderProcess.h"
#include "Render/Globals.h"
#include "RenderAPI/Device.h"

namespace rendersystem
{
    class ShaderProgram;
}

#define GBUFFER_GEOMETRY_STENCIL_MASK 0x0001

namespace Mist
{
	class cModel;

	class GBuffer : public RenderProcess
	{
		enum EDebugMode
		{
			DEBUG_NONE,
			DEBUG_NORMAL,
			DEBUG_ALBEDO,
			DEBUG_EMISSIVE,
			DEBUG_DEPTH,
			DEBUG_ALL
		};

	public:

		enum EGBufferTarget
		{
			RT_NORMAL,
			RT_ALBEDO,
			RT_EMISSIVE,
			RT_DEPTH_STENCIL,

			RT_COUNT
		};

		GBuffer(Renderer* renderer, IRenderEngine* engine);

		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_GBUFFER;  }
		virtual void Init(rendersystem::RenderSystem* rs) override;
		virtual void Destroy(rendersystem::RenderSystem* rs) override;
		virtual void Update();
		virtual void Draw(rendersystem::RenderSystem* rs) override;
		virtual void ImGuiDraw() override;
		virtual render::RenderTarget* GetRenderTarget(uint32_t index = 0) const override;

		static render::RenderTarget* GetGBuffer();

		static render::Format GetGBufferFormat(EGBufferTarget target);

	private:
		void InitPipeline(rendersystem::RenderSystem* rs);
		virtual void DebugDraw() override;
	public:
		render::RenderTargetHandle m_renderTarget;
		rendersystem::ShaderProgram* m_gbufferShader;
		EDebugMode m_debugMode = DEBUG_NONE;

		uint32_t m_renderListId;
	};
}