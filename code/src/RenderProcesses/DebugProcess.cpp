#include "DebugProcess.h"
#include "Debug.h"
#include "InitVulkanTypes.h"
#include "Shader.h"
#include "Logger.h"
#include "Globals.h"
#include "VulkanRenderEngine.h"
#include "RendererBase.h"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"
#include <corecrt_math_defines.h>
#include "imgui_internal.h"


namespace Mist
{
	namespace debugrender
	{
		struct LineVertex
		{
			glm::vec4 Position;
			glm::vec4 Color;
		};

		struct LineBatch
		{
			static constexpr uint32_t MaxLines = 500;
			LineVertex LineArray[MaxLines];
			uint32_t Index = 0;
		} GLineBatch;

		VkDescriptorSet DebugTexture = VK_NULL_HANDLE;
		float NearClip = 1.f;
		float FarClip = 100.f;

		void PushLineVertex(LineBatch& batch, const glm::vec3& pos, const glm::vec3& color)
		{
			if (batch.Index < LineBatch::MaxLines)
			{
				batch.LineArray[batch.Index++] = { glm::vec4(pos, 1.f), glm::vec4(color, 1.f) };
			}
			else
				Logf(LogLevel::Error, "DebugRender line overflow. Increase MaxLines (Current: %u)\n", LineBatch::MaxLines);
		}

		void PushLine(LineBatch& batch, const glm::vec3& init, const glm::vec3& end, const glm::vec3& color)
		{
			PushLineVertex(batch, init, color);
			PushLineVertex(batch, end, color);
		}

		void DrawLine3D(const glm::vec3& init, const glm::vec3& end, const glm::vec3& color)
		{
			PushLine(GLineBatch, init, end, color);
		}

		void DrawAxis(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl)
		{
			glm::mat4 tras = glm::translate(glm::mat4{ 1.f }, pos);
			glm::mat4 rotMat = glm::toMat4(glm::quat(rot));
			glm::mat4 sclMat = glm::scale(scl);
			DrawAxis(tras * rotMat * sclMat);
		}

		void DrawAxis(const glm::mat4& transform)
		{
			DrawLine3D(transform[3], transform[3] + transform[0], glm::vec3(1.f, 0.f, 0.f));
			DrawLine3D(transform[3], transform[3] + transform[1], glm::vec3(0.f, 1.f, 0.f));
			DrawLine3D(transform[3], transform[3] + transform[2], glm::vec3(0.f, 0.f, 1.f));
		}

		void DrawSphere(const glm::vec3& pos, float radius, const glm::vec3& color, uint32_t vertices)
		{
			const float deltaAngle = 2.f * (float)M_PI / (float)vertices;
			for (uint32_t i = 0; i < vertices; ++i)
			{
				float c0 = radius * cosf(deltaAngle * i);
				float c1 = radius * cosf(deltaAngle * (i + 1));
				float s0 = radius * sinf(deltaAngle * i);
				float s1 = radius * sinf(deltaAngle * (i + 1));
				glm::vec3 pos0 = pos + glm::vec3{ c0, 0.f, s0 };
				glm::vec3 pos1 = pos + glm::vec3{ c1, 0.f, s1 };
				DrawLine3D(pos0, pos1, color);
				pos0 = pos + glm::vec3{ c0, s0, 0.f };
				pos1 = pos + glm::vec3{ c1, s1, 0.f };
				DrawLine3D(pos0, pos1, color);
				pos0 = pos + glm::vec3{ 0.f, c0, s0 };
				pos1 = pos + glm::vec3{ 0.f, c1, s1 };
				DrawLine3D(pos0, pos1, color);
			}
		}

		void SetDebugTexture(VkDescriptorSet descriptor)
		{
			DebugTexture = descriptor;
		}

		void SetDebugClipParams(float nearClip, float farClip)
		{
			NearClip = nearClip;
			FarClip = farClip;
		}
	}


