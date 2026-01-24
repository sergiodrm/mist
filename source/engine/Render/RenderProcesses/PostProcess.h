#pragma once

#include "RenderProcess.h"
#include "Render/Globals.h"
#include "Bloom.h"
#include <glm/glm.hpp>


namespace Mist
{
	class PostProcess : public RenderProcess
	{
	public:
		PostProcess(Renderer* renderer, IRenderEngine* engine);
		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_POSTPRO; }
		virtual void Init(rendersystem::RenderSystem* rs) override;
		virtual void Destroy(rendersystem::RenderSystem* rs) override;
		virtual void Draw(rendersystem::RenderSystem* rs) override;
		virtual render::RenderTarget* GetRenderTarget(uint32_t index = 0) const override{ return m_hdrOutput.GetPtr(); }
		virtual void ImGuiDraw() override;
		virtual void DebugDraw() override;
	private:
		rendersystem::ShaderProgram* m_hdrShader;
		render::RenderTargetHandle m_hdrOutput;
		BloomEffect m_bloomEffect;
	};

}