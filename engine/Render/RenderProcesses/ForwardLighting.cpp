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

	RenderTarget* g_forwardRt = nullptr;;

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
		//VkCommandBuffer cmd = renderFrameContext.GraphicsCommandContext.CommandBuffer;
        CommandList* commandList = renderContext.CmdList;

		const cTexture* cubemapTexture = renderFrameContext.Scene->GetSkyboxTexture();

		if (CVar_ShowForwardTech.Get())
		{
            GraphicsState state = {};
			state.Program = m_shader;
			state.Rt = &m_rt;
			//BeginGPUEvent(renderContext, cmd, "ForwardTech");
			commandList->BeginMarker("ForwardTech");
			//m_rt.BeginPass(renderContext, cmd);
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
			//m_skyboxShader->UseProgram(renderContext);
			glm::mat4 viewRot = renderFrameContext.CameraData->InvView;
			viewRot[3] = { 0.f,0.f,0.f,1.f };
			glm::mat4 ubo[2];
			ubo[0] = viewRot;
			ubo[1] = renderFrameContext.CameraData->Projection * viewRot;
			m_skyboxShader->SetBufferData(renderContext, "u_ubo", ubo, sizeof(glm::mat4) * 2);
			m_skyboxShader->BindSampledTexture(renderContext, "u_cubemap", *cubemapTexture);
			//m_skyboxShader->FlushDescriptors(renderContext);
			//RenderAPI::CmdDrawIndexed(cmd, mesh.IndexCount, 1, 0, 0, 0);
			commandList->BindProgramDescriptorSets();
            commandList->DrawIndexed(mesh.IndexCount, 1, 0, 0);

			//m_rt.EndPass(cmd);
			//EndGPUEvent(renderContext, cmd);
			commandList->EndMarker();
		}
#if 0
		cTexture* lightingTex = const_cast<cTexture*>(m_rt.GetTexture());
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.pNext = nullptr;
		barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.srcQueueFamilyIndex = renderContext.GraphicsQueueFamily;
		barrier.dstQueueFamilyIndex = renderContext.ComputeQueueFamily;
		barrier.image = lightingTex->GetNativeImage();
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
			0, nullptr, 0, nullptr, 1, &barrier);
		lightingTex->SetImageLayout(IMAGE_LAYOUT_GENERAL);
#endif // 0


        //if (CVar_ShowForwardTech.Get())
        //{
        //	float width = (float)renderContext.Window->Width;
        //	float height = (float)renderContext.Window->Height;
        //	DebugRender::DrawScreenQuad({ 0.f, 0.f }, { width, height }, *m_rt.GetTexture());
        //}

#if 0
		ComputeBloom::InputConfig bloomConfig;
		bloomConfig.Input = m_rt.GetTexture();
		bloomConfig.Threshold = 1.f;
		m_computeBloom.Compute(renderContext, bloomConfig);
#endif // 0

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