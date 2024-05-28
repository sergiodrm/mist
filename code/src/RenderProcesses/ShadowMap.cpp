#include "ShadowMap.h"
#include <vector>
#include "VulkanRenderEngine.h"
#include "Logger.h"
#include "InitVulkanTypes.h"
#include "GBuffer.h"
#include "SceneImpl.h"
#include "VulkanBuffer.h"
#include "RenderDescriptor.h"
#include "imgui_internal.h"
#include "Renderers/DebugRenderer.h"
#include "glm/ext/matrix_clip_space.inl"
#include "glm/matrix.hpp"
#include "GenericUtils.h"

#define SHADOW_MAP_RT_FORMAT FORMAT_D32_SFLOAT
#define SHADOW_MAP_RT_LAYOUT IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL

namespace Mist
{
	bool GUseCameraForShadowMapping = false;

	ShadowMapPipeline::ShadowMapPipeline()
		: m_shader(nullptr)
	{
		SetProjection(glm::radians(45.f), 16.f / 9.f);
		SetProjection(0.f, 1920.f, 0.f, 1080.f);
		SetClip(1.f, 1000.f);

		memset(m_depthMVPCache, 0, sizeof(m_depthMVPCache));
	}

	ShadowMapPipeline::~ShadowMapPipeline()
	{
	}

