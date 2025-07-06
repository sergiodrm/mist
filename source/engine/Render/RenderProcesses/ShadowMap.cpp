#include "ShadowMap.h"
#include <vector>
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "Render/InitVulkanTypes.h"
#include "GBuffer.h"
#include "Scene/Scene.h"
#include "Render/VulkanBuffer.h"
#include "Render/RenderDescriptor.h"
#include "Render/DebugRender.h"
#include "imgui_internal.h"
#include "glm/ext/matrix_clip_space.inl"
#include "glm/matrix.hpp"
#include "Utils/GenericUtils.h"
#include "Application/Application.h"
#include "Render/Camera.h"
#include "glm/ext/quaternion_geometric.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "../CommandList.h"
#include "RenderSystem/RenderSystem.h"

#define SHADOW_MAP_RT_FORMAT FORMAT_D32_SFLOAT
#define SHADOW_MAP_RT_LAYOUT IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL

#define SHADOW_MAP_CASCADE_SPLITS 1

namespace Mist
{
	bool GUseCameraForShadowMapping = false;

	ShadowMapPipeline::ShadowMapPipeline()
		: m_shader(nullptr)
	{
		SetProjection(glm::radians(45.f), 16.f / 9.f);
		SetProjection(-160.f, 160.f, -120.f, 120.f);
		SetPerspectiveClip(1.f, 1000.f);
		SetOrthographicClip(-500.f, 150.f);

		memset(m_depthMVPCache, 0, sizeof(m_depthMVPCache));
	}

	ShadowMapPipeline::~ShadowMapPipeline()
	{
	}

	void ShadowMapPipeline::Init(const RenderContext& renderContext, const RenderTarget* renderTarget)
	{
		rendersystem::ShaderBuildDescription shaderDesc;
		shaderDesc.vsDesc.filePath = "shaders/depth.vert";
		m_shader = g_render->CreateShader(shaderDesc);
	}

	void ShadowMapPipeline::Destroy(const RenderContext& renderContext)
	{	
		g_render->DestroyShader(&m_shader);
	}

	void ShadowMapPipeline::AddFrameData(const RenderContext& renderContext, UniformBufferMemoryPool* buffer)
	{
	}

	void ShadowMapPipeline::SetPerspectiveClip(float nearClip, float farClip)
	{
		m_perspectiveParams[2] = nearClip;
		m_perspectiveParams[3] = farClip;
		//DebugRender::SetDebugClipParams(nearClip, farClip);
	}

	void ShadowMapPipeline::SetOrthographicClip(float nearClip, float farClip)
	{
		m_orthoParams[4] = nearClip;
		m_orthoParams[5] = farClip;
		//DebugRender::SetDebugClipParams(nearClip, farClip);
	}

	glm::mat4 ShadowMapPipeline::GetProjection(EShadowMapProjectionType projType) const
	{
		switch (projType)
		{
		case PROJECTION_PERSPECTIVE:
			return glm::perspective(m_perspectiveParams[0], m_perspectiveParams[1], m_perspectiveParams[2], m_perspectiveParams[3]);
		case PROJECTION_ORTHOGRAPHIC:
			return glm::ortho(m_orthoParams[0], m_orthoParams[1], m_orthoParams[2], m_orthoParams[3], m_orthoParams[4], m_orthoParams[5]);
		}
		return glm::mat4(1.f);
	}

	void ShadowMapPipeline::SetProjection(float fov, float aspectRatio)
	{
		m_perspectiveParams[0] = fov;
		m_perspectiveParams[1] = aspectRatio;
	}

	void ShadowMapPipeline::SetProjection(float minX, float maxX, float minY, float maxY)
	{
		m_orthoParams[0] = minX;
		m_orthoParams[1] = maxX;
		m_orthoParams[2] = minY;
		m_orthoParams[3] = maxY;
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

		// Projection goes to Z*-1.f, rotate lightRot 180 yaw to make it match to light.
		glm::mat4 lightRotMat = lightRot.ToMat4();
		//lightRotMat = math::PitchYawRollToMat4({ 0.f, (float)M_PI, 0.f}) * lightRotMat;
		// Light translation
		glm::mat4 t = math::PosToMat4(lightPos);

		glm::mat4 depthView = glm::inverse(t * lightRotMat);
		glm::mat4 depthProj = lightProj;
		depthProj[1][1] *= -1.f;
		glm::mat4 depthVP = depthProj * depthView;
		// Light Matrix with inverse(viewMatrix) because gbuffer calculates position buffer in view space.
		glm::mat4 lightVP = depthBias * depthVP * glm::inverse(viewMatrix);
		SetDepthVP(lightIndex, depthVP);
		SetLightVP(lightIndex, lightVP);
	}
	
