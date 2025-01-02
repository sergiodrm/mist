#include "ForwardLighting.h"
#include "Render/VulkanRenderEngine.h"
#include "Core/Debug.h"
#include "Application/Application.h"
#include "Render/InitVulkanTypes.h"
#include "Render/RenderContext.h"
#include "ShadowMap.h"
#include "Core/Logger.h"
#include "Render/DebugRender.h"
#include <imgui/imgui.h>
#include "Utils/GenericUtils.h"

namespace Mist
{
	CBoolVar CVar_ShowForwardTech("r_showforwardtech", false);

	ForwardLighting::ForwardLighting()
		: m_shader(nullptr)
	{ }

	void ForwardLighting::Init(const RenderContext& renderContext)
	{
		// Create render target
		{
			RenderTargetDescription desc;
			desc.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
			desc.AddColorAttachment(FORMAT_R16G16B16A16_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 0.f, 0.f, 0.f, 1.f });
			desc.SetDepthAttachment(FORMAT_D24_UNORM_S8_UINT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 1.f, 0.f });
			desc.ResourceName = "RT_ForwardLighting";
			m_rt.Create(renderContext, desc);
		}

		// Create shader program
		{
			tShaderProgramDescription desc;
			desc.VertexShaderFile.Filepath = SHADER_FILEPATH("forward_lighting.vert");
			desc.FragmentShaderFile.Filepath = SHADER_FILEPATH("forward_lighting.frag");
			desc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back({ "MAX_SHADOW_MAPS", "3" });
			desc.RenderTarget = &m_rt;
			desc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
			desc.DepthStencilMode = DEPTH_STENCIL_DEPTH_WRITE | DEPTH_STENCIL_DEPTH_TEST | DEPTH_STENCIL_STENCIL_TEST;
			desc.FrontStencil.CompareMask = 0x1;
			desc.FrontStencil.Reference = 0x1;
			desc.FrontStencil.WriteMask = 0x1;
			desc.FrontStencil.CompareOp = COMPARE_OP_ALWAYS;
			desc.FrontStencil.FailOp = STENCIL_OP_REPLACE;
			desc.FrontStencil.PassOp = STENCIL_OP_REPLACE;
			desc.FrontStencil.DepthFailOp = STENCIL_OP_REPLACE;
			tShaderDynamicBufferDescription dynBufferDesc;
			dynBufferDesc.Name = "u_model";
			dynBufferDesc.ElemCount = globals::MaxRenderObjects;
			tShaderDynamicBufferDescription materialDynDesc;
			materialDynDesc.Name = "u_material";
			materialDynDesc.ElemCount = globals::MaxRenderObjects;
			materialDynDesc.IsShared = true;
			desc.DynamicBuffers.push_back(dynBufferDesc);
			desc.DynamicBuffers.push_back(materialDynDesc);
			m_shader = ShaderProgram::Create(renderContext, desc);
		}

		{
			// Skybox
			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("skybox.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("skybox.frag");
			//shaderDesc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back({ "SKYBOX_GBUFFER" });
			shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
			shaderDesc.RenderTarget = &m_rt;
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
	}

	void ForwardLighting::Destroy(const RenderContext& renderContext)
	{
		m_skyboxModel->Destroy(renderContext);
		delete m_skyboxModel;
		m_skyboxModel = nullptr;
		m_rt.Destroy(renderContext);
	}

	void ForwardLighting::InitFrameData(const RenderContext& context, const Renderer& renderFrame, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
	}

	void ForwardLighting::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& renderFrameContext)
	{
	}

	void ForwardLighting::Draw(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext)
	{
		CPU_PROFILE_SCOPE(ForwardLighting);
		CommandBuffer cmd = renderFrameContext.GraphicsCommandContext.CommandBuffer;

		// Shadow map matrices
		ShadowMapProcess* shadowMapping = (ShadowMapProcess*)renderContext.Renderer->GetRenderProcess(RENDERPROCESS_SHADOWMAP);
		const glm::mat4* lightViewMatrices = &shadowMapping->m_shadowMapPipeline.GetLightVP(0);
		// Shadow map textures
		tArray<const cTexture*, globals::MaxShadowMapAttachments> shadowMapTextures;
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
			shadowMapTextures[i] = shadowMapping->GetRenderTarget(i)->GetDepthTexture();

		const cTexture* cubemapTexture = renderFrameContext.Scene->GetSkyboxTexture();

		BeginGPUEvent(renderContext, cmd, "ForwardTech");
		m_rt.BeginPass(renderContext, cmd);
		m_shader->UseProgram(renderContext);
		m_shader->SetBufferData(renderContext, "u_Camera", renderFrameContext.CameraData, sizeof(*renderFrameContext.CameraData));
		m_shader->SetBufferData(renderContext, "u_depthInfo", lightViewMatrices, sizeof(glm::mat4) * globals::MaxShadowMapAttachments);
		m_shader->SetBufferData(renderContext, "u_env", &renderFrameContext.Scene->GetEnvironmentData(), sizeof(EnvironmentData));
		m_shader->BindTextureArraySlot(renderContext, 2, shadowMapTextures.data(), globals::MaxShadowMapAttachments);
		m_shader->BindTextureSlot(renderContext, 6, *cubemapTexture);
		renderFrameContext.Scene->Draw(renderContext, m_shader, 3, 1, VK_NULL_HANDLE, RenderFlags_Fixed);

		m_skyboxShader->UseProgram(renderContext);
		glm::mat4 viewRot = renderFrameContext.CameraData->InvView;
		viewRot[3] = { 0.f,0.f,0.f,1.f };
		glm::mat4 ubo[2];
		ubo[0] = viewRot;
		ubo[1] = renderFrameContext.CameraData->Projection * viewRot;
		m_skyboxShader->SetBufferData(renderContext, "u_ubo", ubo, sizeof(glm::mat4) * 2);
		m_skyboxShader->BindTextureSlot(renderContext, 1, *cubemapTexture);
		m_skyboxShader->FlushDescriptors(renderContext);
		check(m_skyboxModel->m_meshes.GetSize() == 1);
		const cMesh& mesh = m_skyboxModel->m_meshes[0];
		mesh.BindBuffers(cmd);
		RenderAPI::CmdDrawIndexed(cmd, mesh.IndexCount, 1, 0, 0, 0);

		m_rt.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);

		if (CVar_ShowForwardTech.Get())
		{
			float width = (float)renderContext.Window->Width;
			float height = (float)renderContext.Window->Height;
			DebugRender::DrawScreenQuad({ 0.f, 0.f }, { width, height }, *m_rt.GetTexture());
		}
	}

	void ForwardLighting::ImGuiDraw()
	{
		ImGui::Begin("Forward tech");
		ImGuiUtils::CheckboxCBoolVar(CVar_ShowForwardTech);
		ImGui::End();
	}
	
	const RenderTarget* ForwardLighting::GetRenderTarget(uint32_t index) const
	{
		return &m_rt;
	}
}