	void ShadowMapPipeline::Init(const RenderContext& renderContext, const RenderTarget* renderTarget)
	{
		ShaderProgramDescription shaderDesc;

		// CreatePipeline
		ShaderDescription depthShader{ .Filepath = globals::DepthVertexShader, .Stage = VK_SHADER_STAGE_VERTEX_BIT };
		const VertexInputLayout inputLayout = VertexInputLayout::GetStaticMeshVertexLayout();

		shaderDesc.DynamicBuffers.push_back("u_ubo");
		shaderDesc.DynamicBuffers.push_back("u_model");
		shaderDesc.VertexShaderFile = globals::DepthVertexShader;
		shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
		shaderDesc.RenderTarget = renderTarget;
		m_shader = ShaderProgram::Create(renderContext, shaderDesc);
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

	void ShadowMapPipeline::SetClip(float nearClip, float farClip)
	{
		m_clip[0] = nearClip;
		m_clip[1] = farClip;
		DebugRender::SetDebugClipParams(nearClip, farClip);
	}

	glm::mat4 ShadowMapPipeline::GetProjection(EShadowMapProjectionType projType) const
	{
		switch (projType)
		{
		case PROJECTION_PERSPECTIVE:
			return glm::perspective(m_perspectiveParams[0], m_perspectiveParams[1], m_clip[0], m_clip[1]);
		case PROJECTION_ORTHOGRAPHIC:
			return glm::ortho(m_orthoParams[0], m_orthoParams[1], m_orthoParams[2], m_orthoParams[3], m_clip[0], m_clip[1]);
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

	void ShadowMapPipeline::SetupLight(uint32_t lightIndex, const glm::vec3& lightPos, const glm::vec3& lightRot, EShadowMapProjectionType projType)
	{
		glm::mat4 depthView = glm::inverse(math::ToMat4(lightPos, lightRot, glm::vec3(1.f)));
		glm::mat4 depthProj = GetProjection(projType);
		depthProj[1][1] *= -1.f;
		glm::mat4 depthVP = depthProj * depthView;
		SetDepthVP(lightIndex, depthVP);
	}

	void ShadowMapPipeline::FlushToUniformBuffer(const RenderContext& renderContext, UniformBufferMemoryPool* buffer)
	{
		buffer->SetUniform(renderContext, UNIFORM_ID_SHADOW_MAP_VP, m_depthMVPCache, GetBufferSize());
	}

	void ShadowMapPipeline::RenderShadowMap(VkCommandBuffer cmd, const Scene* scene, uint32_t frameIndex, uint32_t lightIndex)
	{
		check(lightIndex < globals::MaxShadowMapAttachments);
		m_shader->UseProgram(cmd);
		uint32_t depthVPOffset = sizeof(glm::mat4) * lightIndex;
		m_shader->BindDescriptorSets(cmd, &m_frameData[frameIndex].DepthMVPSet, 1, 0, &depthVPOffset, 1);

		scene->Draw(cmd, m_shader, 1, m_frameData[frameIndex].ModelSet);
	}

	const glm::mat4& ShadowMapPipeline::GetDepthVP(uint32_t index) const
	{
		check(index < globals::MaxShadowMapAttachments);
		return m_depthMVPCache[index];
	}

	void ShadowMapPipeline::SetDepthVP(uint32_t index, const glm::mat4& mat)
	{

		check(index < globals::MaxShadowMapAttachments);
		m_depthMVPCache[index] = mat;
	}

	uint32_t ShadowMapPipeline::GetBufferSize() const
	{
		return sizeof(glm::mat4) * globals::MaxShadowMapAttachments;
	}

	void ShadowMapPipeline::ImGuiDraw(bool createWindow)
	{
		if (createWindow)
			ImGui::Begin("ShadowMap proj params");
		if (ImGui::DragFloat("Near clip", &m_clip[0], 1.f) | ImGui::DragFloat("Far clip", &m_clip[1], 1.f))
			DebugRender::SetDebugClipParams(m_clip[0], m_clip[1]);
		ImGui::DragFloat("FOV", &m_perspectiveParams[0], 0.01f);
		ImGui::DragFloat("Aspect ratio", &m_perspectiveParams[1], 0.01f);
		ImGui::DragFloat2("Ortho x", &m_orthoParams[0]);
		ImGui::DragFloat2("Ortho y", &m_orthoParams[2]);
		if (createWindow)
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
		Scene* scene = renderFrameContext.Scene;
		EnvironmentData& envData = scene->GetEnvironmentData();
		m_attachmentIndexBits = 0;

		// Update shadow map matrix
		if (GUseCameraForShadowMapping)
		{
			m_shadowMapPipeline.SetDepthVP(0, renderFrameContext.CameraData->ViewProjection);
		}
		else
		{
			float shadowMapIndex = 0.f;
			if (envData.DirectionalLight.ShadowMapIndex >= 0.f)
			{
				m_attachmentIndexBits |= 1 << (uint32_t)shadowMapIndex;
				m_shadowMapPipeline.SetupLight(0, envData.DirectionalLight.Position, math::ToRot(envData.DirectionalLight.Direction), ShadowMapPipeline::PROJECTION_ORTHOGRAPHIC);
				envData.DirectionalLight.ShadowMapIndex = shadowMapIndex++;
			}

			for (uint32_t i = 0; i < EnvironmentData::MaxLights; ++i)
			{
				SpotLightData& light = envData.SpotLights[i];
				if (light.ShadowMapIndex >= 0.f)
				{
					m_attachmentIndexBits |= 1 << (uint32_t)shadowMapIndex;
					m_shadowMapPipeline.SetupLight((uint32_t)shadowMapIndex, light.Position, math::ToRot(light.Direction * -1.f), ShadowMapPipeline::PROJECTION_PERSPECTIVE);
					light.ShadowMapIndex = shadowMapIndex++;
					if ((uint32_t)shadowMapIndex >= globals::MaxShadowMapAttachments)
						break;
				}
			}
		}
		m_shadowMapPipeline.FlushToUniformBuffer(renderContext, &renderFrameContext.GlobalBuffer);

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
			lightMatrix[i] = renderFrameContext.CameraData->View * depthBias * m_shadowMapPipeline.GetDepthVP(i);
		}
		renderFrameContext.GlobalBuffer.SetUniform(renderContext, UNIFORM_ID_LIGHT_VP, lightMatrix, sizeof(glm::mat4) * globals::MaxShadowMapAttachments);
	}

	void ShadowMapProcess::Draw(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext)
	{
		VkCommandBuffer cmd = renderFrameContext.GraphicsCommand;
		BeginGPUEvent(renderContext, cmd, "ShadowMapping");

		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
		{
			m_shadowMapTargetArray[i].BeginPass(cmd);
			if (m_attachmentIndexBits & (1 << i))
			{
				const EnvironmentData& envData = renderFrameContext.Scene->GetEnvironmentData();
				m_shadowMapPipeline.RenderShadowMap(cmd, renderFrameContext.Scene, renderFrameContext.FrameIndex, i);
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