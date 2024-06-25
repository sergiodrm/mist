#include "Lighting.h"
#include <vector>
#include "VulkanRenderEngine.h"
#include "Logger.h"
#include "InitVulkanTypes.h"
#include "GBuffer.h"
#include "SceneImpl.h"
#include "VulkanBuffer.h"
#include "RenderDescriptor.h"
#include "imgui_internal.h"

#define GBUFFER_RT_FORMAT_POSITION FORMAT_R32G32B32A32_SFLOAT
#define GBUFFER_RT_FORMAT_NORMAL GBUFFER_RT_FORMAT_POSITION
#define GBUFFER_RT_FORMAT_ALBEDO FORMAT_R8G8B8A8_UNORM
#define GBUFFER_RT_FORMAT_DEPTH FORMAT_D32_SFLOAT


#define GBUFFER_RT_LAYOUT_POSITION IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
#define GBUFFER_RT_LAYOUT_NORMAL GBUFFER_RT_LAYOUT_POSITION
#define GBUFFER_RT_LAYOUT_ALBEDO GBUFFER_RT_LAYOUT_POSITION
#define GBUFFER_RT_LAYOUT_DEPTH IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL

#define GBUFFER_COMPOSITION_FORMAT FORMAT_R8G8B8A8_UNORM
#define GBUFFER_COMPOSITION_LAYOUT IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 


namespace Mist
{
	void DeferredLighting::Init(const RenderContext& renderContext)
	{
		tClearValue clearValue{ .color = {0.2f, 0.2f, 0.2f, 0.f} };
		RenderTargetDescription description;
		description.AddColorAttachment(GBUFFER_COMPOSITION_FORMAT, GBUFFER_COMPOSITION_LAYOUT, SAMPLE_COUNT_1_BIT, clearValue);
		description.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		description.RenderArea.offset = { .x = 0, .y = 0 };
		m_renderTarget.Create(renderContext, description);

		{
			// Deferred pipeline
			ShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile = SHADER_FILEPATH("deferred.frag");
			shaderDesc.RenderTarget = &m_renderTarget;
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			m_shader = ShaderProgram::Create(renderContext, shaderDesc);
		}

		// init quad
		float vertices[] =
		{
			// vkscreencoords	// uvs
			-1.f, -1.f, 0.f,	0.f, 0.f,
			1.f, -1.f, 0.f,		1.f, 0.f,
			1.f, 1.f, 0.f,		1.f, 1.f,
			-1.f, 1.f, 0.f,		0.f, 1.f
		};
		uint32_t indices[] = { 0, 2, 1, 0, 3, 2 };
		BufferCreateInfo bufferInfo;
		bufferInfo.Data = vertices;
		bufferInfo.Size = sizeof(vertices);
		m_quadVB.Init(renderContext, bufferInfo);
		bufferInfo.Data = indices;
		bufferInfo.Size = sizeof(indices);
		m_quadIB.Init(renderContext, bufferInfo);

#if 0
		{
			ShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile = SHADER_FILEPATH("skybox.vert");
			shaderDesc.FragmentShaderFile = SHADER_FILEPATH("skybox.frag");
			shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
			shaderDesc.RenderTarget = &m_renderTarget;
			shaderDesc.CullMode = CULL_MODE_FRONT_BIT;
			shaderDesc.DepthStencilMode = DEPTH_STENCIL_NONE;
			shaderDesc.FrontFaceMode = FRONT_FACE_COUNTER_CLOCKWISE;
			m_skyboxShader = ShaderProgram::Create(renderContext, shaderDesc);
		}
#endif // 0

	}

	void DeferredLighting::Destroy(const RenderContext& renderContext)
	{
		m_renderTarget.Destroy(renderContext);
		m_quadIB.Destroy(renderContext);
		m_quadVB.Destroy(renderContext);
	}

	void DeferredLighting::InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
		// Composition
		VkDescriptorBufferInfo info = buffer.GenerateDescriptorBufferInfo(UNIFORM_ID_SCENE_ENV_DATA);

		// image sampler
		Sampler sampler = CreateSampler(renderContext);

		// GBuffer textures binding
		const RenderProcess* gbuffer = renderer.GetRenderProcess(RENDERPROCESS_GBUFFER);
		const RenderTarget& rt = *gbuffer->GetRenderTarget();
		GBuffer::EGBufferTarget rts[3] = { GBuffer::EGBufferTarget::RT_POSITION, GBuffer::EGBufferTarget::RT_NORMAL, GBuffer::EGBufferTarget::RT_ALBEDO };
		tArray<VkDescriptorImageInfo, 3> infoArray;
		for (uint32_t i = 0; i < 3; ++i)
		{
			infoArray[i].sampler = sampler;
			infoArray[i].imageLayout = tovk::GetImageLayout(rt.GetDescription().ColorAttachmentDescriptions[rts[i]].Layout);
			infoArray[i].imageView = rt.GetRenderTarget(rts[i]);
		}

		// SSAO texture binding
		const RenderProcess* ssao = renderer.GetRenderProcess(RENDERPROCESS_SSAO);
		VkDescriptorImageInfo ssaoInfo;
		ssaoInfo.sampler = sampler;
		ssaoInfo.imageLayout = tovk::GetImageLayout(ssao->GetRenderTarget()->GetDescription().ColorAttachmentDescriptions[0].Layout);
		ssaoInfo.imageView = ssao->GetRenderTarget()->GetRenderTarget(0);

		// Shadow mapping
		const RenderProcess* shadowMapProcess = renderer.GetRenderProcess(RENDERPROCESS_SHADOWMAP);
		VkDescriptorImageInfo shadowMapTex[globals::MaxShadowMapAttachments];
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
		{
			const RenderTarget& rt = *shadowMapProcess->GetRenderTarget(i);
			shadowMapTex[i].sampler = sampler;
			shadowMapTex[i].imageLayout = tovk::GetImageLayout(rt.GetDescription().DepthAttachmentDescription.Layout);
			shadowMapTex[i].imageView = rt.GetDepthBuffer();
		}
		VkDescriptorBufferInfo shadowMapBuffer = buffer.GenerateDescriptorBufferInfo(UNIFORM_ID_LIGHT_VP);

		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindBuffer(0, &info, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(1, &infoArray[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(2, &infoArray[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(3, &infoArray[2], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(4, &ssaoInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(5, shadowMapTex, globals::MaxShadowMapAttachments, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindBuffer(6, &shadowMapBuffer, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(renderContext, m_frameData[frameIndex].Set);

#if 0
		// Skybox
		VkDescriptorBufferInfo cameraBufferInfo = buffer.GenerateDescriptorBufferInfo("ProjViewRot");
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindBuffer(0, &cameraBufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.Build(renderContext, m_frameData[frameIndex].CameraSkyboxSet);
#endif // 0

	}

	void DeferredLighting::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		VkCommandBuffer cmd = frameContext.GraphicsCommand;
		// Composition
		BeginGPUEvent(renderContext, cmd, "Deferred lighting", 0xff00ffff);
		m_renderTarget.BeginPass(cmd);
		m_shader->UseProgram(cmd);
		m_shader->BindDescriptorSets(cmd, &m_frameData[frameContext.FrameIndex].Set, 1);
		m_quadVB.Bind(cmd);
		m_quadIB.Bind(cmd);
		vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
		m_renderTarget.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);
	}
}