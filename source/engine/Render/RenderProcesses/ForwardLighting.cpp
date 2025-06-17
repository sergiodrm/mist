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
#include "../Material.h"
#include "RenderSystem/RenderSystem.h"

#define MIST_DISABLE_FORWARD

namespace Mist
{
	CBoolVar CVar_ShowForwardTech("r_showforwardtech", false);

	render::RenderTargetHandle g_forwardRt = nullptr;

	ForwardLighting::ForwardLighting()
		: m_shader(nullptr), m_rt(nullptr)
	{
	}

	void ForwardLighting::Init(const RenderContext& renderContext)
	{
#ifdef MIST_DISABLE_FORWARD
		return;
#endif

		uint32_t width = 1920;
		uint32_t height = 1080;
		// Create render target
		{
			render::TextureDescription texDesc;
			texDesc.format = render::Format_R16G16B16A16_UNorm;
			texDesc.extent = { width, height, 1 };
			texDesc.isRenderTarget = true;
			texDesc.debugName = "ForwardLighting_HDR";
			render::TextureHandle texture = g_device->CreateTexture(texDesc);

			render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			m_rt = g_device->CreateRenderTarget(rtDesc);
			g_forwardRt = m_rt;
		}

		// Create shader program
		{
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.vsDesc.filePath = "shaders/forward_lighting.vert";
			shaderDesc.fsDesc.filePath = "shaders/forward_lighting.frag";
			shaderDesc.fsDesc.options.PushMacroDefinition("MAX_SHADOW_MAPS", 3);
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
			m_shader = _new rendersystem::ShaderProgram(g_device, shaderDesc);
		}

		{
            rendersystem::ShaderBuildDescription shaderDesc;
            shaderDesc.vsDesc.filePath = "shaders/skybox.vert";
            shaderDesc.fsDesc.filePath = "shaders/skybox.frag";
            m_shader = _new rendersystem::ShaderProgram(g_device, shaderDesc);

			m_skyboxModel = _new cModel();
			m_skyboxModel->LoadModel(renderContext, ASSET_PATH("models/cube.gltf"));
		}
	}

	void ForwardLighting::Destroy(const RenderContext& renderContext)
	{
#ifdef MIST_DISABLE_FORWARD
        return;
#endif
		m_skyboxModel->Destroy(renderContext);
		delete m_skyboxModel;
		m_skyboxModel = nullptr;

		m_rt = nullptr;
		delete m_shader;
		delete m_skyboxShader;
	}

	void ForwardLighting::InitFrameData(const RenderContext& context, const Renderer& renderFrame, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
#ifdef MIST_DISABLE_FORWARD
        return;
#endif
	}

	void ForwardLighting::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& renderFrameContext)
	{
#ifdef MIST_DISABLE_FORWARD
        return;
#endif
	}

	void ForwardLighting::Draw(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext)
	{
#ifdef MIST_DISABLE_FORWARD
        return;
#endif
		CPU_PROFILE_SCOPE(ForwardLighting);

		render::TextureHandle cubemapTexture = renderFrameContext.Scene->GetSkyboxTexture();

		if (CVar_ShowForwardTech.Get())
		{
			g_render->SetDefaultState();
			g_render->SetShader(m_shader);
			g_render->SetRenderTarget(m_rt);
			renderFrameContext.Scene->RenderPipelineDraw(renderContext, RenderFlags_Fixed, 3);

			g_render->SetShader(m_skyboxShader);
			check(m_skyboxModel->m_meshes.GetSize() == 1);
			const cMesh& mesh = m_skyboxModel->m_meshes[0];
			g_render->SetVertexBuffer(mesh.vb);
			g_render->SetIndexBuffer(mesh.ib);
			glm::mat4 viewRot = GetCameraData()->InvView;
			viewRot[3] = { 0.f,0.f,0.f,1.f };
			glm::mat4 ubo[2];
			ubo[0] = viewRot;
			ubo[1] = GetCameraData()->Projection * viewRot;
			g_render->SetShaderProperty("u_ubo", ubo, sizeof(glm::mat4) * 2);
			g_render->SetTextureSlot("u_cubemap", cubemapTexture);
			g_render->DrawIndexed(mesh.indexCount);
			
			DebugRender::DrawScreenQuad({}, { m_rt->m_info.extent.width, m_rt->m_info.extent.height }, m_rt->m_description.colorAttachments[0].texture);;
		}
	}

	void ForwardLighting::ImGuiDraw()
	{
#ifdef MIST_DISABLE_FORWARD
        return;
#endif
		ImGui::Begin("Forward tech");
		ImGuiUtils::CheckboxCBoolVar(CVar_ShowForwardTech);
		ImGui::End();
	}

	render::RenderTarget* ForwardLighting::GetRenderTarget(uint32_t index) const
	{
#ifdef MIST_DISABLE_FORWARD
        return nullptr;
#endif
		return m_rt.GetPtr();
	}
}