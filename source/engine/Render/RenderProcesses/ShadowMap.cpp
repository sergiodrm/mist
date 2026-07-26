#include "ShadowMap.h"
#include <vector>
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "GBuffer.h"
#include "Scene/Scene.h"
#include "Scene/SceneRenderer.h"
#include "Render/DebugRender.h"
#include "imgui_internal.h"
#include "Utils/GenericUtils.h"
#include "Application/Application.h"
#include "Render/Camera.h"
#include "glm/ext/quaternion_geometric.hpp"
#include "RenderSystem/RenderSystem.h"
#include "Utils/Angles.h"

namespace Mist
{
	CIntVar CVar_ShadowMapResolutionWidth("r_shadowMapResolutionWidth", 1024);
	CIntVar CVar_ShadowMapResolutionHeight("r_shadowMapResolutionHeight", 1024);

	glm::mat4 GetSpotLightProjection(float cutOff, float nearClip, float farClip)
	{
		//return glm::perspective(2.f * glm::radians(cutOff), 1.f, nearClip, farClip);
		return math::PerspectiveProj(2.f * glm::radians(cutOff), 1.f, nearClip, farClip);
	}

	static void GetFitOrthoViewProj(glm::mat4& proj, glm::mat4& view, const Frustum& frustum, const tAngles& lightRot, float zMult)
	{
		glm::mat4 inv = glm::inverse(proj * view);
		glm::vec4 frustumCorners[8];
		uint32_t index = 0;
		glm::vec3 center = glm::vec3(0.f);
		for (unsigned int x = 0; x < 2; ++x)
		{
			for (unsigned int y = 0; y < 2; ++y)
			{
				for (unsigned int z = 0; z < 2; ++z)
				{
					const glm::vec4 pt =
						inv * glm::vec4(
							2.0f * x - 1.0f, // -1, 1
							2.0f * y - 1.0f, // -1, 1
							z, //  0, 1
							1.0f);
					frustumCorners[index] = pt / pt.w;
					center += frustumCorners[index++];
				}
			}
		}
		center /= 8.f;

		const glm::mat4 lightView = glm::lookAt(center, center - lightRot.Forward(), lightRot.Up());

		float minX = FLT_MAX;
		float maxX = -FLT_MAX;
		float minY = FLT_MAX;
		float maxY = -FLT_MAX;
		float minZ = FLT_MAX;
		float maxZ = -FLT_MAX;
		for (uint32_t i = 0; i < 8; ++i)
		{
			glm::vec4 trf = lightView * frustumCorners[i];
			minX = __min(minX, trf.x);
			maxX = __max(maxX, trf.x);
			minY = __min(minY, trf.y);
			maxY = __max(maxY, trf.y);
			minZ = __min(minZ, trf.z);
			maxZ = __max(maxZ, trf.z);
		}

		if (minZ < 0)
			minZ *= zMult;
		else
			minZ /= zMult;
		if (maxZ < 0)
			maxZ /= zMult;
		else
			maxZ *= zMult;

		auto lmb = [](float& _min, float& _max)
			{
				float diff = (_max - _min);
				_max = diff - 0.5f * diff;
				_min = -0.5f * diff;
			};

		//lmb(minX, maxX);
		//lmb(minY, maxY);
		//lmb(minZ, maxZ);

		//logferror("%f %f %f %f %f %f\n", minX, maxX, minY, maxY, minZ, maxZ);
		const glm::mat4 lightProjection = math::OrthoProj(minX, maxX, minY, maxY, minZ, maxZ);
		view = lightView;
		proj = lightProjection;
	}

	static glm::mat4 GetFitOrthoViewProj(const Frustum& frustum, const tAngles& lightRot, float zMult)
	{
		glm::mat4 proj, view;
		GetFitOrthoViewProj(proj, view, frustum, lightRot, zMult);
		return proj * view;
	}

	//glm::mat4 ShadowMapPipeline::ComputeShadowVolume(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& lightDir, float nearClip, float farClip, float splitLambda)
	//{
	//	float ratio = farClip / nearClip;
	//	float range = farClip - nearClip;

