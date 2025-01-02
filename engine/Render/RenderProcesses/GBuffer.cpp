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
#if 0
		m_skyboxModel->Destroy(renderContext);
		delete m_skyboxModel;
		m_skyboxModel = nullptr;
#endif // 0
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
		VkCommandBuffer cmd = frameContext.GraphicsCommandContext.CommandBuffer;

		// MRT
		BeginGPUEvent(renderContext, cmd, "GBuffer_MRT", 0xffff00ff);
		m_renderTarget.BeginPass(renderContext, cmd);
		m_gbufferShader->UseProgram(renderContext);
		m_gbufferShader->SetBufferData(renderContext, "u_camera", frameContext.CameraData, sizeof(*frameContext.CameraData));

		m_renderTarget.ClearDepthStencil(cmd, 1.f, 0.f);
		//vkCmdSetStencilOp(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);
		//vkCmdSetStencilWriteMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0xffffffff);
		//vkCmdSetStencilCompareMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0x00000000);
		//vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0x00000000);
		//vkCmdSetStencilTestEnable(cmd, VK_TRUE);

		frameContext.Scene->Draw(renderContext, m_gbufferShader, 2, 1, VK_NULL_HANDLE, RenderFlags_Fixed | RenderFlags_Emissive);
#if 0

		m_skyboxShader->UseProgram(renderContext);
		const cTexture* texture = frameContext.Scene->GetSkyboxTexture();
		glm::mat4 viewRot = frameContext.CameraData->InvView;
		viewRot[3] = { 0.f,0.f,0.f,1.f };
		glm::mat4 ubo[2];
		ubo[0] = viewRot;
		ubo[1] = frameContext.CameraData->Projection * viewRot;
		m_skyboxShader->SetBufferData(renderContext, "u_ubo", ubo, sizeof(glm::mat4) * 2);
		m_skyboxShader->BindTextureSlot(renderContext, 1, *texture);
		m_skyboxShader->FlushDescriptors(renderContext);
		check(m_skyboxModel->m_meshes.GetSize() == 1);
		const cMesh& mesh = m_skyboxModel->m_meshes[0];
		mesh.BindBuffers(cmd);
		RenderAPI::CmdDrawIndexed(cmd, mesh.IndexCount, 1, 0, 0, 0);
#endif // 0


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
				DebugRender::DrawScreenQuad(pos, size, *m_renderTarget.GetTexture(i));
				pos.y += ydiff;
			}
		}
			break;
		case DEBUG_POSITION:
			DebugRender::DrawScreenQuad(pos, size, *m_renderTarget.GetTexture(RT_POSITION));
			break;
		case DEBUG_NORMAL:
			DebugRender::DrawScreenQuad(pos, size, *m_renderTarget.GetTexture(RT_NORMAL));
			break;
		case DEBUG_ALBEDO:
			DebugRender::DrawScreenQuad(pos, size, *m_renderTarget.GetTexture(RT_ALBEDO));
			break;
		case DEBUG_DEPTH:
			DebugRender::DrawScreenQuad(pos, size, *m_renderTarget.GetDepthTexture());
			break;
		case DEBUG_EMISSIVE:
			DebugRender::DrawScreenQuad(pos, size, *m_renderTarget.GetTexture(RT_EMISSIVE));
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
			modelDynDesc.ElemCount = globals::MaxRenderObjects;
			materialDynDesc.IsShared = true;

			shaderDesc.DynamicBuffers.push_back(modelDynDesc);
			shaderDesc.DynamicBuffers.push_back(materialDynDesc);
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("mrt.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("mrt.frag");
			shaderDesc.RenderTarget = &m_renderTarget;
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

			//shaderDesc.DynamicStates.reserve(4);
			//shaderDesc.DynamicStates.push_back(DYNAMIC_STATE_STENCIL_COMPARE_MASK);
			//shaderDesc.DynamicStates.push_back(DYNAMIC_STATE_STENCIL_REFERENCE);
			//shaderDesc.DynamicStates.push_back(DYNAMIC_STATE_STENCIL_WRITE_MASK);
			m_gbufferShader = ShaderProgram::Create(renderContext, shaderDesc);
		}

#if 0
		{
			// Skybox
			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("skybox.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("skybox.frag");
			shaderDesc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back({ "SKYBOX_GBUFFER" });
			shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
			shaderDesc.RenderTarget = &m_renderTarget;
			shaderDesc.CullMode = CULL_MODE_FRONT_BIT;
			shaderDesc.DepthStencilMode = DEPTH_STENCIL_NONE;
			shaderDesc.FrontFaceMode = FRONT_FACE_COUNTER_CLOCKWISE;

			shaderDesc.DepthStencilMode = /*DEPTH_STENCIL_DEPTH_WRITE | DEPTH_STENCIL_DEPTH_TEST |*/ DEPTH_STENCIL_STENCIL_TEST;
			shaderDesc.FrontStencil.CompareMask = 0x1;
			shaderDesc.FrontStencil.Reference = 0x1;
			shaderDesc.FrontStencil.WriteMask = 0x1;
			shaderDesc.FrontStencil.CompareOp = COMPARE_OP_NOT_EQUAL;
			shaderDesc.FrontStencil.FailOp = STENCIL_OP_KEEP;
			shaderDesc.FrontStencil.PassOp = STENCIL_OP_KEEP;
			shaderDesc.FrontStencil.DepthFailOp = STENCIL_OP_KEEP;
			shaderDesc.BackStencil = shaderDesc.FrontStencil;

			m_skyboxShader = ShaderProgram::Create(renderContext, shaderDesc);

			m_skyboxModel = _new cModel();
			m_skyboxModel->LoadModel(renderContext, ASSET_PATH("models/cube.gltf"));
		}
#endif // 0

	}
}