#pragma once

#include "RenderProcess.h"
#include "Render/Globals.h"
#include <glm/glm.hpp>


namespace Mist
{
	class cModel;

	class Lighting : public RenderProcess
	{
		struct ShadowMapParams
		{
			tArray<glm::mat4, globals::MaxShadowMapAttachments> lightViewProjectionArray;
			float noiseScale = 0.0025f;
			float noiseFactor = 3.f;
			float noisePCFKernelSize = 9.f;
			float _padding = 0.f;
		};
	public:
		Lighting(Renderer* renderer, IRenderEngine* engine);

		static RenderProcessType GetProcessTypeStatic() { return RENDERPROCESS_LIGHTING; }
		virtual RenderProcessType GetProcessType() const override { return GetProcessTypeStatic(); }
		virtual void Init(rendersystem::RenderSystem* rs) override;
		virtual void Destroy(rendersystem::RenderSystem* rs) override;
		virtual void Update() override;
		virtual void Draw(rendersystem::RenderSystem* rs) override;
		virtual render::RenderTarget* GetRenderTarget(uint32_t index = 0) const override{ return m_skyboxRt.GetPtr(); }
		virtual void ImGuiDraw() override;
		virtual void DebugDraw() override;
	private:
		rendersystem::ShaderProgram* m_lightingShader;
		rendersystem::ShaderProgram* m_lightingFogShader;
		rendersystem::ShaderProgram* m_skyboxShader;
		rendersystem::ShaderProgram* m_forwardLightingShader;
		uint32_t m_forwardRenderListId;
		cModel* m_skyModel;

		render::RenderTargetHandle m_lightingRt;
		render::RenderTargetHandle m_skyboxRt;

		render::BlendFactor m_colorSrc = render::BlendFactor_SrcAlpha;
		render::BlendFactor m_colorDst = render::BlendFactor_OneMinusSrcAlpha;
		render::BlendFactor m_alphaSrc = render::BlendFactor_One;
		render::BlendFactor m_alphaDst = render::BlendFactor_Zero;
		render::BlendOp m_blendOp = render::BlendOp_Add;

		ShadowMapParams m_shadowMapParams;
	};

}