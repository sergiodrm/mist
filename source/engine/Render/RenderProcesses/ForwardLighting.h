#pragma once

#include "RenderProcess.h"
#include "Render/RenderProcesses/Bloom.h"

namespace Mist
{
	class cModel;

	class ForwardLighting : public RenderProcess
	{
	public:
		ForwardLighting(Renderer* renderer, IRenderEngine* engine);
		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_FORWARD_LIGHTING; }
		virtual void Init(rendersystem::RenderSystem* rs) override;
		virtual void Destroy(rendersystem::RenderSystem* rs) override;
		virtual void Draw(rendersystem::RenderSystem* rs) override;
		virtual void ImGuiDraw() override;
		virtual render::RenderTarget* GetRenderTarget(uint32_t index) const override;
	protected:

	protected:
		// Render State
		rendersystem::ShaderProgram* m_shader;
		render::RenderTargetHandle m_rt;

		rendersystem::ShaderProgram* m_skyboxShader;
		cModel* m_skyboxModel;
	};
}