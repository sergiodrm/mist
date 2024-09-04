#include "ShadowMap.h"
#include <vector>
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "Render/InitVulkanTypes.h"
#include "GBuffer.h"
#include "Scene/SceneImpl.h"
#include "Render/VulkanBuffer.h"
#include "Render/RenderDescriptor.h"
#include "imgui_internal.h"
#include "glm/ext/matrix_clip_space.inl"
#include "glm/matrix.hpp"
#include "Utils/GenericUtils.h"
#include "Application/Application.h"

#define SHADOW_MAP_RT_FORMAT FORMAT_D32_SFLOAT
#define SHADOW_MAP_RT_LAYOUT IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL

namespace Mist
{
	bool GUseCameraForShadowMapping = false;

	ShadowMapPipeline::ShadowMapPipeline()
		: m_shader(nullptr)
	{
		SetProjection(glm::radians(45.f), 16.f / 9.f);
		SetProjection(-10.f, 10.f, -10.f, 10.f);
		SetPerspectiveClip(1.f, 1000.f);
		SetOrthographicClip(-10.f, 10.f);

		memset(m_depthMVPCache, 0, sizeof(m_depthMVPCache));
	}

	ShadowMapPipeline::~ShadowMapPipeline()
	{
	}

	void ShadowMapPipeline::Init(const RenderContext& renderContext, const RenderTarget* renderTarget)
	{
		GraphicsShaderProgramDescription shaderDesc;

		// CreatePipeline
		const VertexInputLayout inputLayout = VertexInputLayout::GetStaticMeshVertexLayout();

		tShaderDynamicBufferDescription modelDynDesc;
		modelDynDesc.Name = "u_model";
		modelDynDesc.ElemCount = globals::MaxRenderObjects;
		modelDynDesc.IsShared = true;

		tShaderDynamicBufferDescription uboDynDesc;
		uboDynDesc.Name = "u_ubo";
		uboDynDesc.ElemCount = globals::MaxShadowMapAttachments;
		uboDynDesc.IsShared = false;

		shaderDesc.DynamicBuffers.push_back(uboDynDesc);
		shaderDesc.DynamicBuffers.push_back(modelDynDesc);
		shaderDesc.VertexShaderFile.Filepath = globals::DepthVertexShader;
		shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
		shaderDesc.RenderTarget = renderTarget;
		m_shader = ShaderProgram::Create(renderContext, shaderDesc);
		m_shader->SetupDescriptors(renderContext);
	}

	void ShadowMapPipeline::Destroy(const RenderContext& renderContext)
	{	}

