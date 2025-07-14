#include "GBuffer.h"
#include "Render/VulkanBuffer.h"
#include "Render/RenderContext.h"
#include "Render/VulkanRenderEngine.h"
#include "Render/DebugRender.h"
#include "Scene/Scene.h"
#include <imgui/imgui.h>
#include "Application/CmdParser.h"
#include "Application/Application.h"
#include "Render/Model.h"
#include "../CommandList.h"

#include "RenderSystem/RenderSystem.h"



namespace Mist
{
	GBuffer* g_gbuffer = nullptr;

	void GBuffer::Init(const RenderContext& renderContext)
	{
		check(g_gbuffer == nullptr);
		g_gbuffer = this;
		render::Extent2D extent = g_render->GetBackbufferResolution();
		uint32_t width = extent.width;
		uint32_t height = extent.height;

		render::TextureHandle textures[RT_COUNT];
		render::RenderTargetDescription rtDesc;
		for (uint32_t i = 0; i < RT_COUNT; ++i)
		{
			render::TextureDescription texDesc;
			texDesc.format = GetGBufferFormat((EGBufferTarget)i);
			check(texDesc.format != render::Format_Undefined);
			texDesc.extent = { width, height, 1 };
			texDesc.isRenderTarget = true;
			textures[i] = g_device->CreateTexture(texDesc);

			if (render::utils::IsDepthStencilFormat(texDesc.format))
				rtDesc.SetDepthStencilAttachment(textures[i]);
			else
				rtDesc.AddColorAttachment(textures[i]);
		}
		m_renderTarget = g_device->CreateRenderTarget(rtDesc);
		InitPipeline(renderContext);
	}

	void GBuffer::Destroy(const RenderContext& renderContext)
	{
		check(g_gbuffer == this);
		//RenderTarget::Destroy(renderContext, m_renderTarget);

		//check(m_renderTarget.GetRefCounter() == 1);
		m_renderTarget = nullptr;
		g_gbuffer = nullptr;
		g_render->DestroyShader(&m_gbufferShader);
	}

