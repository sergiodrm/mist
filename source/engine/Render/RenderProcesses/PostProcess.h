#pragma once

#include "RenderProcess.h"
#include "Render/Globals.h"
#include <glm/glm.hpp>


namespace Mist
{
	/************************************************************************/
	/* TAA                                                                  */
	/************************************************************************/
	class TAA
	{
	public:

		enum
		{
			TAA_Basic,
			TAA_ClampingColor,

			TAA_ShaderCount
		};

		void Init(rendersystem::RenderSystem* rs);
		void Destroy(rendersystem::RenderSystem* rs);
		void Draw(rendersystem::RenderSystem* rs, const render::RenderTargetHandle& rt);

		const render::RenderTargetHandle& GetOutput() const;
		const render::RenderTargetHandle& GetHistory() const;

		void ImGuiDraw();

	private:
		rendersystem::ShaderProgram* GetTAAShader() const { return m_taaShader[m_taaShaderIndex]; }

	private:
		render::RenderTargetHandle m_rts[2];
		rendersystem::ShaderProgram* m_taaShader[TAA_ShaderCount];
		uint32_t m_taaShaderIndex{TAA_ClampingColor};
	};

	/************************************************************************/
	/* Bloom                                                                */
	/************************************************************************/
	class Bloom
	{
	public:

		enum { Bloom_ChainLevels = 5 };

		struct Config
		{
			enum
			{
				BloomMode_Disabled,
				BloomMode_Active,
				BloomMode_DebugFilterPass,
				BloomMode_DebugDownscalePass
			};
			uint32_t bloomMode = BloomMode_Active;
			float mixCompositeAlpha = 0.5f;
			float upscaleFilterRadius = 0.005f;
			float threshold = 1.5f;
			float knee = 0.1f;
		};

		Bloom();

		void Init(rendersystem::RenderSystem* rs);
		void Draw(rendersystem::RenderSystem* rs, const render::RenderTargetHandle& inputRt);
		void Destroy(rendersystem::RenderSystem* rs);

		inline void SetConfig(const Config& config) { m_config = config; }

		void ImGuiDraw();

	private:
		// Config of bloom draw. Can be updated before each draw.
		Config m_config;

		struct
		{
			rendersystem::ShaderProgram* filter;
			rendersystem::ShaderProgram* downsample;
			rendersystem::ShaderProgram* upsample;
			rendersystem::ShaderProgram* compose;
		} m_shaders;
		tArray<render::RenderTargetHandle, Bloom_ChainLevels> m_renderTargetArray;
		tArray<render::TextureHandle, Bloom_ChainLevels> m_renderTargetTexturesArray;
	};

	/************************************************************************/
	/* PostProcess                                                          */
	/************************************************************************/
	class PostProcess : public RenderProcess
	{
	public:
		PostProcess(Renderer* renderer, IRenderEngine* engine);

		static RenderProcessType GetProcessTypeStatic() { return RENDERPROCESS_POSTPRO; }
		virtual RenderProcessType GetProcessType() const override { return GetProcessTypeStatic(); }
		virtual void Init(rendersystem::RenderSystem* rs) override;
		virtual void Destroy(rendersystem::RenderSystem* rs) override;
		virtual void Draw(rendersystem::RenderSystem* rs) override;
		virtual render::RenderTarget* GetRenderTarget(uint32_t index = 0) const override{ return m_hdrOutput.GetPtr(); }
		virtual void ImGuiDraw() override;
		virtual void DebugDraw() override;
	private:
		rendersystem::ShaderProgram* m_hdrShader;
		render::RenderTargetHandle m_hdrOutput;
		Bloom m_bloomEffect;
		TAA m_taa;
	};

}