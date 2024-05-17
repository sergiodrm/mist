// Autogenerated code for Mist project
// Source file
#include "ModelRenderer.h"
#include "Debug.h"
#include "InitVulkanTypes.h"
#include "Shader.h"
#include "Globals.h"
#include "VulkanRenderEngine.h"
#include "SceneImpl.h"
#include "imgui_internal.h"

#include "DebugRenderer.h"
#include "GenericUtils.h"
#include "glm/ext/matrix_clip_space.inl"
#include "ModelRenderer.h"



namespace Mist
{
	bool GUseCameraForShadowMapping = false;

	// TODO: share render targets between renderers... do something
	tArray<tArray<VkImageView, globals::MaxShadowMapAttachments>, globals::MaxOverlappedFrames> GShadowMapViews;

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

		shaderDesc.DynamicBuffers.push_back("u_model");
		shaderDesc.VertexShaderFile = globals::DepthVertexShader;
		shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
		shaderDesc.RenderTarget = renderTarget;
		m_shader = ShaderProgram::Create(renderContext, shaderDesc);
	}

	void ShadowMapPipeline::Destroy(const RenderContext& renderContext)
	{	}

	void ShadowMapPipeline::AddFrameData(const RenderContext& renderContext, UniformBuffer* buffer)
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
		debugrender::SetDebugClipParams(nearClip, farClip);
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

	void ShadowMapPipeline::FlushToUniformBuffer(const RenderContext& renderContext, UniformBuffer* buffer)
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
			debugrender::SetDebugClipParams(m_clip[0], m_clip[1]);
		ImGui::DragFloat("FOV", &m_perspectiveParams[0], 0.01f);
		ImGui::DragFloat("Aspect ratio", &m_perspectiveParams[1], 0.01f);
		ImGui::DragFloat2("Ortho x", &m_orthoParams[0]);
		ImGui::DragFloat2("Ortho y", &m_orthoParams[2]);
		if (createWindow)
			ImGui::End();
	}

	ShadowMapRenderer::ShadowMapRenderer()
	{
	}

	void ShadowMapRenderer::Init(const RendererCreateInfo& info)
	{
		// RenderTarget description
		RenderTargetDescription rtDesc;
		rtDesc.SetDepthAttachment(FORMAT_D32_SFLOAT, IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { .depthStencil = {.depth = 1.f} });
		rtDesc.RenderArea.extent = { .width = info.Context.Window->Width, .height = info.Context.Window->Height };

		// Debug shadow mapping texture
		SamplerBuilder builder;
		m_debugSampler = builder.Build(info.Context);
		VkDescriptorImageInfo imageInfo;
		imageInfo.sampler = m_debugSampler.GetSampler();
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; i++)
		{
			UniformBuffer* uniformBuffer = info.FrameUniformBufferArray[i];
			// Configure frame data for shadowmap
			m_shadowMapPipeline.AddFrameData(info.Context, uniformBuffer);

			uniformBuffer->AllocUniform(info.Context, UNIFORM_ID_LIGHT_VP, sizeof(glm::mat4) * globals::MaxShadowMapAttachments);

			for (uint32_t j = 0; j < globals::MaxShadowMapAttachments; ++j)
			{
				// Create shadow map rt
				m_frameData[i].RenderTargetArray[j].Create(info.Context, rtDesc);
				GShadowMapViews[i][j] = m_frameData[i].RenderTargetArray[j].GetRenderTarget(0);

				// Shadow map descriptors for debug
				imageInfo.imageView = GShadowMapViews[i][j];
				DescriptorBuilder::Create(*info.Context.LayoutCache, *info.Context.DescAllocator)
					.BindImage(0, &imageInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
					.Build(info.Context, m_frameData[i].DebugShadowMapTextureSet[j]);
			}
		}

		// Init shadow map pipeline when render target is created
		m_shadowMapPipeline.Init(info.Context, &m_frameData[0].RenderTargetArray[0]);
	}

	void ShadowMapRenderer::Destroy(const RenderContext& renderContext)
	{
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			for (uint32_t j = 0; j < globals::MaxShadowMapAttachments; ++j)
				m_frameData[i].RenderTargetArray[j].Destroy(renderContext);
		}
		m_shadowMapPipeline.Destroy(renderContext);
		m_debugSampler.Destroy(renderContext);
	}

	void ShadowMapRenderer::PrepareFrame(const RenderContext& renderContext, RenderFrameContext& renderFrameContext)
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
			lightMatrix[i] = depthBias * m_shadowMapPipeline.GetDepthVP(i);
		}
		renderFrameContext.GlobalBuffer.SetUniform(renderContext, UNIFORM_ID_LIGHT_VP, lightMatrix, sizeof(glm::mat4) * globals::MaxShadowMapAttachments);
	}

	void ShadowMapRenderer::RecordCmd(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext, uint32_t attachmentIndex)
	{
		check(attachmentIndex < globals::MaxShadowMapAttachments);
		VkCommandBuffer cmd = renderFrameContext.GraphicsCommand;
		BeginGPUEvent(renderContext, cmd, "ShadowMapping");

		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
		{
			m_frameData[renderFrameContext.FrameIndex].RenderTargetArray[i].BeginPass(cmd);
			if (m_attachmentIndexBits & (1 << i))
			{
				const EnvironmentData& envData = renderFrameContext.Scene->GetEnvironmentData();
				m_shadowMapPipeline.RenderShadowMap(cmd, renderFrameContext.Scene, renderFrameContext.FrameIndex, attachmentIndex);
			}
			m_frameData[renderFrameContext.FrameIndex].RenderTargetArray[i].EndPass(cmd);
		}

		EndGPUEvent(renderContext, cmd);
	}

	void ShadowMapRenderer::ImGuiDraw()
	{
		ImGui::Begin("Shadow mapping");
		static bool debugShadows = false;
		ImGui::Checkbox("Debug shadow mapping", &debugShadows);
		static int shadowMapDebugIndex = 0;
		if (debugShadows)
		{
			ImGui::SliderInt("ShadowMap index", &shadowMapDebugIndex, 0, globals::MaxShadowMapAttachments-1);
			debugrender::SetDebugTexture(m_frameData[0].DebugShadowMapTextureSet[shadowMapDebugIndex]);
		}
		ImGui::Checkbox("Use camera for shadow mapping", &GUseCameraForShadowMapping);
		ImGui::Separator();
		m_shadowMapPipeline.ImGuiDraw(false);
		ImGui::End();
	}

	VkImageView ShadowMapRenderer::GetRenderTarget(uint32_t currentFrameIndex, uint32_t attachmentIndex) const
	{
		return m_frameData[currentFrameIndex].RenderTargetArray[attachmentIndex].GetRenderTarget(0);
	}

	VkImageView ShadowMapRenderer::GetDepthBuffer(uint32_t currentFrameIndex, uint32_t attachmentIndex) const
	{
		return m_frameData[currentFrameIndex].RenderTargetArray[attachmentIndex].GetDepthBuffer();
	}

	LightingRenderer::LightingRenderer() : IRendererBase()
	{
	}

	void LightingRenderer::Init(const RendererCreateInfo& info)
	{
		// Sampler for shadow map binding
		SamplerBuilder builder;
		//builder.AddressMode.AddressMode.U = SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		m_depthMapSampler = builder.Build(info.Context);

		// RenderTarget description for lighting
		RenderTargetDescription rtDesc;
		rtDesc.RenderArea.extent = { .width = info.Context.Window->Width, .height = info.Context.Window->Height };
		rtDesc.AddColorAttachment(FORMAT_R32G32B32A32_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { .color = {1.f, 0.f, 0.f, 1.f} });
		rtDesc.SetDepthAttachment(FORMAT_D32_SFLOAT, IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { .depthStencil = {.depth = 1.f} });

		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			// Create render target
			m_frameData[i].RT.Create(info.Context, rtDesc);

			UniformBuffer* uniformBuffer = info.FrameUniformBufferArray[i];

			// Configure per frame DescriptorSet.
			// Color pass
			VkDescriptorBufferInfo cameraDescInfo = uniformBuffer->GenerateDescriptorBufferInfo(UNIFORM_ID_CAMERA);
			VkDescriptorBufferInfo enviroDescInfo = uniformBuffer->GenerateDescriptorBufferInfo(UNIFORM_ID_SCENE_ENV_DATA);
			VkDescriptorBufferInfo modelsDescInfo = uniformBuffer->GenerateDescriptorBufferInfo(UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY);
			VkDescriptorBufferInfo lightMatrixDescInfo = uniformBuffer->GenerateDescriptorBufferInfo(UNIFORM_ID_LIGHT_VP);

			//check(globals::MaxShadowMapAttachments == info.ShadowMapAttachments[i].size());
			VkDescriptorImageInfo shadowMapDescInfo[globals::MaxShadowMapAttachments];
			for (uint32_t j = 0; j < globals::MaxShadowMapAttachments; ++j)
			{
				shadowMapDescInfo[j].imageView = GShadowMapViews[i][j];
				shadowMapDescInfo[j].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				shadowMapDescInfo[j].sampler = m_depthMapSampler.GetSampler();
			}

			const RenderTarget& rt = *(const RenderTarget*)info.AdditionalData;
			VkDescriptorImageInfo ssaoImageInfo;
			ssaoImageInfo.imageLayout = tovk::GetImageLayout(rt.GetDescription().ColorAttachmentDescriptions[0].Layout);
			ssaoImageInfo.imageView = rt.GetRenderTarget(0);
			ssaoImageInfo.sampler = m_depthMapSampler.GetSampler();

			// Materials have its own descriptor set (right now).

			DescriptorBuilder::Create(*info.Context.LayoutCache, *info.Context.DescAllocator)
				.BindBuffer(0, &cameraDescInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
				.BindBuffer(1, &lightMatrixDescInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
				.BindBuffer(2, &enviroDescInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.BindImage(3, shadowMapDescInfo, globals::MaxShadowMapAttachments, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.BindImage(4, &ssaoImageInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.Build(info.Context, m_frameData[i].PerFrameSet);
			DescriptorBuilder::Create(*info.Context.LayoutCache, *info.Context.DescAllocator)
				.BindBuffer(0, &modelsDescInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
				.Build(info.Context, m_frameData[i].ModelSet);
		}

		/**********************************/
		/** Pipeline layout and pipeline **/
		/**********************************/
		ShaderDescription shaderStageDescs[] =
		{
			{.Filepath = globals::BasicVertexShader, .Stage = VK_SHADER_STAGE_VERTEX_BIT},
			{.Filepath = globals::BasicFragmentShader, .Stage = VK_SHADER_STAGE_FRAGMENT_BIT}
		};

		ShaderProgramDescription shaderDesc;
		shaderDesc.DynamicBuffers.push_back("u_Object");
		shaderDesc.VertexShaderFile = globals::BasicVertexShader;
		shaderDesc.FragmentShaderFile = globals::BasicFragmentShader;
		shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
		shaderDesc.RenderTarget = &m_frameData[0].RT;
		m_shader = ShaderProgram::Create(info.Context, shaderDesc);
	}

	void LightingRenderer::Destroy(const RenderContext& renderContext)
	{
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
			m_frameData[i].RT.Destroy(renderContext);
		m_depthMapSampler.Destroy(renderContext);
	}

	void LightingRenderer::PrepareFrame(const RenderContext& renderContext, RenderFrameContext& renderFrameContext)
	{
		
	}

	void LightingRenderer::RecordCmd(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext, uint32_t attachmentIndex)
	{
		CPU_PROFILE_SCOPE(LightingRenderer);
		VkCommandBuffer cmd = renderFrameContext.GraphicsCommand;

		BeginGPUEvent(renderContext, cmd, "Forward Lighting");
		m_frameData[renderFrameContext.FrameIndex].RT.BeginPass(cmd);

		// Bind pipeline
		m_shader->UseProgram(cmd);
		// Bind global descriptor sets
		VkDescriptorSet sets[] = { m_frameData[renderFrameContext.FrameIndex].PerFrameSet };
		uint32_t setCount = sizeof(sets) / sizeof(VkDescriptorSet);
		m_shader->BindDescriptorSets(cmd, sets, setCount, 0, nullptr, 0);

		// DrawScene
		renderFrameContext.Scene->Draw(cmd, m_shader, 2, 1, m_frameData[renderFrameContext.FrameIndex].ModelSet);

		m_frameData[renderFrameContext.FrameIndex].RT.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);
	}

	void LightingRenderer::ImGuiDraw()
	{
	}

	VkImageView LightingRenderer::GetRenderTarget(uint32_t currentFrameIndex, uint32_t attachmentIndex) const
	{
		return m_frameData[currentFrameIndex].RT.GetRenderTarget(attachmentIndex);
	}

	VkImageView LightingRenderer::GetDepthBuffer(uint32_t currentFrameIndex, uint32_t attachmentIndex) const
	{
		return m_frameData[currentFrameIndex].RT.GetDepthBuffer();
	}

}