	void ShadowMapPipeline::SetupSpotLight(uint32_t lightIndex, const glm::mat4& cameraView, const glm::vec3& pos, const tAngles& rot, float cutoff, float nearClip, float farClip)
	{
		glm::mat4 depthProj = glm::perspective(2.f*glm::radians(cutoff), 1.f, nearClip, farClip);
		SetupLight(lightIndex, pos, rot, depthProj, cameraView);
	}

	void ShadowMapPipeline::SetupDirectionalLight(uint32_t lightIndex, const glm::mat4& cameraView, const glm::mat4& cameraProj, const tAngles& lightRot, float nearClip, float farClip)
	{
#if 1
		const glm::mat4 depthProj = GetProjection(PROJECTION_ORTHOGRAPHIC);
		const glm::vec3 camerapos = glm::vec3(0.f);// math::GetPos(cameraView);
		SetupLight(lightIndex, camerapos, lightRot, depthProj, cameraView);
#else
		glm::mat4 lightRotMat = math::PitchYawRollToMat4(lightRot);
		glm::vec3 lightDir = -math::GetDir(lightRotMat);
		glm::mat4 viewProj = ComputeShadowVolume(cameraView, cameraProj, lightDir, 1.f, 10.f);
		SetupLight(lightIndex, viewProj, cameraView);
#endif // 0

	}

	void ShadowMapPipeline::FlushToUniformBuffer(const RenderContext& renderContext, UniformBufferMemoryPool* buffer)
	{
		//m_shader->SetDynamicBufferData(renderContext, "u_ubo", m_depthMVPCache, sizeof(glm::mat4), globals::MaxShadowMapAttachments);
	}

	void ShadowMapPipeline::RenderShadowMap(const RenderContext& context, const Scene* scene, uint32_t lightIndex)
	{
		check(lightIndex < globals::MaxShadowMapAttachments);
		uint32_t depthVPOffset = sizeof(glm::mat4) * lightIndex; 
		g_render->SetShaderProperty("u_ubo", &m_depthMVPCache[lightIndex], sizeof(glm::mat4));
		scene->Draw(context, nullptr, 0, 0, 0, RenderFlags_ShadowMap | RenderFlags_NoTextures);
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

	ShadowMapProcess::ShadowMapProcess()
	{
		m_debugLightParams.clips[0] = 1.f;
		m_debugLightParams.clips[1] = 1000.f;
	}

	void ShadowMapProcess::Init(const RenderContext& context)
	{
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; i++)
		{
			render::TextureDescription texDesc;
			texDesc.extent = { .width = context.Window->Width, .height = context.Window->Height, .depth = 1 };
			texDesc.format = render::Format_D32_SFloat;
			texDesc.isRenderTarget = true;
			render::TextureHandle depthTex = g_device->CreateTexture(texDesc);

			render::RenderTargetDescription rtDesc;
			rtDesc.SetDepthStencilAttachment(depthTex);
			m_shadowMapTargetArray[i] = g_device->CreateRenderTarget(rtDesc);
		}

		// Init shadow map pipeline when render target is created
		m_shadowMapPipeline.Init(context, nullptr);
	}