	void ShadowMapPipeline::AddFrameData(const RenderContext& renderContext, UniformBufferMemoryPool* buffer)
	{
		// Alloc info for depthVP matrix
		buffer->AllocUniform(renderContext, UNIFORM_ID_SHADOW_MAP_VP, GetBufferSize());

		FrameData fd;
		// Alloc desc set pointing to buffer
		VkDescriptorBufferInfo depthBufferInfo = buffer->GenerateDescriptorBufferInfo(UNIFORM_ID_SHADOW_MAP_VP);
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindBuffer(0, &depthBufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
			.Build(renderContext, fd.DepthMVPSet);

		// create descriptor set for model matrix array
		VkDescriptorBufferInfo modelsBufferInfo = buffer->GenerateDescriptorBufferInfo(UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY);
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindBuffer(0, &modelsBufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
			.Build(renderContext, fd.ModelSet);
		m_frameData.push_back(fd);
	}

	void ShadowMapPipeline::SetPerspectiveClip(float nearClip, float farClip)
	{
		m_perspectiveParams[2] = nearClip;
		m_perspectiveParams[3] = farClip;
		DebugRender::SetDebugClipParams(nearClip, farClip);
	}

	void ShadowMapPipeline::SetOrthographicClip(float nearClip, float farClip)
	{
		m_orthoParams[4] = nearClip;
		m_orthoParams[5] = farClip;
		DebugRender::SetDebugClipParams(nearClip, farClip);
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

	void ShadowMapPipeline::SetupLight(uint32_t lightIndex, const glm::vec3& lightPos, const glm::vec3& lightRot, EShadowMapProjectionType projType, const glm::mat4& viewMatrix)
	{
		static constexpr glm::mat4 depthBias =
		{
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f
		};

		// Projection goes to Z*-1.f, rotate lightRot 180 yaw to make it match to light.
		glm::mat4 lightRotMat = math::PitchYawRollToMat4(lightRot);
		lightRotMat = math::PitchYawRollToMat4({ 0.f, (float)M_PI, 0.f}) * lightRotMat;
		// Light translation
		glm::mat4 t = math::PosToMat4(lightPos);

		glm::mat4 depthView = glm::inverse(t * lightRotMat);
		glm::mat4 depthProj = GetProjection(projType);
		depthProj[1][1] *= -1.f;
		glm::mat4 depthVP = depthProj * depthView;
		// Light Matrix with inverse(viewMatrix) because gbuffer calculates position buffer in view space.
		glm::mat4 lightVP = depthBias * depthVP * glm::inverse(viewMatrix);
		SetDepthVP(lightIndex, depthVP);
		SetLightVP(lightIndex, lightVP);
	}

	void ShadowMapPipeline::FlushToUniformBuffer(const RenderContext& renderContext, UniformBufferMemoryPool* buffer)
	{
		buffer->SetUniform(renderContext, UNIFORM_ID_SHADOW_MAP_VP, m_depthMVPCache, GetBufferSize());
		buffer->SetUniform(renderContext, UNIFORM_ID_LIGHT_VP, m_lightMVPCache, GetBufferSize());
		m_shader->SetDynamicBufferData(renderContext, "u_ubo", m_depthMVPCache, sizeof(glm::mat4), globals::MaxShadowMapAttachments);
	}

	void ShadowMapPipeline::RenderShadowMap(const RenderContext& context, const Scene* scene, uint32_t frameIndex, uint32_t lightIndex)
	{
		VkCommandBuffer cmd = context.GetFrameContext().GraphicsCommand;
		check(lightIndex < globals::MaxShadowMapAttachments);
		m_shader->UseProgram(context);
		uint32_t depthVPOffset = sizeof(glm::mat4) * lightIndex; 
		//m_shader->BindDescriptorSets(cmd, &m_frameData[frameIndex].DepthMVPSet, 1, 0, &depthVPOffset, 1);
		m_shader->SetDynamicBufferOffset(context, "u_ubo", depthVPOffset);

		scene->Draw(context, m_shader, 1, m_frameData[frameIndex].ModelSet);
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
		ImGui::Begin("ShadowMap proj params");
		ImGui::DragFloat("Near clip", &m_perspectiveParams[3], 1.f);
		ImGui::DragFloat("Far clip", &m_perspectiveParams[4], 1.f);
		ImGui::DragFloat("FOV", &m_perspectiveParams[0], 0.01f);
		ImGui::DragFloat("Aspect ratio", &m_perspectiveParams[1], 0.01f);
		ImGui::DragFloat2("Ortho x", &m_orthoParams[0]);
		ImGui::DragFloat2("Ortho y", &m_orthoParams[2]);
		ImGui::DragFloat("Ortho Near clip", &m_orthoParams[4], 1.f);
		ImGui::DragFloat("Ortho Far clip", &m_orthoParams[5], 1.f);
		ImGui::End();
	}

	ShadowMapProcess::ShadowMapProcess()
	{
	}

	void ShadowMapProcess::Init(const RenderContext& context)
	{
		// RenderTarget description
		RenderTargetDescription rtDesc;
		rtDesc.SetDepthAttachment(SHADOW_MAP_RT_FORMAT, SHADOW_MAP_RT_LAYOUT, SAMPLE_COUNT_1_BIT, { .depthStencil = {.depth = 1.f} });
		rtDesc.RenderArea.extent = { .width = context.Window->Width, .height = context.Window->Height };

		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; i++)
			m_shadowMapTargetArray[i].Create(context, rtDesc);

		// Init shadow map pipeline when render target is created
		m_shadowMapPipeline.Init(context, &m_shadowMapTargetArray[0]);
	}

	void ShadowMapProcess::InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
		// Configure frame data for shadowmap
		m_shadowMapPipeline.AddFrameData(renderContext, &buffer);

		buffer.AllocUniform(renderContext, UNIFORM_ID_LIGHT_VP, sizeof(glm::mat4) * globals::MaxShadowMapAttachments);

		VkDescriptorImageInfo imageInfo;
		imageInfo.sampler = CreateSampler(renderContext);
		imageInfo.imageLayout = tovk::GetImageLayout(SHADOW_MAP_RT_LAYOUT);
		for (uint32_t j = 0; j < globals::MaxShadowMapAttachments; ++j)
		{
			// Shadow map descriptors for debug
			imageInfo.imageView = m_shadowMapTargetArray[j].GetDepthBuffer();
			DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
				.BindImage(0, &imageInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.Build(renderContext, m_frameData[frameIndex].DebugShadowMapTextureSet[j]);
		}
	}

	void ShadowMapProcess::Destroy(const RenderContext& renderContext)
	{
		for (uint32_t j = 0; j < globals::MaxShadowMapAttachments; ++j)
			m_shadowMapTargetArray[j].Destroy(renderContext);
		m_shadowMapPipeline.Destroy(renderContext);
	}

	void ShadowMapProcess::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& renderFrameContext)
	{
		Scene& scene = *renderFrameContext.Scene;
		
		// Update shadow map matrix
		if (GUseCameraForShadowMapping)
		{
			m_shadowMapPipeline.SetDepthVP(0, renderFrameContext.CameraData->ViewProjection);
		}
		else
		{
			float shadowMapIndex = 0.f;
			glm::mat4 invView = renderFrameContext.CameraData->InvView;

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
						m_shadowMapPipeline.SetupLight(m_lightCount++,
							glm::inverse(renderFrameContext.CameraData->InvView)[3],
							t.Rotation,
							ShadowMapPipeline::PROJECTION_ORTHOGRAPHIC,
							glm::inverse(invView));
						break;
					case ELightType::Spot:
						m_shadowMapPipeline.SetProjection(light->OuterCutoff * 2.f, 16.f / 9.f);
						m_shadowMapPipeline.SetupLight(m_lightCount++, t.Position, t.Rotation, ShadowMapPipeline::PROJECTION_PERSPECTIVE, invView);
						break;
					default:
						check(false && "Unreachable");
					}
				}
			}
			check(m_lightCount <= globals::MaxShadowMapAttachments);
		}

		m_shadowMapPipeline.FlushToUniformBuffer(renderContext, &renderFrameContext.GlobalBuffer);

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
		renderFrameContext.GlobalBuffer.SetUniform(renderContext, UNIFORM_ID_LIGHT_VP, lightMatrix, sizeof(glm::mat4) * globals::MaxShadowMapAttachments);