	void DebugPipeline::Init(const RenderContext& context, const RenderTarget* renderTarget)
	{
		/**********************************/
		/** Pipeline layout and pipeline **/
		/**********************************/
		{
			ShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile = globals::LineVertexShader;
			shaderDesc.FragmentShaderFile = globals::LineFragmentShader;
			shaderDesc.InputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float4, EAttributeType::Float4 });
			shaderDesc.Topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			shaderDesc.RenderTarget = renderTarget;
			m_lineShader = ShaderProgram::Create(context, shaderDesc);
		}

		// VertexBuffer
		BufferCreateInfo vbInfo;
		vbInfo.Size = sizeof(debugrender::LineVertex) * debugrender::LineBatch::MaxLines;
		vbInfo.Data = nullptr;
		m_lineVertexBuffer.Init(context, vbInfo);

		float vertices[] =
		{
			0.5f, -1.f, 0.f, 0.f, 0.f,
			1.f, -1.f, 0.f, 1.f, 0.f,
			1.f, -0.5f, 0.f, 1.f, 1.f,
			0.5f, -0.5f, 0.f, 0.f, 1.f,
		};
		uint32_t indices[] = { 0, 2, 1, 0, 3, 2 };
		BufferCreateInfo quadInfo;
		quadInfo.Size = sizeof(vertices);
		quadInfo.Data = vertices;
		m_quadVertexBuffer.Init(context, quadInfo);

		quadInfo.Size = sizeof(uint32_t) * 6;
		quadInfo.Data = indices;
		m_quadIndexBuffer.Init(context, quadInfo);

		// Quad pipeline
		{
			ShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile = globals::QuadVertexShader;
			shaderDesc.FragmentShaderFile = globals::DepthQuadFragmentShader;
			shaderDesc.InputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float3, EAttributeType::Float2 });
			shaderDesc.RenderTarget = renderTarget;
			m_quadShader = ShaderProgram::Create(context, shaderDesc);
		}

		SamplerBuilder builder;
		m_depthSampler = builder.Build(context);
	}

	void DebugPipeline::PushFrameData(const RenderContext& context, UniformBufferMemoryPool* buffer)
	{
		FrameData fd;

		// Uniform buffer
		buffer->AllocUniform(context, "QuadUBO", sizeof(float) * 2);
		VkDescriptorBufferInfo bufferInfo = buffer->GenerateDescriptorBufferInfo("QuadUBO");

		DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator)
			.BindBuffer(0, &bufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(context, fd.SetUBO);

		VkDescriptorBufferInfo cameraBI = buffer->GenerateDescriptorBufferInfo(UNIFORM_ID_CAMERA);
		DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator)
			.BindBuffer(0, &cameraBI, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.Build(context, fd.CameraSet);

		m_frameSet.push_back(fd);
	}

	void DebugPipeline::PrepareFrame(const RenderContext& context, UniformBufferMemoryPool* buffer)
	{
		float ubo[2] = { debugrender::NearClip, debugrender::FarClip };
		buffer->SetUniform(context, "QuadUBO", ubo, sizeof(float) * 2);
	}

	void DebugPipeline::Draw(const RenderContext& context, VkCommandBuffer cmd, uint32_t frameIndex)
	{
		CPU_PROFILE_SCOPE(DebugPass);
		BeginGPUEvent(context, cmd, "DebugRenderer");
		if (debugrender::GLineBatch.Index > 0)
		{
			// Flush lines to vertex buffer
			GPUBuffer::SubmitBufferToGpu(m_lineVertexBuffer, &debugrender::GLineBatch.LineArray, sizeof(debugrender::LineVertex) * debugrender::GLineBatch.Index);

			m_lineShader->UseProgram(cmd);
			VkDescriptorSet sets[] = { m_frameSet[frameIndex].CameraSet };
			uint32_t setCount = sizeof(sets) / sizeof(VkDescriptorSet);
			m_lineShader->BindDescriptorSets(cmd, sets, setCount);
			m_lineVertexBuffer.Bind(cmd);
			vkCmdDraw(cmd, debugrender::GLineBatch.Index, 1, 0, 0);
			debugrender::GLineBatch.Index = 0;
			++Mist_profiling::GRenderStats.DrawCalls;
		}

		if (m_debugDepthMap && debugrender::DebugTexture != VK_NULL_HANDLE)
		{
			m_quadShader->UseProgram(cmd);
			VkDescriptorSet sets[2] = { m_frameSet[frameIndex].SetUBO, debugrender::DebugTexture };
			m_quadShader->BindDescriptorSets(cmd, sets, 2);
			m_quadVertexBuffer.Bind(cmd);
			m_quadIndexBuffer.Bind(cmd);
			vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
		}
		EndGPUEvent(context, cmd);
	}

	void DebugPipeline::Destroy(const RenderContext& context)
	{
		m_quadVertexBuffer.Destroy(context);
		m_quadIndexBuffer.Destroy(context);
		m_depthSampler.Destroy(context);
		m_lineVertexBuffer.Destroy(context);
	}

	void DebugPipeline::ImGuiDraw()
	{
		ImGui::Begin("debug");
		ImGui::Checkbox("DebugMap", &m_debugDepthMap);
		if (m_debugDepthMap)
		{
			if (debugrender::DebugTexture != VK_NULL_HANDLE)
				ImGui::TextColored(ImVec4(0.2f, 0.5f, 0.1f, 1.f), "Texture bound.");
			else
				ImGui::TextColored(ImVec4(0.5f, 0.1f, 0.1f, 1.f), "Texture NOT bound.");
		}
		ImGui::End();
	}



