#include "ShadowMap.h"
#include <vector>
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "GBuffer.h"
#include "Scene/Scene.h"
#include "Render/DebugRender.h"
#include "imgui_internal.h"
#include "glm/ext/matrix_clip_space.inl"
#include "glm/matrix.hpp"
#include "Utils/GenericUtils.h"
#include "Application/Application.h"
#include "Render/Camera.h"
#include "glm/ext/quaternion_geometric.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "RenderSystem/RenderSystem.h"

#define SHADOW_MAP_RT_FORMAT FORMAT_D32_SFLOAT
#define SHADOW_MAP_RT_LAYOUT IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL

#define SHADOW_MAP_CASCADE_SPLITS 1

namespace Mist
{
	bool GUseCameraForShadowMapping = false;

	CIntVar CVar_ShadowMapResolutionWidth("r_shadowMapResolutionWidth", 1024);
	CIntVar CVar_ShadowMapResolutionHeight("r_shadowMapResolutionHeight", 1024);

	glm::mat4 GetSpotLightProjection(float cutOff, float nearClip, float farClip)
	{
		return glm::perspective(2.f * glm::radians(cutOff), 1.f, nearClip, farClip);
	}

	glm::mat4 GetDirectionalLightProjection(float left, float right, float bottom, float top, float nearClip, float farClip)
	{
		return glm::ortho(left, right, bottom, top, nearClip, farClip);
	}

	glm::mat4 GetLightVPMatrix(const glm::vec3& pos, const tAngles& angles, const glm::mat4& proj)
	{
		// Projection goes to Z*-1.f, rotate lightRot 180 yaw to make it match to light.
		glm::mat4 rot = angles.ToMat4();
		//lightRotMat = math::PitchYawRollToMat4({ 0.f, (float)M_PI, 0.f}) * lightRotMat;
		// Light translation
		glm::mat4 tras = math::PosToMat4(pos);

		glm::mat4 depthView = glm::inverse(tras * rot);
		glm::mat4 depthProj = proj;
		depthProj[1][1] *= -1.f;
		return depthProj * depthView;
	}

	glm::mat4 GetLightVPMatrix(const glm::vec3& pos, const tAngles& angles, float cutoff, float nearClip, float farClip)
	{
		return GetLightVPMatrix(pos, angles, GetSpotLightProjection(cutoff, nearClip, farClip));
	}

	glm::mat4 GetLightVPMatrix(const glm::vec3& pos, const tAngles& angles, float left, float right, float bottom, float top, float nearClip, float farClip)
	{
		return GetLightVPMatrix(pos, angles, GetDirectionalLightProjection(left, right, bottom, top, nearClip, farClip));
	}

	ShadowMapPipeline::ShadowMapPipeline()
		: m_shader(nullptr)
	{
		memset(m_depthMVPCache, 0, sizeof(m_depthMVPCache));
		memset(m_lightMVPCache, 0, sizeof(m_lightMVPCache));
	}

	ShadowMapPipeline::~ShadowMapPipeline()
	{
	}

	void ShadowMapPipeline::Init(rendersystem::RenderSystem* rs)
	{
		rendersystem::ShaderBuildDescription shaderDesc;
		shaderDesc.vsDesc.filePath = "shaders/depth.vert";
		m_shader = rs->CreateShader(shaderDesc);
	}

	void ShadowMapPipeline::Destroy(rendersystem::RenderSystem* rs)
	{	
		rs->DestroyShader(&m_shader);
	}

	void ShadowMapPipeline::SetupLight(uint32_t lightIndex, const glm::vec3& lightPos, const tAngles& lightRot, const glm::mat4& lightProj, const glm::mat4& viewMatrix)
	{
		static constexpr glm::mat4 depthBias =
		{
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f
		};
		glm::mat4 depthVP = GetLightVPMatrix(lightPos, lightRot, lightProj);

		glm::mat4 lightVP = depthBias * depthVP;
		SetDepthVP(lightIndex, depthVP);
		SetLightVP(lightIndex, lightVP);
	}
	