	void GBuffer::InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
	}

	void GBuffer::UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext)
	{
	}

	void GBuffer::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		CPU_PROFILE_SCOPE(CpuGBuffer);
		rendersystem::RenderSystem* renderSystem = g_render;
		renderSystem->SetRenderTarget(m_renderTarget);
		renderSystem->SetShader(m_gbufferShader);
		renderSystem->SetShaderProperty("u_camera", GetCameraData(), sizeof(CameraData));
		renderSystem->ClearColor();
		renderSystem->ClearDepthStencil();
		renderSystem->SetStencilEnable(true);
		renderSystem->SetStencilMask(0xff, 0xff, 1);
		renderSystem->SetStencilOpFrontAndBack(render::StencilOp_Keep, render::StencilOp_Keep, render::StencilOp_Replace);
		frameContext.Scene->Draw(renderSystem, RenderFlags_Fixed | RenderFlags_Emissive);
		renderSystem->ClearState();
		renderSystem->SetDefaultState();
	}

	void GBuffer::ImGuiDraw()
	{
		ImGui::Begin("GBuffer");
		static const char* rts[] = { "None", "Position", "Normal", "Albedo", "Emissive", "Depth", "All"};
		static int index = 0;
		if (ImGui::BeginCombo("Debug mode", rts[index]))
		{
			for (uint32_t i = 0; i < CountOf(rts); ++i)
			{
				if (ImGui::Selectable(rts[i], i == index))
				{
					index = i;
					m_debugMode = (EDebugMode)i;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::End();
	}

	render::RenderTarget* GBuffer::GetRenderTarget(uint32_t index) const
	{
		return m_renderTarget.GetPtr();
	}

	render::RenderTarget* GBuffer::GetGBuffer()
	{
		check(g_gbuffer);
		return g_gbuffer->GetRenderTarget();
	}

	void GBuffer::DebugDraw(const RenderContext& context)
	{
		render::Extent2D extent = g_render->GetBackbufferResolution();
		float w = (float)extent.width;
		float h = (float)extent.height;
		float factor = 0.5f;
		glm::vec2 pos = { w * factor, 0.f };
		glm::vec2 size = glm::vec2{ w, h } *factor;
		switch (m_debugMode)
		{
		case DEBUG_NONE:
			break;
		case DEBUG_ALL:
		{
			float x = w * 0.75f;
			float y = 0.f;
			float ydiff = h / (float)RT_COUNT;
			pos = { x, y };
			size = { w * 0.25f, ydiff };
			static_assert(RT_COUNT > 0);
			for (uint32_t i = RT_POSITION; i < RT_DEPTH_STENCIL; ++i)
			{
				DebugRender::DrawScreenQuad(pos, size, m_renderTarget->m_description.colorAttachments[i].texture);
				pos.y += ydiff;
			}
			DebugRender::DrawScreenQuad(pos, size, m_renderTarget->m_description.depthStencilAttachment.texture);
			pos.y += ydiff;
		}
			break;
		case DEBUG_DEPTH:
			DebugRender::DrawScreenQuad(pos, size, m_renderTarget->m_description.depthStencilAttachment.texture);
			break;
		default:
			DebugRender::DrawScreenQuad(pos, size, m_renderTarget->m_description.colorAttachments[m_debugMode].texture);
			break;
		}
	}

	void GBuffer::InitPipeline(const RenderContext& renderContext)
	{
		{
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.vsDesc.filePath = "shaders/mrt.vert";
			shaderDesc.fsDesc.filePath = "shaders/mrt.frag";
#define DECLARE_MACRO_ENUM(_flag) shaderDesc.fsDesc.options.PushMacroDefinition(#_flag, _flag)
            DECLARE_MACRO_ENUM(MATERIAL_FLAG_NONE);
            DECLARE_MACRO_ENUM(MATERIAL_FLAG_HAS_ALBEDO_MAP);
            DECLARE_MACRO_ENUM(MATERIAL_FLAG_HAS_NORMAL_MAP);
            DECLARE_MACRO_ENUM(MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP);
            DECLARE_MACRO_ENUM(MATERIAL_FLAG_HAS_SPECULAR_GLOSSINESS_MAP);
            DECLARE_MACRO_ENUM(MATERIAL_FLAG_HAS_EMISSIVE_MAP);
            DECLARE_MACRO_ENUM(MATERIAL_FLAG_EMISSIVE);
            DECLARE_MACRO_ENUM(MATERIAL_FLAG_UNLIT);
            DECLARE_MACRO_ENUM(MATERIAL_FLAG_NO_PROJECT_SHADOWS);
			DECLARE_MACRO_ENUM(MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS);

            DECLARE_MACRO_ENUM(MATERIAL_TEXTURE_ALBEDO);
            DECLARE_MACRO_ENUM(MATERIAL_TEXTURE_NORMAL);
            DECLARE_MACRO_ENUM(MATERIAL_TEXTURE_SPECULAR);
            DECLARE_MACRO_ENUM(MATERIAL_TEXTURE_OCCLUSION);
            DECLARE_MACRO_ENUM(MATERIAL_TEXTURE_METALLIC_ROUGHNESS);
			DECLARE_MACRO_ENUM(MATERIAL_TEXTURE_EMISSIVE);
#undef DECLARE_MACRO_ENUM
			m_gbufferShader = g_render->CreateShader(shaderDesc);
		}
	}

	render::Format GBuffer::GetGBufferFormat(EGBufferTarget target)
	{
		switch (target)
		{
		case RT_POSITION:
		case RT_NORMAL: return render::Format_R16G16B16A16_SFloat;
		case RT_ALBEDO: return render::Format_R8G8B8A8_UNorm;
		case RT_EMISSIVE: return render::Format_R16G16B16A16_SFloat;
		case RT_DEPTH_STENCIL: return render::Format_D24_UNorm_S8_UInt;
		}
		return render::Format_Undefined;
	}
}