#if 0

	void DebugRenderer::Init(const RendererCreateInfo& info)
	{
		/**********************************/
		/** Pipeline layout and pipeline **/
		/**********************************/
		ShaderDescription descriptions[] =
		{
			{.Filepath = globals::LineVertexShader, .Stage = VK_SHADER_STAGE_VERTEX_BIT},
			{.Filepath = globals::LineFragmentShader, .Stage = VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		uint32_t descriptionCount = sizeof(descriptions) / sizeof(ShaderDescription);

		VertexInputLayout inputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float4, EAttributeType::Float4 });
		m_renderPipeline = RenderPipeline::Create(info.RContext, info.Pass, 0,
			descriptions, descriptionCount, inputLayout, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

		// VertexBuffer
		BufferCreateInfo vbInfo;
		vbInfo.Size = sizeof(debugrender::LineVertex) * debugrender::LineBatch::MaxLines;
		vbInfo.Data = nullptr;
		m_lineVertexBuffer.Init(info.RContext, vbInfo);

		QuadVertex vertices[] =
		{
			{{0.5f, -1.f, 0.f}, {0.f, 0.f}},
			{{1.f, -1.f, 0.f}, {1.f, 0.f}},
			{{1.f, -0.5f, 0.f}, {1.f, 1.f}},
			{{0.5f, -0.5f, 0.f}, {0.f, 1.f}},
		};
		uint32_t indices[] = { 0, 2, 1, 0, 3, 2 };
		BufferCreateInfo quadInfo;
		quadInfo.Size = sizeof(QuadVertex) * 4;
		quadInfo.Data = vertices;
		m_quadVertexBuffer.Init(info.RContext, quadInfo);

		quadInfo.Size = sizeof(uint32_t) * 6;
		quadInfo.Data = indices;
		m_quadIndexBuffer.Init(info.RContext, quadInfo);

		// quad pipeline
		ShaderDescription shaders[2]
		{
			{.Filepath = globals::QuadVertexShader, .Stage = VK_SHADER_STAGE_VERTEX_BIT},
			{.Filepath = globals::DepthQuadFragmentShader, .Stage = VK_SHADER_STAGE_FRAGMENT_BIT},
		};
		inputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float3, EAttributeType::Float2 });
		//m_quadPipeline = RenderPipeline::Create(info.RContext, info.Pass, 0, shaders, 2, inputLayout);

		VkSamplerCreateInfo samplerInfo = vkinit::SamplerCreateInfo(FILTER_LINEAR);
		vkCreateSampler(info.RContext.Device, &samplerInfo, nullptr, &m_depthSampler);

		// Submit buffer uniform info
		m_frameData.resize(globals::MaxOverlappedFrames);
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			// Uniform buffer
			UniformBuffer* buffer = info.FrameUniformBufferArray[i];
			buffer->AllocUniform(info.RContext, "QuadUBO", sizeof(float) * 2);
			VkDescriptorBufferInfo bufferInfo = buffer->GenerateDescriptorBufferInfo("QuadUBO");

			DescriptorBuilder::Create(*info.Context.LayoutCache, *info.Context.DescAllocator)
				.BindBuffer(0, &bufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.Build(info.RContext, m_frameData[i].SetUBO);
		}
	}

	void DebugRenderer::Destroy(const RenderContext& renderContext)
	{
		m_quadVertexBuffer.Destroy(renderContext);
		m_quadIndexBuffer.Destroy(renderContext);
		vkDestroySampler(renderContext.Device, m_depthSampler, nullptr);
		m_quadPipeline.Destroy(renderContext);
		m_lineVertexBuffer.Destroy(renderContext);
		m_renderPipeline.Destroy(renderContext);
	}

	void DebugRenderer::PrepareFrame(const RenderContext& renderContext, RenderFrameContext& renderFrameContext)
	{
		float ubo[2] = { debugrender::NearClip, debugrender::FarClip };
		renderFrameContext.GlobalBuffer.SetUniform(renderContext, "QuadUBO", ubo, sizeof(float) * 2);
	}

	void DebugRenderer::RecordCmd(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext, uint32_t attachmentIndex)
	{
		CPU_PROFILE_SCOPE(DebugPass);
		BeginGPUEvent(renderContext, renderFrameContext.GraphicsCommand, "DebugRenderer");
		if (debugrender::GLineBatch.Index > 0)
		{
			// Flush lines to vertex buffer
			GPUBuffer::SubmitBufferToGpu(m_lineVertexBuffer, &debugrender::GLineBatch.LineArray, sizeof(debugrender::LineVertex) * debugrender::GLineBatch.Index);

			vkCmdBindPipeline(renderFrameContext.GraphicsCommand, VK_PIPELINE_BIND_POINT_GRAPHICS, m_renderPipeline.GetPipelineHandle());
			VkDescriptorSet sets[] = { renderFrameContext.CameraDescriptorSet };
			uint32_t setCount = sizeof(sets) / sizeof(VkDescriptorSet);
			vkCmdBindDescriptorSets(renderFrameContext.GraphicsCommand, VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_renderPipeline.GetPipelineLayoutHandle(), 0, setCount, sets, 0, nullptr);
			++Mist_profiling::GRenderStats.SetBindingCount;
			m_lineVertexBuffer.Bind(renderFrameContext.GraphicsCommand);
			vkCmdDraw(renderFrameContext.GraphicsCommand, debugrender::GLineBatch.Index, 1, 0, 0);
			debugrender::GLineBatch.Index = 0;
			++Mist_profiling::GRenderStats.DrawCalls;
		}

		if (m_debugDepthMap && debugrender::DebugTexture != VK_NULL_HANDLE)
		{
			VkCommandBuffer cmd = renderFrameContext.GraphicsCommand;
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_quadPipeline.GetPipelineHandle());
			VkDescriptorSet sets[2] = { m_frameData[renderFrameContext.FrameIndex].SetUBO, debugrender::DebugTexture };
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_quadPipeline.GetPipelineLayoutHandle(), 0, 2, sets, 0, nullptr);
			m_quadVertexBuffer.Bind(cmd);
			m_quadIndexBuffer.Bind(cmd);
			vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
		}
		EndGPUEvent(renderContext, renderFrameContext.GraphicsCommand);
	}

	void DebugRenderer::ImGuiDraw()
	{
		ImGui::Begin("debug");
		ImGui::Checkbox("DebugMap", &m_debugDepthMap);
		if (m_debugDepthMap)
		{
			if (debugrender::DebugTexture != VK_NULL_HANDLE)
				ImGui::TextColored(ImVec4(0.2f, 0.5f, 0.1f, 1.f), "Texture bound.");
			else
				ImGui::TextColored(ImVec4(0.5f, 0.1f, 0.1f, 1.f), "Texture NOT bound.");
		}
		ImGui::End();
	}

	VkImageView DebugRenderer::GetRenderTarget(uint32_t currentFrameIndex, uint32_t attachmentIndex) const
	{
		return VK_NULL_HANDLE;
	}
#endif // 0

}