	void ShadowMapProcess::InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
	}

	void ShadowMapProcess::Destroy(const RenderContext& renderContext)
	{
		for (uint32_t j = 0; j < globals::MaxShadowMapAttachments; ++j)
			m_shadowMapTargetArray[j] = nullptr;
		m_shadowMapPipeline.Destroy(renderContext);
	}

	void ShadowMapProcess::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& renderFrameContext)
	{
		Scene& scene = *renderFrameContext.Scene;
		
		// Update shadow map matrix
		if (GUseCameraForShadowMapping)
		{
			m_shadowMapPipeline.SetDepthVP(0, GetCameraData()->ViewProjection);
		}
		else
		{
			float shadowMapIndex = 0.f;
			glm::mat4 invView = GetCameraData()->InvView;
			glm::mat4 cameraProj = GetCameraData()->Projection;

			// TODO: cache on scene a preprocessed light array to show. Dont iterate over ALL objects checking if they have light component.
			uint32_t count = scene.GetRenderObjectCount();
			m_lightCount = 0;
			for (uint32_t i = 0; i < count; ++i)
			{
				const LightComponent* light = scene.GetLight(i);
				if (light && light->Type != ELightType::Point && light->ProjectShadows)
				{
					const TransformComponent& t = scene.GetTransform(i);
					switch (light->Type)
					{
					case ELightType::Directional:
						if (m_debugDirParams.show)
						{
							m_debugDirParams.pos = glm::vec3(0.f);
							m_debugDirParams.rot = t.Rotation;
							for (uint32_t j = 0; j< CountOf(m_debugDirParams.clips); ++j)
								m_debugDirParams.clips[j] = m_shadowMapPipeline.m_orthoParams[j];
						}
						m_shadowMapPipeline.SetupDirectionalLight(m_lightCount++, invView, cameraProj, t.Rotation);
						break;
					case ELightType::Spot:
					{
						if (m_debugLightParams.show)
						{
							m_debugLightParams.pos = t.Position;
							m_debugLightParams.rot = t.Rotation;
							m_debugLightParams.cutoff = light->OuterCutoff;
						}
						m_shadowMapPipeline.SetupSpotLight(m_lightCount++, invView, t.Position, t.Rotation, light->OuterCutoff, m_debugLightParams.clips[0], m_debugLightParams.clips[1]);
					}
						break;
					default:
						check(false && "Unreachable");
					}
				}
			}
			check(m_lightCount <= globals::MaxShadowMapAttachments);
		}

		m_shadowMapPipeline.FlushToUniformBuffer(renderContext, renderFrameContext.GlobalBuffer);

#if 0
		// Update light VP matrix for lighting pass
		static constexpr glm::mat4 depthBias =
		{
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f
		};
		glm::mat4 lightMatrix[globals::MaxShadowMapAttachments];
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
		{
			//lightMatrix[i] = renderFrameContext.CameraData->View * depthBias * m_shadowMapPipeline.GetDepthVP(i);
			lightMatrix[i] = renderFrameContext.CameraData->View * depthBias * m_shadowMapPipeline.GetDepthVP(i);
		}
		renderFrameContext.GlobalBuffer->SetUniform(renderContext, UNIFORM_ID_LIGHT_VP, lightMatrix, sizeof(glm::mat4) * globals::MaxShadowMapAttachments);
#endif // 0

	}

	void ShadowMapProcess::Draw(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext)
	{
		CPU_PROFILE_SCOPE(CpuShadowMapping);

		check(m_lightCount <= globals::MaxShadowMapAttachments);
		g_render->SetShader(m_shadowMapPipeline.GetShader());
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
		{
			g_render->SetRenderTarget(m_shadowMapTargetArray[i]);
			if (i < m_lightCount)
				m_shadowMapPipeline.RenderShadowMap(renderContext, renderFrameContext.Scene, i);
		}
		g_render->ClearState();
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
			ImGui::InputInt("ShadowMap index", (int*)&m_debugIndex);
			m_debugIndex = math::Clamp(m_debugIndex, 0u, globals::MaxShadowMapAttachments - 1);
		}
		ImGui::Checkbox("Use camera for shadow mapping", &GUseCameraForShadowMapping);
		ImGui::Checkbox("Debug spot frustum", &m_debugLightParams.show);
		ImGui::Checkbox("Debug dir frustum", &m_debugDirParams.show);
		ImGui::Separator();
		ImGui::Text("Debug proj params");
		ImGui::DragFloat("Near clip", &m_debugLightParams.clips[0], 1.f);
		ImGui::DragFloat("Far clip", &m_debugLightParams.clips[1], 1.f);
		ImGui::DragFloat("FOV", &m_shadowMapPipeline.m_perspectiveParams[0], 0.01f);
		ImGui::DragFloat("Aspect ratio", &m_shadowMapPipeline.m_perspectiveParams[1], 0.01f);
		ImGui::DragFloat2("Ortho x", &m_shadowMapPipeline.m_orthoParams[0]);
		ImGui::DragFloat2("Ortho y", &m_shadowMapPipeline.m_orthoParams[2]);
		ImGui::DragFloat("Ortho Near clip", &m_shadowMapPipeline.m_orthoParams[4], 1.f);
		ImGui::DragFloat("Ortho Far clip", &m_shadowMapPipeline.m_orthoParams[5], 1.f);
		ImGui::End();
	}

	render::RenderTarget* ShadowMapProcess::GetRenderTarget(uint32_t index) const
	{
		check(index < globals::MaxShadowMapAttachments);
		return m_shadowMapTargetArray[index].GetPtr();
	}

	void ShadowMapProcess::DebugDraw(const RenderContext& context)
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
				DebugRender::DrawScreenQuad({ w * (1.f-f), 0.f }, { w * f, h * f }, m_shadowMapTargetArray[m_debugIndex]->m_description.depthStencilAttachment.texture);
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
		if (m_debugLightParams.show)
		{
			tFrustum f = Camera::CalculateFrustum(m_debugLightParams.pos,
				m_debugLightParams.rot, glm::radians(m_debugLightParams.cutoff), 1.f, m_debugLightParams.clips[0], m_debugLightParams.clips[1]);
			glm::vec3 color = glm::vec3(0.2f, 0.96f, 0.5f);
			f.DrawDebug(color);
		}
		if (m_debugDirParams.show)
		{
			tFrustum f = Camera::CalculateFrustum(m_debugDirParams.pos, m_debugDirParams.rot,
				m_debugDirParams.clips[0],
				m_debugDirParams.clips[1],
				m_debugDirParams.clips[2],
				m_debugDirParams.clips[3],
				m_debugDirParams.clips[4],
				m_debugDirParams.clips[5]);
			f.DrawDebug({ 0.7f, 0.2f, 0.5f });
		}
	}
}