	//	float splits[SHADOW_MAP_CASCADE_SPLITS];
	//	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_SPLITS; ++i)
	//	{
	//		float p = (i + 1) / (float)SHADOW_MAP_CASCADE_SPLITS;
	//		float l = nearClip * glm::pow(ratio, p);
	//		float u = nearClip + p * range;
	//		float d = splitLambda * (l - u) + u;
	//		splits[i] = (d - nearClip) / range;
	//	}
	//	float lastSplit = 0.f;
	//	glm::mat4 viewProj(1.f);
	//	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_SPLITS; ++i)
	//	{
	//		glm::vec3 frustumCorners[8] =
	//		{
	//			glm::vec3(-1.0f,  1.0f, 0.0f),
	//			glm::vec3(1.0f,  1.0f, 0.0f),
	//			glm::vec3(1.0f, -1.0f, 0.0f),
	//			glm::vec3(-1.0f, -1.0f, 0.0f),
	//			glm::vec3(-1.0f,  1.0f,  1.0f),
	//			glm::vec3(1.0f,  1.0f,  1.0f),
	//			glm::vec3(1.0f, -1.0f,  1.0f),
	//			glm::vec3(-1.0f, -1.0f,  1.0f),
	//		};

	//		// Project frustum corners to world space
	//		const glm::mat4 matToWorld = glm::inverse(projection * view);
	//		for (uint32_t j = 0; j < 8; ++j)
	//		{
	//			glm::vec4 corner = matToWorld * glm::vec4(frustumCorners[j], 1.f);
	//			frustumCorners[j] = corner / corner.w;
	//		}
	//		for (uint32_t j = 0; j < 4; ++j)
	//		{
	//			glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
	//			frustumCorners[j + 4] = dist * splits[i] + frustumCorners[j];
	//			frustumCorners[j] = dist * lastSplit + frustumCorners[j];
	//		}
	//		// Frustum center
	//		glm::vec3 center(0.f);
	//		for (uint32_t j = 0; j < 8; ++j)
	//			center += frustumCorners[j];
	//		center /= 8.f;
	//		float radius = 0.f;
	//		for (uint32_t j = 0; j < 8; ++j)
	//		{
	//			float dist = glm::length(frustumCorners[j] - center);
	//			radius = __max(radius, dist);
	//		}
	//		radius = ceilf(radius * 16.f) / 16.f;

	//		glm::vec3 maxExtents = glm::vec3(radius);
	//		glm::vec3 minExtents = -maxExtents;
	//		glm::mat4 lightView = glm::lookAt(center - lightDir * -minExtents.z, center, glm::vec3(0.f, 1.f, 0.f));
	//		glm::mat4 lightProj = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.f, maxExtents.z - minExtents.z);

	//		viewProj = lightProj * lightView;

	//		lastSplit = splits[i];
	//	}

	//	return viewProj;
	//}

	ShadowMapProcess::ShadowMapProcess(Renderer* renderer, IRenderEngine* engine)
		: RenderProcess(renderer, engine)
	{
		m_camera.GetCamera().SetFarClip(10.f);
		m_zmult = 1.f;
		m_camera.GetCamera().SetProjectionType(ECameraProjectionType::Perspective);
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

		rendersystem::ShaderBuildDescription shaderDesc;
		shaderDesc.SetGraphics("shaders/render_pipeline/depth.vert", nullptr);
		m_shader = rs->CreateShader(shaderDesc);
	}

	void ShadowMapProcess::Destroy(rendersystem::RenderSystem* rs)
	{
		for (uint32_t j = 0; j < globals::MaxShadowMapAttachments; ++j)
			m_shadowMapTargetArray[j] = nullptr;
		rs->DestroyShader(&m_shader);
	}

	void ShadowMapProcess::Update()
	{
	}

