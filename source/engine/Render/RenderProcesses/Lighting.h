#pragma once

#include "RenderProcess.h"
#include "Render/Globals.h"
#include "Bloom.h"
#include <glm/glm.hpp>


namespace Mist
{
	class cModel;

	class Lighting : public RenderProcess
	{
	public:
		Lighting(Renderer* renderer, IRenderEngine* engine);
		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_LIGHTING; }
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
	};

}