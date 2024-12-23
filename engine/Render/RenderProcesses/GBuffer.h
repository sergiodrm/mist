#pragma once

#include "RenderProcess.h"
#include "Render/RenderTarget.h"
#include "Render/Globals.h"
#include "Render/Texture.h"


#define GBUFFER_RT_FORMAT_POSITION FORMAT_R32G32B32A32_SFLOAT
#define GBUFFER_RT_FORMAT_NORMAL GBUFFER_RT_FORMAT_POSITION
#define GBUFFER_RT_FORMAT_ALBEDO FORMAT_R8G8B8A8_UNORM
#define GBUFFER_RT_FORMAT_DEPTH FORMAT_D24_UNORM_S8_UINT // FORMAT_D32_SFLOAT
#define GBUFFER_RT_FORMAT_EMISSIVE FORMAT_R8G8B8A8_UNORM


#define GBUFFER_RT_LAYOUT_POSITION IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
#define GBUFFER_RT_LAYOUT_NORMAL GBUFFER_RT_LAYOUT_POSITION
#define GBUFFER_RT_LAYOUT_ALBEDO GBUFFER_RT_LAYOUT_POSITION
#define GBUFFER_RT_LAYOUT_DEPTH IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
#define GBUFFER_RT_LAYOUT_EMISSIVE GBUFFER_RT_LAYOUT_POSITION

#define HDR_FORMAT FORMAT_R16G16B16A16_SFLOAT
#define GBUFFER_COMPOSITION_FORMAT FORMAT_R8G8B8A8_UNORM
#define GBUFFER_COMPOSITION_LAYOUT IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 

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
			DEBUG_EMISSIVE,
			DEBUG_ALL
		};

	public:

		enum EGBufferTarget
		{
			RT_POSITION,
			RT_NORMAL,
			RT_ALBEDO,
			RT_EMISSIVE,
			RT_DEPTH,

			RT_COUNT
		};

		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_GBUFFER;  }
		virtual void Init(const RenderContext& renderContext) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer) override;
		virtual void UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext) override;
		virtual void Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext) override;
		virtual void ImGuiDraw() override;
		virtual const RenderTarget* GetRenderTarget(uint32_t index = 0) const override;

	private:
		void InitPipeline(const RenderContext& renderContext);
		virtual void DebugDraw(const RenderContext& context) override;
	public:
		RenderTarget m_renderTarget;
		ShaderProgram* m_gbufferShader;
		ShaderProgram* m_skyboxShader;
		cModel* m_skyboxModel;
		EDebugMode m_debugMode = DEBUG_NONE;
	};
}