	void ShadowMapProcess::Draw(rendersystem::RenderSystem* rs)
	{
		PROF_SCOPE(CpuShadowMapping);
		Scene* scene = GetEngine()->GetScene();
		if (!scene)
			return;

		check(m_lightCount <= globals::MaxShadowMapAttachments);
		rs->SetShader(m_shader);
		for (uint32_t i = 0; i < m_lightCount; ++i)
		{
			char buff[64];
			sprintf_s(buff, "Shadow Map %d", i);
			rs->BeginMarker(buff);

			rs->SetRenderTarget(m_shadowMapTargetArray[i]);
			rs->SetViewport(m_shadowMapTargetArray[i]->m_info.GetViewport());
			rs->SetScissor(m_shadowMapTargetArray[i]->m_info.GetScissor());
			rs->ClearDepthStencil();
			rs->SetDepthEnable();
			//rs->SetCullMode(render::RasterCullMode_Front);

			glm::mat4 lightViewProj = GetShadowMapData(i).GetVP();
			rs->SetShaderProperty("u_ubo", &lightViewProj, sizeof(glm::mat4));

			RenderContext rc;
			rc.rs = rs;
			rc.passId = m_renderListIds[i];
			SceneRenderer::GetSceneRenderer()->DrawList(rc);

			rs->EndMarker();
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
		ImGui::DragFloat("zmult", &m_zmult);
		ImGui::DragFloat("far clip", &m_directionalClipFar);
		static bool followCamera = false;
		ImGui::Checkbox("Follow camera", &followCamera);
		ImGui::Checkbox("Test", &m_test);
		if (followCamera || ImGui::Button("Copy camera from scene"))
		{
			Scene* scene = GetEngine()->GetScene();
			m_camera = scene->GetCamera();
		}
		m_camera.ImGuiDraw(false);
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
		auto lmb = [](const char* title, const Frustum& f)
			{
				ImGui::Text("%s", title);
				ImGui::Text("Center: %f %f %f", f.GetCenter().x, f.GetCenter().y, f.GetCenter().z);
				ImGui::Text("Left bottom near: %f %f %f", f.GetPoint(0).x, f.GetPoint(0).y, f.GetPoint(0).z);
				ImGui::Text("Left top near: %f %f %f", f.GetPoint(1).x, f.GetPoint(1).y, f.GetPoint(1).z);
				ImGui::Text("Right bottom near: %f %f %f", f.GetPoint(2).x, f.GetPoint(2).y, f.GetPoint(2).z);
				ImGui::Text("Right top near: %f %f %f", f.GetPoint(3).x, f.GetPoint(3).y, f.GetPoint(3).z);
				ImGui::Text("Left bottom far: %f %f %f", f.GetPoint(4).x, f.GetPoint(4).y, f.GetPoint(4).z);
				ImGui::Text("Left top far: %f %f %f", f.GetPoint(5).x, f.GetPoint(5).y, f.GetPoint(5).z);
				ImGui::Text("Right bottom far: %f %f %f", f.GetPoint(6).x, f.GetPoint(6).y, f.GetPoint(6).z);
				ImGui::Text("Right top far: %f %f %f", f.GetPoint(7).x, f.GetPoint(7).y, f.GetPoint(7).z);
			};
		lmb("Camera", m_cameraFrustum);
		lmb("Light", m_lightFrustum);
		
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
			ShadowMap& shadowMap = m_shadowMaps[lightIndex];

			Scene* scene = GetEngine()->GetScene();
			const Camera& camera = m_camera.GetCamera();
			//const Camera& camera = scene->GetCamera().GetCamera();
			if (m_test)
			{

				glm::mat4 fitCameraProj = math::PerspectiveProj(math::ToRadians(camera.GetFOV()), camera.GetAspectRatio(), camera.GetNearClip(), __min(camera.GetFarClip(), m_directionalClipFar));
				glm::mat4 cameraView = camera.GetView();

				shadowMap.proj = fitCameraProj;
				shadowMap.view = cameraView;

				m_cameraFrustum = fitCameraProj * cameraView;
				//GetFitOrthoViewProj(shadowMap.proj, shadowMap.view, f, -rot.Forward(), m_zmult);
				GetFitOrthoViewProj(shadowMap.proj, shadowMap.view, m_cameraFrustum, rot, m_zmult);
				//GetFitOrthoViewProj(shadowMap.proj, shadowMap.view, fitCameraProj, cameraView, rot, m_zmult);
				m_cameraFrustum.DrawDebug();
				m_lightFrustum = shadowMap.GetVP();
				m_lightFrustum.DrawDebug({ 0,1,0 });
			}
			else
			{
				shadowMap.view = glm::lookAt(camera.GetPosition(), camera.GetPosition() - rot.Forward(), rot.Up());
				shadowMap.proj = math::OrthoProj(left, right, bottom, top, nearClip, farClip);
				Frustum f(shadowMap.proj * shadowMap.view);
				f.DrawDebug({0,1,1});
			}


			CameraData lightAsCameraData;
			lightAsCameraData.Set(shadowMap.view, shadowMap.proj);
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
			ShadowMap& shadowMap = m_shadowMaps[lightIndex];

			shadowMap.view = glm::inverse(math::ToMat4(pos, rot, { 1.f, 1.f,  1.f }));
			shadowMap.proj = GetSpotLightProjection(cutoff, nearClip, farClip);

			Frustum f(shadowMap.proj * shadowMap.view);
			f.DrawDebug();

			CameraData lightAsCameraData;
			lightAsCameraData.Set(shadowMap.view, shadowMap.proj);
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