	void ShadowMapPipeline::SetupSpotLight(uint32_t lightIndex, const glm::mat4& cameraView, const glm::vec3& pos, const tAngles& rot, float cutoff, float nearClip, float farClip)
	{
		glm::mat4 depthProj = GetSpotLightProjection(cutoff, nearClip, farClip);
		SetupLight(lightIndex, pos, rot, depthProj, cameraView);
	}

	void ShadowMapPipeline::SetupDirectionalLight(uint32_t lightIndex, const glm::mat4& cameraView, const glm::mat4& cameraProj, const tAngles& lightRot, float left, float right, float bottom, float top, float nearClip, float farClip)
	{
#if 1
		const glm::mat4 depthProj = GetDirectionalLightProjection(left, right, bottom, top, nearClip, farClip);
		const glm::vec3 camerapos = glm::vec3(0.f);// math::GetPos(cameraView);
		SetupLight(lightIndex, camerapos, lightRot, depthProj, cameraView);
#else
		glm::mat4 lightRotMat = math::PitchYawRollToMat4(lightRot);
		glm::vec3 lightDir = -math::GetDir(lightRotMat);
		glm::mat4 viewProj = ComputeShadowVolume(cameraView, cameraProj, lightDir, 1.f, 10.f);
		SetupLight(lightIndex, viewProj, cameraView);
#endif // 0
	}

	const glm::mat4& ShadowMapPipeline::GetDepthVP(uint32_t index) const
	{
		check(index < globals::MaxShadowMapAttachments);
		return m_depthMVPCache[index];
	}

	const glm::mat4& ShadowMapPipeline::GetLightVP(uint32_t index) const
	{
		check(index < globals::MaxShadowMapAttachments);
		return m_lightMVPCache[index];
	}

	void ShadowMapPipeline::SetDepthVP(uint32_t index, const glm::mat4& mat)
	{
		check(index < globals::MaxShadowMapAttachments);
		m_depthMVPCache[index] = mat;
	}

	void ShadowMapPipeline::SetLightVP(uint32_t index, const glm::mat4& mat)
	{
		check(index < globals::MaxShadowMapAttachments);
		m_lightMVPCache[index] = mat;
	}

	uint32_t ShadowMapPipeline::GetBufferSize() const
	{
		return sizeof(glm::mat4) * globals::MaxShadowMapAttachments;
	}

	void ShadowMapPipeline::ImGuiDraw(bool createWindow)
	{
	}

