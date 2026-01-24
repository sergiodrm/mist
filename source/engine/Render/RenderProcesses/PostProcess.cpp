#include "PostProcess.h"
#include "Lighting.h"
#include "RenderAPI/Device.h"
#include "RenderSystem/RenderSystem.h"
#include "Application/CmdParser.h"
#include "RenderProcess.h"
#include "Render/RendererBase.h"


namespace Mist
{
	CFloatVar CVar_GammaCorrection("r_gammacorrection", 2.2f);
	CFloatVar CVar_Exposure("r_exposure", 2.5f);

	PostProcess::PostProcess(Renderer* renderer, IRenderEngine* engine)
		: RenderProcess(renderer, engine)
	{
	}

	void PostProcess::Init(rendersystem::RenderSystem* rs)
	{
		render::Device* device = rs->GetDevice();
		uint32_t width = rs->GetRenderResolution().width;
		uint32_t height = rs->GetRenderResolution().height;

		{
			render::TextureDescription texDesc;
			texDesc.format = render::Format_R8G8B8A8_UNorm;
			texDesc.extent = { width, height, 1 };
			texDesc.isRenderTarget = true;
			texDesc.debugName = "PostProcess_LDR";
			render::TextureHandle texture = device->CreateTexture(texDesc);

			render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			m_hdrOutput = device->CreateRenderTarget(rtDesc);

			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.vsDesc.filePath = "shaders/quad.vert";
			shaderDesc.fsDesc.filePath = "shaders/hdr.frag";
			m_hdrShader = rs->CreateShader(shaderDesc);
		}
		
		m_bloomEffect.Init(rs);
	}

	void PostProcess::Destroy(rendersystem::RenderSystem* rs)
	{
		m_bloomEffect.Destroy(rs);

		rs->DestroyShader(&m_hdrShader);
		m_hdrOutput = nullptr;
	}

	void PostProcess::Draw(rendersystem::RenderSystem* rs)
	{
		const Lighting* lighting = (const Lighting*)GetRenderer()->GetRenderProcess(RENDERPROCESS_LIGHTING);
		render::RenderTargetHandle lightingRt = lighting->GetRenderTarget();

		// BLOOM
		{
			m_bloomEffect.m_composeTarget = lightingRt;
			m_bloomEffect.m_inputTarget = lightingRt->m_description.colorAttachments[0].texture;
			//m_bloomEffect.m_blendTexture = m_hdrOutput->m_description.colorAttachments[0].texture; //temp, TODO: need a default texture for dummy slot.
			m_bloomEffect.Draw(rs);
		}

		// HDR
		{
			CPU_PROFILE_SCOPE(CpuHDR);
			rs->BeginMarker("HDR");
			// HDR and tone mapping
			struct
			{
				float gamma;
				float exposure;
			} params{ CVar_GammaCorrection.Get(), CVar_Exposure.Get() };
			render::RenderTargetHandle rt = rs->GetLDRTarget();

			rs->SetShader(m_hdrShader);
			rs->SetRenderTarget(rt);
			rs->ClearColor();
			rs->SetDepthEnable(false, false);
			rs->SetShaderProperty("u_HdrParams", &params, sizeof(params));
			rs->SetTextureSlot("u_hdrtex", lightingRt->m_description.colorAttachments[0].texture);
			rs->DrawFullscreenQuad();
			rs->SetDefaultGraphicsState();
			rs->EndMarker();
		}
		rs->ClearState();
	}

	void PostProcess::ImGuiDraw()
	{
		m_bloomEffect.ImGuiDraw();
	}

	void PostProcess::DebugDraw()
	{
	}
}