#endif // 0

	}

	void ShadowMapProcess::Draw(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext)
	{
		VkCommandBuffer cmd = renderFrameContext.GraphicsCommand;
		BeginGPUEvent(renderContext, cmd, "ShadowMapping");

		check(m_lightCount < globals::MaxShadowMapAttachments);
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
		{
			m_shadowMapTargetArray[i].BeginPass(cmd);
			if (i < m_lightCount)
			{
				m_shadowMapPipeline.RenderShadowMap(renderContext, renderFrameContext.Scene, renderFrameContext.FrameIndex, i);
			}
			m_shadowMapTargetArray[i].EndPass(cmd);
		}

		EndGPUEvent(renderContext, cmd);
	}

	void ShadowMapProcess::ImGuiDraw()
	{
		ImGui::Begin("Shadow mapping");
		static const char* modes[] = { "None", "Single tex", "All" };
		static const uint32_t modesSize = sizeof(modes) / sizeof(const char*);
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
		ImGui::Separator();
		m_shadowMapPipeline.ImGuiDraw(false);
		ImGui::End();
	}

	const RenderTarget* ShadowMapProcess::GetRenderTarget(uint32_t index) const
	{
		check(index < globals::MaxShadowMapAttachments);
		return &m_shadowMapTargetArray[index];
	}

	void ShadowMapProcess::DebugDraw(const RenderContext& context)
	{
		switch (m_debugMode)
		{
		case DEBUG_NONE:
			break;
		case DEBUG_SINGLE_RT:
			DebugRender::DrawScreenQuad({ 1920.f * 0.75f, 0.f }, { 1920.f * 0.25f, 1080.f * 0.25f }, m_shadowMapTargetArray[m_debugIndex].GetDepthBuffer(), SHADOW_MAP_RT_LAYOUT);
			break;
		case DEBUG_ALL:
			glm::vec2 screenSize = { 1920.f, 1080.f };
			float factor = 1.f / (float)globals::MaxShadowMapAttachments;
			glm::vec2 pos = { screenSize.x * (1.f - factor), 0.f };
			glm::vec2 size = { screenSize.x * factor, screenSize.y * factor };
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
			{
				DebugRender::DrawScreenQuad(pos, size, m_shadowMapTargetArray[i].GetDepthBuffer(), SHADOW_MAP_RT_LAYOUT);
				pos.y += size.y;
			}
			break;
		}
	}
}