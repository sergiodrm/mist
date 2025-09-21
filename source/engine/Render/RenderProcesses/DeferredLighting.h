#pragma once

#include "RenderProcess.h"
#include "Render/Globals.h"
#include "Bloom.h"
#include <glm/glm.hpp>


namespace Mist
{
	class cModel;

	class DeferredLighting : public RenderProcess
	{
	public:
		DeferredLighting(Renderer* renderer, IRenderEngine* engine);
		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_LIGHTING; }
		virtual void Init(rendersystem::RenderSystem* rs) override;
		virtual void Destroy(rendersystem::RenderSystem* rs) override;
		virtual void Draw(rendersystem::RenderSystem* rs) override;
		virtual render::RenderTarget* GetRenderTarget(uint32_t index = 0) const override{ return m_hdrOutput.GetPtr(); }
		virtual void ImGuiDraw() override;
		virtual void DebugDraw() override;
		render::RenderTargetHandle m_lightingOutput;
	private:
		rendersystem::ShaderProgram* m_lightingShader;
		rendersystem::ShaderProgram* m_lightingFogShader;
		rendersystem::ShaderProgram* m_skyboxShader;
		cModel* m_skyModel;

		rendersystem::ShaderProgram* m_hdrShader;
		render::RenderTargetHandle m_hdrOutput;

		BloomEffect m_bloomEffect;
	};

#if 0
	class ForwardLighting : public RenderProcess
	{
		struct FrameData
		{
			RenderTarget RT;

			// Camera, models and environment
			VkDescriptorSet PerFrameSet;
			VkDescriptorSet ModelSet;
		};
	public:
		ForwardLighting();
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

		tArray<FrameData, globals::MaxOverlappedFrames> m_frameData;

		Sampler m_depthMapSampler;
		bool m_debugCameraDepthMapping;
	};
#endif // 0

}