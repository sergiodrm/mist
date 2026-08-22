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
#include "Lighting.h"

namespace Mist
{
	glm::mat4 GetSpotLightProjection(float cutOff, float nearClip, float farClip)
	{
		//return glm::perspective(2.f * glm::radians(cutOff), 1.f, nearClip, farClip);
		return math::PerspectiveProj(2.f * glm::radians(cutOff), 1.f, nearClip, farClip);
	}

	struct FitOrthoTransform
	{
		float minX;
		float maxX;
		float minY;
		float maxY;
		float minZ;
		float maxZ;
		glm::mat4 view;

		void Compute(const glm::mat4& cameraProj, const glm::mat4& cameraView, const glm::mat4& lightView, float zMult)
		{
			glm::mat4 inv = glm::inverse(cameraProj * cameraView);
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
			DebugRender::DrawSphere(center, 5.f, { 1,0,0 });

			view = glm::lookAt(center, center - math::GetForwardFromTransform(lightView), math::GetUpFromTransform(lightView));

			minX = FLT_MAX;
			maxX = -FLT_MAX;
			minY = FLT_MAX;
			maxY = -FLT_MAX;
			minZ = FLT_MAX;
			maxZ = -FLT_MAX;
			for (uint32_t i = 0; i < 8; ++i)
			{
				glm::vec4 trf = view * frustumCorners[i];
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
		}
	};

	static ShadowMapTransform ComputeSlice(const glm::mat4& cameraProj, const glm::mat4& cameraView, const tAngles& lightRot, float zMult)
	{
		//glm::mat4 inv = glm::inverse(cameraProj * cameraView);
		//glm::vec4 frustumCorners[8];
		//uint32_t index = 0;
		//glm::vec3 center = glm::vec3(0.f);
		//for (unsigned int x = 0; x < 2; ++x)
		//{
		//	for (unsigned int y = 0; y < 2; ++y)
		//	{
		//		for (unsigned int z = 0; z < 2; ++z)
		//		{
		//			const glm::vec4 pt =
		//				inv * glm::vec4(
		//					2.0f * x - 1.0f, // -1, 1
		//					2.0f * y - 1.0f, // -1, 1
		//					z, //  0, 1
		//					1.0f);
		//			frustumCorners[index] = pt / pt.w;
		//			center += frustumCorners[index++];
		//		}
		//	}
		//}
		//center /= 8.f;
		//DebugRender::DrawSphere(center, 5.f, { 1,0,0 });

		//const glm::mat4 lightView = glm::lookAt(center, center - lightRot.Forward(), lightRot.Up());

		//float minX = FLT_MAX;
		//float maxX = -FLT_MAX;
		//float minY = FLT_MAX;
		//float maxY = -FLT_MAX;
		//float minZ = FLT_MAX;
		//float maxZ = -FLT_MAX;
		//for (uint32_t i = 0; i < 8; ++i)
		//{
		//	glm::vec4 trf = lightView * frustumCorners[i];
		//	minX = __min(minX, trf.x);
		//	maxX = __max(maxX, trf.x);
		//	minY = __min(minY, trf.y);
		//	maxY = __max(maxY, trf.y);
		//	minZ = __min(minZ, trf.z);
		//	maxZ = __max(maxZ, trf.z);
		//}

		//if (minZ < 0)
		//	minZ *= zMult;
		//else
		//	minZ /= zMult;
		//if (maxZ < 0)
		//	maxZ /= zMult;
		//else
		//	maxZ *= zMult;

		FitOrthoTransform orthoTransform;
		//orthoTransform.Compute(cameraProj, cameraView, lightRot, zMult);
		const glm::mat4 lightProjection = math::OrthoProj(orthoTransform.minX, orthoTransform.maxX, orthoTransform.minY, orthoTransform.maxY, orthoTransform.minZ, orthoTransform.maxZ);

		ShadowMapTransform transform;
		transform.proj = lightProjection;
		transform.view = orthoTransform.view;
		return transform;
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
		: RenderProcess(renderer, engine), m_shadowMapResolution{1024, 1024}, m_shader(nullptr)
	{
		m_camera.GetCamera().SetFarClip(10.f);
		m_camera.GetCamera().SetProjectionType(ECameraProjectionType::Perspective);
		m_shadowMapTargetArray.Resize(m_shadowMapTargetArray.GetCapacity());
	}

	void ShadowMapProcess::Init(rendersystem::RenderSystem* rs)
	{
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; i++)
			m_renderListIds[i] = SceneRenderer::GetSceneRenderer()->CreateRenderList({ RenderPass_ShadowMap, {} });

		InitShadowMaps(rs, m_shadowMapResolution);

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

			glm::mat4 lightViewProj = GetShadowMapTransform(i).GetVP();
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
		int res[] = { (int)m_shadowMapResolution.width, (int)m_shadowMapResolution.height };
		if (ImGui::DragInt2("Shadow map resolution", res))
		{
			m_shadowMapResolution.width = limits_cast<uint32_t>(res[0]);
			m_shadowMapResolution.height = limits_cast<uint32_t>(res[1]);
		}
		if (ImGui::Button("Reload"))
		{
			InitShadowMaps(GetRenderSystem(), m_shadowMapResolution);
		}
		ImGui::End();
	}

