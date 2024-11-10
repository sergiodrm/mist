#include "GBuffer.h"
#include "Render/VulkanBuffer.h"
#include "Render/RenderContext.h"
#include "Render/VulkanRenderEngine.h"
#include "Scene/Scene.h"
#include <imgui/imgui.h>
#include "Application/CmdParser.h"
#include "Application/Application.h"




namespace Mist
{
	void GBuffer::Init(const RenderContext& renderContext)
	{
		tClearValue clearValue{ .color = {0.2f, 0.2f, 0.2f, 0.f} };
		RenderTargetDescription description;
		description.AddColorAttachment(GBUFFER_RT_FORMAT_POSITION, GBUFFER_RT_LAYOUT_POSITION, SAMPLE_COUNT_1_BIT, clearValue);
		description.AddColorAttachment(GBUFFER_RT_FORMAT_NORMAL, GBUFFER_RT_LAYOUT_NORMAL, SAMPLE_COUNT_1_BIT, clearValue);
		description.AddColorAttachment(GBUFFER_RT_FORMAT_ALBEDO, GBUFFER_RT_LAYOUT_ALBEDO, SAMPLE_COUNT_1_BIT, clearValue);
		description.AddColorAttachment(GBUFFER_RT_FORMAT_EMISSIVE, GBUFFER_RT_LAYOUT_EMISSIVE, SAMPLE_COUNT_1_BIT, {0.f, 0.f, 0.f, 1.f});
		description.DepthAttachmentDescription.Format = GBUFFER_RT_FORMAT_DEPTH;
		description.DepthAttachmentDescription.Layout = GBUFFER_RT_LAYOUT_DEPTH;
		description.DepthAttachmentDescription.MultisampledBit = SAMPLE_COUNT_1_BIT;
		clearValue.depthStencil.depth = 1.f;
		description.DepthAttachmentDescription.ClearValue = clearValue;
		description.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		description.RenderArea.offset = { .x = 0, .y = 0 };
		description.ResourceName = "Gbuffer_RT";
		m_renderTarget.Create(renderContext, description);
		InitPipeline(renderContext);
	}

	void GBuffer::Destroy(const RenderContext& renderContext)
	{
		m_renderTarget.Destroy(renderContext);
	}

	void GBuffer::InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
	}

	void GBuffer::UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext)
	{
	}

	void GBuffer::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		VkCommandBuffer cmd = frameContext.GraphicsCommandContext.CommandBuffer;

		// MRT
		BeginGPUEvent(renderContext, cmd, "GBuffer_MRT", 0xffff00ff);
		m_renderTarget.BeginPass(renderContext, cmd);
		m_shader->UseProgram(renderContext);
		m_shader->SetBufferData(renderContext, "u_camera", frameContext.CameraData, sizeof(*frameContext.CameraData));

		frameContext.Scene->Draw(renderContext, m_shader, 2, 1, VK_NULL_HANDLE, RenderFlags_Fixed | RenderFlags_Emissive);
		m_renderTarget.EndPass(frameContext.GraphicsCommandContext.CommandBuffer);
		EndGPUEvent(renderContext, cmd);
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
		return &m_renderTarget;
	}

	void GBuffer::DebugDraw(const RenderContext& context)
	{
		float w = 1920.f;
		float h = 1080.f;
		float factor = 0.5f;
		glm::vec2 pos = { w * factor, 0.f };
		glm::vec2 size = glm::vec2{ w, h } *factor;
		TextureBindingDescriptor tex;
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
			for (uint32_t i = RT_POSITION; i < RT_COUNT-1; ++i)
			{
				tex.View = GetRenderTarget()->GetRenderTarget(i);
				tex.Layout = GetRenderTarget()->GetDescription().ColorAttachmentDescriptions[i].Layout;
				tex.Sampler = nullptr;
				DebugRender::DrawScreenQuad(pos, size, tex);
				pos.y += ydiff;
			}
			tex.View = GetRenderTarget()->GetDepthAttachment().View;
			tex.Layout = GetRenderTarget()->GetDescription().DepthAttachmentDescription.Layout;
			tex.Sampler = nullptr;
			DebugRender::DrawScreenQuad(pos, size, tex);
			pos.y += ydiff;
		}
			break;
		case DEBUG_POSITION:
		{
			tex.View = GetRenderTarget()->GetRenderTarget(RT_POSITION);
			tex.Layout = GBUFFER_RT_LAYOUT_POSITION;
			tex.Sampler = nullptr;
			DebugRender::DrawScreenQuad(pos, size, tex);
		}
			break;
		case DEBUG_NORMAL:
		{
			tex.View = GetRenderTarget()->GetRenderTarget(RT_NORMAL);
			tex.Layout = GBUFFER_RT_LAYOUT_NORMAL;
			tex.Sampler = nullptr;
			DebugRender::DrawScreenQuad(pos, size, tex);
		}
			break;
		case DEBUG_ALBEDO:
		{
			tex.View = GetRenderTarget()->GetRenderTarget(RT_ALBEDO);
			tex.Layout = GBUFFER_RT_LAYOUT_ALBEDO;
			tex.Sampler = nullptr;
			DebugRender::DrawScreenQuad(pos, size, tex);
		}
			break;
		case DEBUG_DEPTH:
		{
			tex.View = GetRenderTarget()->GetRenderTarget(RT_DEPTH);
			tex.Layout = GBUFFER_RT_LAYOUT_DEPTH;
			tex.Sampler = nullptr;
			DebugRender::DrawScreenQuad(pos, size, tex);
		}
			break;
		case DEBUG_EMISSIVE:
		{
			tex.View = GetRenderTarget()->GetRenderTarget(RT_EMISSIVE);
			tex.Layout = GBUFFER_RT_LAYOUT_ALBEDO;
			tex.Sampler = nullptr;
			DebugRender::DrawScreenQuad(pos, size, tex);
		}
			break;
		default:
			break;

		}
	}

	void GBuffer::InitPipeline(const RenderContext& renderContext)
	{
		// MRT pipeline
		tShaderProgramDescription shaderDesc;

		tShaderDynamicBufferDescription modelDynDesc;
		modelDynDesc.Name = "u_model";
		modelDynDesc.ElemCount = globals::MaxRenderObjects;
		modelDynDesc.IsShared = true;

		tShaderDynamicBufferDescription materialDynDesc = modelDynDesc;
		materialDynDesc.Name = "u_material";
		modelDynDesc.ElemCount = globals::MaxRenderObjects;
		materialDynDesc.IsShared = true;

		shaderDesc.DynamicBuffers.push_back(modelDynDesc);
		shaderDesc.DynamicBuffers.push_back(materialDynDesc);
		shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("mrt.vert");
		shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("mrt.frag");
		shaderDesc.RenderTarget = &m_renderTarget;
		shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
		m_shader = ShaderProgram::Create(renderContext, shaderDesc);
		m_shader->SetupDescriptors(renderContext);
	}
}