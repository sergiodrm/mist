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




namespace Mist
{
	GBuffer* g_gbuffer = nullptr;

	void GBuffer::Init(const RenderContext& renderContext)
	{
		check(g_gbuffer == nullptr);
		g_gbuffer = this;

		tClearValue clearValue{ .color = {0.2f, 0.2f, 0.2f, 0.f} };
		RenderTargetDescription description;
		description.AddColorAttachment(GetGBufferFormat(RT_POSITION), IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValue);
		description.AddColorAttachment(GetGBufferFormat(RT_NORMAL), IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValue);
		description.AddColorAttachment(GetGBufferFormat(RT_ALBEDO), IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValue);
		description.SetDepthAttachment(GetGBufferFormat(RT_DEPTH_STENCIL), IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, {});
		description.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		description.RenderArea.offset = { .x = 0, .y = 0 };
		description.ResourceName = "Gbuffer_RT";
		m_renderTarget = RenderTarget::Create(renderContext, description);
		InitPipeline(renderContext);
	}

	void GBuffer::Destroy(const RenderContext& renderContext)
	{
		check(g_gbuffer == this);
		RenderTarget::Destroy(renderContext, m_renderTarget);

		g_gbuffer = nullptr;
	}

	void GBuffer::InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
	}

	void GBuffer::UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext)
	{
	}

	CIntVar CVar_GBufferDraw("r_gbufferdraw", 0);

	void GBuffer::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		CPU_PROFILE_SCOPE(CpuGBuffer);
        CommandList* commandList = renderContext.CmdList;

		// MRT
        commandList->BeginMarker("GBuffer_MRT");
		GraphicsState state{ .Rt = m_renderTarget };
		if (CVar_GBufferDraw.Get())
			state.Program = m_gbufferShader;
		commandList->SetGraphicsState(state);
		commandList->ClearDepthStencil();
		if (CVar_GBufferDraw.Get())
		{
			state.Program->SetBufferData(renderContext, "u_camera", frameContext.CameraData, sizeof(*frameContext.CameraData));
			frameContext.Scene->Draw(renderContext, state.Program, 2, 1, VK_NULL_HANDLE, RenderFlags_Fixed | RenderFlags_Emissive);
		}
		else
			frameContext.Scene->RenderPipelineDraw(renderContext, RenderFlags_Fixed);
		commandList->ClearState();
		commandList->EndMarker();
	}

	void GBuffer::ImGuiDraw()
	{
		ImGui::Begin("GBuffer");
		static const char* rts[] = { "None", "Position", "Normal", "Albedo", "Depth", "Emissive", "All"};
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

	const RenderTarget* GBuffer::GetRenderTarget(uint32_t index) const
	{
		return m_renderTarget;
	}

	const RenderTarget* GBuffer::GetGBuffer()
	{
		check(g_gbuffer);
		return g_gbuffer->GetRenderTarget();
	}

	void GBuffer::DebugDraw(const RenderContext& context)
	{
		float w = (float)context.Window->Width;
		float h = (float)context.Window->Height;
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
			for (uint32_t i = RT_POSITION; i < RT_COUNT; ++i)
			{
				DebugRender::DrawScreenQuad(pos, size, *m_renderTarget->GetTexture(i));
				pos.y += ydiff;
			}
		}
			break;
		case DEBUG_POSITION:
			DebugRender::DrawScreenQuad(pos, size, *m_renderTarget->GetTexture(RT_POSITION));
			break;
		case DEBUG_NORMAL:
			DebugRender::DrawScreenQuad(pos, size, *m_renderTarget->GetTexture(RT_NORMAL));
			break;
		case DEBUG_ALBEDO:
			DebugRender::DrawScreenQuad(pos, size, *m_renderTarget->GetTexture(RT_ALBEDO));
			break;
		case DEBUG_DEPTH:
			DebugRender::DrawScreenQuad(pos, size, *m_renderTarget->GetDepthTexture());
			break;
		default:
			break;
		}
	}

	void GBuffer::InitPipeline(const RenderContext& renderContext)
	{
		{
			// MRT pipeline
			tShaderProgramDescription shaderDesc;

			tShaderDynamicBufferDescription modelDynDesc;
			modelDynDesc.Name = "u_model";
			modelDynDesc.ElemCount = globals::MaxRenderObjects;
			modelDynDesc.IsShared = true;

			tShaderDynamicBufferDescription materialDynDesc = modelDynDesc;
			materialDynDesc.Name = "u_material";
			materialDynDesc.ElemCount = globals::MaxMaterials;
			materialDynDesc.IsShared = true;

			shaderDesc.DynamicBuffers.push_back(modelDynDesc);
			shaderDesc.DynamicBuffers.push_back(materialDynDesc);
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("mrt.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("mrt.frag");
#define DECLARE_MACRO_ENUM(_flag) shaderDesc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back({#_flag, _flag})
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

			shaderDesc.RenderTarget = m_renderTarget;
			shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
			shaderDesc.DepthStencilMode = DEPTH_STENCIL_DEPTH_WRITE | DEPTH_STENCIL_DEPTH_TEST | DEPTH_STENCIL_STENCIL_TEST;
			shaderDesc.FrontStencil.CompareMask = 0x1;
			shaderDesc.FrontStencil.Reference = 0x1;
			shaderDesc.FrontStencil.WriteMask = 0x1;
			shaderDesc.FrontStencil.CompareOp = COMPARE_OP_ALWAYS;
			shaderDesc.FrontStencil.FailOp = STENCIL_OP_REPLACE;
			shaderDesc.FrontStencil.PassOp = STENCIL_OP_REPLACE;
			shaderDesc.FrontStencil.DepthFailOp = STENCIL_OP_REPLACE;
			shaderDesc.BackStencil = shaderDesc.FrontStencil;
			m_gbufferShader = ShaderProgram::Create(renderContext, shaderDesc);
		}
	}

	EFormat GBuffer::GetGBufferFormat(EGBufferTarget target)
	{
		switch (target)
		{
		case RT_POSITION:
		case RT_NORMAL: return FORMAT_R32G32B32A32_SFLOAT;
		case RT_ALBEDO: return FORMAT_R8G8B8A8_UNORM;
		case RT_DEPTH_STENCIL: return FORMAT_D24_UNORM_S8_UINT;
		}
		return FORMAT_UNDEFINED;
	}
}