	render::RenderTarget* ShadowMapProcess::GetRenderTarget(uint32_t index) const
	{
		check(index < globals::MaxShadowMapAttachments);
		return m_shadowMapTargetArray[index].GetPtr();
	}

	uint32_t ShadowMapProcess::SetupDirectionalLight(const Camera& cameraReference, const glm::mat4& lightView, float nearClip, float farClip, float zmult)
	{
		const Camera* ref = m_test ? &m_camera.GetCamera() : &cameraReference;
		float sliceNearClip = nearClip;
		float sliceFarClip = farClip;
		if (sliceNearClip >= sliceFarClip)
		{
			logferror("Invalid config of clips in directional light shadow map. Near clip must be less than far clip ( near: %f | far: %f )\n", nearClip, farClip);
			return UINT32_MAX;
		}

		glm::mat4 fitCameraProj = math::PerspectiveProj(math::ToRadians(ref->GetFOV()), ref->GetAspectRatio(), sliceNearClip, sliceFarClip);
		FitOrthoTransform orthoTransform;
		orthoTransform.Compute(fitCameraProj, ref->GetView(), lightView, zmult);

		return SetupDirectionalLight(orthoTransform.view, orthoTransform.minX, orthoTransform.maxX, orthoTransform.maxY, orthoTransform.minY, orthoTransform.minZ, orthoTransform.maxZ);
	}

	uint32_t ShadowMapProcess::SetupDirectionalLight(const glm::mat4& lightView, float left, float right, float top, float bottom, float nearClip, float farClip)
	{
		return SetupShadowMap(lightView, math::OrthoProj(left, right, bottom, top, nearClip, farClip));
	}

	uint32_t ShadowMapProcess::SetupSpotLight(const glm::mat4& lightView, float cutoff, float nearClip, float farClip)
	{
		return SetupShadowMap(lightView, GetSpotLightProjection(cutoff, nearClip, farClip));
	}

	uint32_t ShadowMapProcess::SetupShadowMap(const glm::mat4& view, const glm::mat4& proj, const glm::vec3* debugColor)
	{
		uint32_t lightIndex = UINT32_MAX;
		if (m_lightCount < globals::MaxShadowMapAttachments)
		{
			lightIndex = m_lightCount++;
			ShadowMapTransform& shadowTransform = m_shadowTransfoms[lightIndex];
			shadowTransform.view = view;
			shadowTransform.proj = proj;

			CameraData lightAsCameraData;
			lightAsCameraData.Set(shadowTransform.view, shadowTransform.proj);
			SceneRenderer::GetSceneRenderer()->SetRenderListInfo(m_renderListIds[lightIndex], { RenderPass_ShadowMap, lightAsCameraData });

			if (debugColor)
			{
				Frustum f(shadowTransform.GetVP());
				f.DrawDebug(*debugColor);
			}
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

	void ShadowMapProcess::InitShadowMaps(rendersystem::RenderSystem* rs, const render::Extent2D& resolution)
	{
		// invalidate
		for (uint32_t i = 0; i < m_shadowMapTargetArray.GetSize(); ++i)
		{
			m_shadowMapTargetArray[i] = nullptr;
			m_shadowMapTargetArray[i] = CreateDepthRenderTarget(rs, resolution);
		}
	}

	render::RenderTargetHandle ShadowMapProcess::CreateDepthRenderTarget(rendersystem::RenderSystem* rs, const render::Extent2D& extent)
	{
		render::TextureDescription texDesc;
		texDesc.extent = { .width = extent.width, .height = extent.height, .depth = 1 };
		texDesc.format = render::Format_D32_SFloat;
		texDesc.isRenderTarget = true;
		render::TextureHandle depthTex = rs->GetDevice()->CreateTexture(texDesc);

		render::RenderTargetDescription rtDesc;
		rtDesc.SetDepthStencilAttachment(depthTex);
		render::RenderTargetHandle rt = rs->GetDevice()->CreateRenderTarget(rtDesc);
		checkdbg(rt);
		return rt;
	}
}