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
#include "../CommandList.h"

namespace Mist
{
	CBoolVar CVar_ShowForwardTech("r_showforwardtech", true);

	RenderTarget* g_forwardRt = nullptr;

	ForwardLighting::ForwardLighting()
		: m_shader(nullptr)
	{
	}

	void ForwardLighting::Init(const RenderContext& renderContext)
	{
		// Create render target
		{
			RenderTargetDescription desc;
			desc.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
			// Color attachment
			desc.AddColorAttachment(FORMAT_R16G16B16A16_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 0.f, 0.f, 0.f, 1.f });
			// Emissive attachment
			//desc.AddColorAttachment(FORMAT_R16G16B16A16_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 0.f, 0.f, 0.f, 1.f });
			desc.SetDepthAttachment(FORMAT_D24_UNORM_S8_UINT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 1.f, 0.f });
			desc.ResourceName = "RT_ForwardLighting";
			m_rt.Create(renderContext, desc);
			g_forwardRt = &m_rt;
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
			materialDynDesc.ElemCount = globals::MaxMaterials;
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

		m_computeBloom = _new ComputeBloom(&renderContext);
		m_computeBloom->Init();
	}

	void ForwardLighting::Destroy(const RenderContext& renderContext)
	{
		m_computeBloom->Destroy();
		delete m_computeBloom;
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
        CommandList* commandList = renderContext.CmdList;

		const cTexture* cubemapTexture = renderFrameContext.Scene->GetSkyboxTexture();

		if (CVar_ShowForwardTech.Get())
		{
            GraphicsState state = {};
			state.Program = m_shader;
			state.Rt = &m_rt;
			commandList->BeginMarker("ForwardTech");
			commandList->SetGraphicsState(state);
#if 0
			tViewRenderInfo renderInfo;
			// In this pass we render color lighting and 
			renderInfo.flags = RenderFlags_Fixed | RenderFlags_Emissive;
			renderInfo.view = *renderFrameContext.CameraData;
			renderInfo.cubemap = cubemapTexture;
			renderInfo.cubemapSlot = 6;
			ShadowMapProcess* shadowMapping = (ShadowMapProcess*)renderContext.Renderer->GetRenderProcess(RENDERPROCESS_SHADOWMAP);
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
			{
				// Shadow map matrices from shadow map process
				renderInfo.shadowMap.LightViewMatrices[i] = shadowMapping->m_shadowMapPipeline.GetLightVP(i);
				// Shadow map textures from shadow map process
				renderInfo.shadowMapTextures[i] = shadowMapping->GetRenderTarget(i)->GetDepthTexture();
			}
			renderInfo.shadowMapTexturesSlot = 2;
			renderInfo.environment = renderFrameContext.Scene->GetEnvironmentData();
			renderFrameContext.Scene->DrawWithMaterials(renderContext, renderInfo, 3);
#elif 1
			renderFrameContext.Scene->RenderPipelineDraw(renderContext, RenderFlags_Fixed, 3);
#else
			m_shader->UseProgram(renderContext);
			m_shader->SetBufferData(renderContext, "u_Camera", renderFrameContext.CameraData, sizeof(*renderFrameContext.CameraData));
			m_shader->SetBufferData(renderContext, "u_depthInfo", lightViewMatrices, sizeof(glm::mat4) * globals::MaxShadowMapAttachments);
			m_shader->SetBufferData(renderContext, "u_env", &renderFrameContext.Scene->GetEnvironmentData(), sizeof(EnvironmentData));
			m_shader->BindTextureArraySlot(renderContext, 2, shadowMapTextures.data(), globals::MaxShadowMapAttachments);
			m_shader->BindTextureSlot(renderContext, 6, *cubemapTexture);
			renderFrameContext.Scene->Draw(renderContext, m_shader, 3, 1, VK_NULL_HANDLE, RenderFlags_Fixed);
#endif // 0

			state.Program = m_skyboxShader;
			check(m_skyboxModel->m_meshes.GetSize() == 1);
			const cMesh& mesh = m_skyboxModel->m_meshes[0];
			state.Vbo = mesh.VertexBuffer;
            state.Ibo = mesh.IndexBuffer;
			commandList->SetGraphicsState(state);
			glm::mat4 viewRot = renderFrameContext.CameraData->InvView;
			viewRot[3] = { 0.f,0.f,0.f,1.f };
			glm::mat4 ubo[2];
			ubo[0] = viewRot;
			ubo[1] = renderFrameContext.CameraData->Projection * viewRot;
			m_skyboxShader->SetBufferData(renderContext, "u_ubo", ubo, sizeof(glm::mat4) * 2);
			m_skyboxShader->BindSampledTexture(renderContext, "u_cubemap", *cubemapTexture);
			commandList->BindProgramDescriptorSets();
            commandList->DrawIndexed(mesh.IndexCount, 1, 0, 0);
			
			commandList->EndMarker();

			DebugRender::DrawScreenQuad({}, { m_rt.GetWidth(), m_rt.GetHeight() }, *m_rt.GetTexture());

            // Bloom
			m_bloomInputConfig.Input = m_rt.GetTexture();
			m_computeBloom->Compute(m_bloomInputConfig);
		}
	}

	void ForwardLighting::ImGuiDraw()
	{
		ImGui::Begin("Forward tech");
		ImGuiUtils::CheckboxCBoolVar(CVar_ShowForwardTech);
        ImGui::DragFloat("Bloom threshold", &m_bloomInputConfig.Threshold, 0.01f, 0.f, 10.f);
        ImGui::DragFloat("Bloom knee", &m_bloomInputConfig.Knee, 0.01f, 0.f, 10.f);
		ImGui::End();
	}

	const RenderTarget* ForwardLighting::GetRenderTarget(uint32_t index) const
	{
		return &m_rt;
	}
}