	glm::mat4 ShadowMapPipeline::ComputeShadowVolume(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& lightDir, float nearClip, float farClip, float splitLambda)
	{
		float ratio = farClip / nearClip;
		float range = farClip - nearClip;

		float splits[SHADOW_MAP_CASCADE_SPLITS];
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_SPLITS; ++i)
		{
			float p = (i + 1) / (float)SHADOW_MAP_CASCADE_SPLITS;
			float l = nearClip * glm::pow(ratio, p);
			float u = nearClip + p * range;
			float d = splitLambda * (l - u) + u;
			splits[i] = (d - nearClip) / range;
		}
		float lastSplit = 0.f;
		glm::mat4 viewProj(1.f);
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_SPLITS; ++i)
		{
			glm::vec3 frustumCorners[8] =
			{
				glm::vec3(-1.0f,  1.0f, 0.0f),
				glm::vec3(1.0f,  1.0f, 0.0f),
				glm::vec3(1.0f, -1.0f, 0.0f),
				glm::vec3(-1.0f, -1.0f, 0.0f),
				glm::vec3(-1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f, -1.0f,  1.0f),
				glm::vec3(-1.0f, -1.0f,  1.0f),
			};

			// Project frustum corners to world space
			const glm::mat4 matToWorld = glm::inverse(projection * view);
			for (uint32_t j = 0; j < 8; ++j)
			{
				glm::vec4 corner = matToWorld * glm::vec4(frustumCorners[j], 1.f);
				frustumCorners[j] = corner / corner.w;
			}
			for (uint32_t j = 0; j < 4; ++j)
			{
				glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
				frustumCorners[j + 4] = dist * splits[i] + frustumCorners[j];
				frustumCorners[j] = dist * lastSplit + frustumCorners[j];
			}
			// Frustum center
			glm::vec3 center(0.f);
			for (uint32_t j = 0; j < 8; ++j)
				center += frustumCorners[j];
			center /= 8.f;
			float radius = 0.f;
			for (uint32_t j = 0; j < 8; ++j)
			{
				float dist = glm::length(frustumCorners[j] - center);
				radius = __max(radius, dist);
			}
			radius = ceilf(radius * 16.f) / 16.f;

			glm::vec3 maxExtents = glm::vec3(radius);
			glm::vec3 minExtents = -maxExtents;
			glm::mat4 lightView = glm::lookAt(center - lightDir * -minExtents.z, center, glm::vec3(0.f, 1.f, 0.f));
			glm::mat4 lightProj = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.f, maxExtents.z - minExtents.z);

			viewProj = lightProj * lightView;

			lastSplit = splits[i];
		}

		return viewProj;
	}

	void ShadowMapPipeline::SetupLight(uint32_t lightIndex, const glm::mat4& depthViewProj, const glm::mat4& view)
	{
		static constexpr glm::mat4 depthBias =
		{
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f
		};
		// Light Matrix with inverse(viewMatrix) because gbuffer calculates position buffer in view space.
		glm::mat4 lightVP = depthBias * depthViewProj * glm::inverse(view);
		SetDepthVP(lightIndex, depthViewProj);
		SetLightVP(lightIndex, lightVP);
	}

	ShadowMapProcess::ShadowMapProcess(Renderer* renderer, IRenderEngine* engine)
		: RenderProcess(renderer, engine)
	{
	}

	void ShadowMapProcess::Init(rendersystem::RenderSystem* rs)
	{
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; i++)
		{
			render::TextureDescription texDesc;
			texDesc.extent = { .width = (uint32_t)CVar_ShadowMapResolutionWidth.Get(), .height = (uint32_t)CVar_ShadowMapResolutionHeight.Get(), .depth = 1};
			texDesc.format = render::Format_D32_SFloat;
			texDesc.isRenderTarget = true;
			render::TextureHandle depthTex = rs->GetDevice()->CreateTexture(texDesc);

			render::RenderTargetDescription rtDesc;
			rtDesc.SetDepthStencilAttachment(depthTex);
			m_shadowMapTargetArray[i] = rs->GetDevice()->CreateRenderTarget(rtDesc);

			m_renderListIds[i] = SceneRenderer::GetSceneRenderer()->CreateRenderList({ RenderPass_ShadowMap, {} });
		}

		// Init shadow map pipeline when render target is created
		m_shadowMapPipeline.Init(rs);
	}

	void ShadowMapProcess::Destroy(rendersystem::RenderSystem* rs)
	{
		for (uint32_t j = 0; j < globals::MaxShadowMapAttachments; ++j)
			m_shadowMapTargetArray[j] = nullptr;
		m_shadowMapPipeline.Destroy(rs);
	}

	void ShadowMapProcess::Update()
	{
	}

	void ShadowMapProcess::Draw(rendersystem::RenderSystem* rs)
	{
		CPU_PROFILE_SCOPE(CpuShadowMapping);
		Scene* scene = GetEngine()->GetScene();
		if (!scene)
			return;

		check(m_lightCount <= globals::MaxShadowMapAttachments);
		rs->SetShader(m_shadowMapPipeline.GetShader());
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
		{
			rs->SetRenderTarget(m_shadowMapTargetArray[i]);
			rs->SetViewport(m_shadowMapTargetArray[i]->m_info.GetViewport());
			rs->SetScissor(m_shadowMapTargetArray[i]->m_info.GetScissor());
			rs->ClearDepthStencil();
			rs->SetDepthEnable();
			if (i < m_lightCount)
			{
				rs->SetShaderProperty("u_ubo", &m_shadowMapPipeline.GetDepthVP(i), sizeof(glm::mat4));
				RenderContext rc;
				rc.rs = rs;
				rc.passId = m_renderListIds[i];
				SceneRenderer::GetSceneRenderer()->DrawList(rc);
			}
		}
		rs->ClearState();
		rs->SetDefaultGraphicsState();
		m_lightCount = 0;
	}

	void ShadowMapProcess::ImGuiDraw()
	{
		ImGui::Begin("Shadow mapping");
		static const char* modes[] = { "None", "Single tex", "All" };
		static const uint32_t modesSize = CountOf(modes);
		if (ImGui::BeginCombo("Debug mode", modes[m_debugMode]))
		{
			for (uint32_t i = 0; i < modesSize; ++i)
			{
				if (ImGui::Selectable(modes[i], i == m_debugMode))
					m_debugMode = (EDebugMode)i;
			}
			ImGui::EndCombo();
		}
		if (m_debugMode == DEBUG_SINGLE_RT)
		{
			ImGui::InputInt("ShadowMap index", (int*)&m_textureDebugIndex);
			m_textureDebugIndex = math::Clamp(m_textureDebugIndex, 0u, globals::MaxShadowMapAttachments - 1);
		}
		ImGui::Checkbox("Use camera for shadow mapping", &GUseCameraForShadowMapping);
		ImGui::End();
	}

	render::RenderTarget* ShadowMapProcess::GetRenderTarget(uint32_t index) const
	{
		check(index < globals::MaxShadowMapAttachments);
		return m_shadowMapTargetArray[index].GetPtr();
	}

	uint32_t ShadowMapProcess::SetupDirectionalLight(const glm::vec3& pos, const tAngles& rot, float left, float right, float top, float bottom, float nearClip, float farClip)
	{
		uint32_t lightIndex = UINT32_MAX;
		if (m_lightCount < globals::MaxShadowMapAttachments)
		{
			lightIndex = m_lightCount++;
			glm::mat4 view = glm::inverse(math::ToMat4(pos, rot, { 1.f, 1.f,  1.f }));
			const CameraData* cameraData = GetCameraData();
			CameraData lightAsCameraData;
			lightAsCameraData.Set(view, GetDirectionalLightProjection(left, right, bottom, top, nearClip, farClip));
			m_shadowMapPipeline.SetupDirectionalLight(lightIndex, cameraData->View, cameraData->Projection, rot, left, right, bottom, top, nearClip, farClip);
			SceneRenderer::GetSceneRenderer()->SetRenderListInfo(m_renderListIds[lightIndex], { RenderPass_ShadowMap, lightAsCameraData });
		}
		return lightIndex;
	}

	uint32_t ShadowMapProcess::SetupSpotLight(const glm::vec3& pos, const tAngles& rot, float cutoff, float nearClip, float farClip)
	{
		uint32_t lightIndex = UINT32_MAX;
		if (m_lightCount < globals::MaxShadowMapAttachments)
		{
			lightIndex = m_lightCount++;
			glm::mat4 view = glm::inverse(math::ToMat4(pos, rot, { 1.f, 1.f,  1.f }));
			const CameraData* cameraData = GetCameraData();
			CameraData lightAsCameraData;
			lightAsCameraData.Set(view, GetSpotLightProjection(cutoff, nearClip, farClip));
			m_shadowMapPipeline.SetupSpotLight(lightIndex, cameraData->View, pos, rot, cutoff, nearClip, farClip);
			SceneRenderer::GetSceneRenderer()->SetRenderListInfo(m_renderListIds[lightIndex], { RenderPass_ShadowMap, lightAsCameraData });
		}
		return lightIndex;
	}

	void ShadowMapProcess::DebugDraw()
	{
		render::Extent2D extent = g_render->GetBackbufferResolution();
		float w = (float)extent.width;
		float h = (float)extent.height;
		switch (m_debugMode)
		{
		case DEBUG_NONE:
			break;
		case DEBUG_SINGLE_RT:
			{	
				float f = 0.33f;
				DebugRender::DrawScreenQuad({ w * (1.f-f), 0.f }, { w * f, h * f }, m_shadowMapTargetArray[m_textureDebugIndex]->m_description.depthStencilAttachment.texture);
			}
			break;
		case DEBUG_ALL:
			{
				glm::vec2 screenSize = { w, h };
				float factor = 1.f / (float)globals::MaxShadowMapAttachments;
				glm::vec2 pos = { screenSize.x * (1.f - factor), 0.f };
				glm::vec2 size = { screenSize.x * factor, screenSize.y * factor };
				for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				{
					DebugRender::DrawScreenQuad(pos, size, m_shadowMapTargetArray[i]->m_description.depthStencilAttachment.texture);
					pos.y += size.y;
				}
			}
			break;
		}
	}
}