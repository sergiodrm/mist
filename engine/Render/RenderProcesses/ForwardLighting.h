#pragma once

#include "RenderProcess.h"
#include "Render/RenderTarget.h"
#include "Render/RenderProcesses/Bloom.h"

namespace Mist
{
	struct RenderContext;
	class ShaderProgram;
	class cModel;

	class ForwardLighting : public RenderProcess
	{
	public:
		ForwardLighting();
		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_FORWARD_LIGHTING; }
		virtual void Init(const RenderContext& renderContext) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void InitFrameData(const RenderContext& context, const Renderer& renderFrame, uint32_t frameIndex, UniformBufferMemoryPool& buffer) override;
		virtual void UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& renderFrameContext) override;
		virtual void Draw(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext) override;
		virtual void ImGuiDraw() override;
		virtual const RenderTarget* GetRenderTarget(uint32_t index) const override;
	protected:

	protected:
		// Render State
		ShaderProgram* m_shader;
		RenderTarget* m_rt;

		ShaderProgram* m_skyboxShader;
		cModel* m_skyboxModel;
	};
}