#include "DebugProcess.h"
#include "Core/Debug.h"
#include "Render/InitVulkanTypes.h"
#include "Render/Shader.h"
#include "Core/Logger.h"
#include "Render/Globals.h"
#include "Render/VulkanRenderEngine.h"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"
#include <corecrt_math_defines.h>
#include "imgui_internal.h"
#include "Utils/GenericUtils.h"
#include "Render/RenderAPI.h"
#include "Render/Texture.h"
#include "Application/Application.h"


namespace Mist
{
	namespace DebugRender
	{
		constexpr glm::vec2 QuadVertexUVs[4] =
		{
			{0.f, 0.f},
			{1.f, 0.f},
			{0.f, 1.f},
			{1.f, 1.f}
		};

		struct LineVertex
		{
			glm::vec4 Position;
			glm::vec4 Color;
		};

		struct QuadVertex
		{
			glm::vec2 ScreenPosition;
			glm::vec2 TexCoords;
			uint32_t TexIndex;
		};

		struct LineBatch
		{
			static constexpr index_t MaxLines = 500;

			tStaticArray<LineVertex, MaxLines> LineArray;
			inline void Reset() { LineArray.Clear(); }
		} GLineBatch;

		struct QuadBatch
		{
			static constexpr index_t MaxQuads = 100;
			static constexpr index_t MaxQuadVertices = MaxQuads * 4;
			static constexpr index_t MaxQuadIndices = MaxQuads * 6;
			static constexpr index_t MaxViews = 8;

			tStaticArray<QuadVertex, MaxQuadVertices> QuadArray;
			tStaticArray<const cTexture*, MaxViews> Textures;

			inline  void Reset() { QuadArray.Clear(); Textures.Clear(); }
		} GQuadBatch;


		index_t SubmitQuadTexture(QuadBatch& batch, const cTexture* texture)
		{
			for (index_t i = 0; i < batch.Textures.GetSize(); ++i)
			{
				if (texture == batch.Textures[i])
					return i;
			}
			index_t index = index_invalid;
			if (batch.Textures.GetSize() < batch.MaxViews)
			{
				index = batch.Textures.GetSize();
				batch.Textures.Push(texture);
			}
			else
				logferror("DebugRender Quad overflow. Increase QuadBatch (Current %u).\n", QuadBatch::MaxQuads);
			return index;
		}

		void PushLineVertex(LineBatch& batch, const glm::vec3& pos, const glm::vec3& color)
		{
			if (batch.LineArray.GetSize() < LineBatch::MaxLines)
				batch.LineArray.Push({ glm::vec4(pos, 1.f), glm::vec4(color, 1.f) });
			else
				logferror("DebugRender line overflow. Increase MaxLines (Current: %u)\n", LineBatch::MaxLines);
		}

		void PushLine(LineBatch& batch, const glm::vec3& init, const glm::vec3& end, const glm::vec3& color)
		{
			PushLineVertex(batch, init, color);
			PushLineVertex(batch, end, color);
		}

		void Init(const RenderContext& context)
		{
			const cTexture* defaultTex = GetTextureCheckerboard4x4(context);
			DebugRender::GQuadBatch.Textures.Clear(defaultTex);
		}

		void Destroy(const RenderContext& context)
		{
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

		void DrawScreenQuad(const glm::vec2& screenPos, const glm::vec2& size, const TextureBindingDescriptor& texDescriptor)
		{
			
		}

		void DrawScreenQuad(const glm::vec2& screenPos, const glm::vec2& size, ImageView view, EImageLayout layout, Sampler sampler)
		{
			DrawScreenQuad(screenPos, size, TextureBindingDescriptor{.View = view, .Layout = layout, .Sampler = sampler});
		}

		void DrawScreenQuad(const glm::vec2& screenPos, const glm::vec2& size, const cTexture& texture, uint32_t view)
		{
			if (GQuadBatch.QuadArray.GetSize() < QuadBatch::MaxQuadVertices)
			{
				index_t viewIndex = SubmitQuadTexture(GQuadBatch, &texture);
				if (viewIndex != index_invalid)
				{
					glm::vec2 diffs[4] = { {0.f, 0.f}, {size.x, 0.f}, {0.f, size.y}, {size.x, size.y} };
					index_t offset = GQuadBatch.QuadArray.GetSize();
					GQuadBatch.QuadArray.Resize(offset+4);
					for (index_t i = 0; i < 4; ++i)
					{
						QuadVertex& vertex = GQuadBatch.QuadArray[offset + i];
						vertex.ScreenPosition = { screenPos.x + diffs[i].x, screenPos.y + diffs[i].y };
						vertex.TexCoords = QuadVertexUVs[i];
						vertex.TexIndex = viewIndex;
					}
				}
			}
			else
				logferror("DebugRender Quad overflow. Increate QuadBatch size (Current %u)\n", QuadBatch::MaxQuads);
		}
#if 0

		void UpdateQuadTexDescriptorSet(const RenderContext& context, const QuadBatch& batch, VkDescriptorSet set, bool updateAll = false)
		{
			static Sampler defSampler = CreateSampler(context);
			uint32_t size = updateAll ? QuadBatch::MaxViews : GQuadBatch.ViewIndex;
			check(size > 0 && size <= QuadBatch::MaxViews);
			VkDescriptorImageInfo info[QuadBatch::MaxViews];
			for (uint32_t i = 0; i < size; ++i)
			{
				info[i].sampler = GQuadBatch.TexDescriptors[i].Sampler ? GQuadBatch.TexDescriptors[i].Sampler : defSampler;
				info[i].imageLayout = tovk::GetImageLayout(GQuadBatch.TexDescriptors[i].Layout);
				info[i].imageView = GQuadBatch.TexDescriptors[i].View;
			}
			VkWriteDescriptorSet writeInfo;
			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.pNext = nullptr;
			writeInfo.descriptorCount = size;
			writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeInfo.dstArrayElement = 0;
			writeInfo.dstBinding = 0;
			writeInfo.dstSet = set;
			writeInfo.pImageInfo = info;
			vkUpdateDescriptorSets(context.Device, 1, &writeInfo, 0, VK_NULL_HANDLE);
		}

		void BuildDescriptorSetQuadTex(const RenderContext& context, const QuadBatch& batch, VkDescriptorSet& set)
		{
			DescriptorLayoutCache& layoutCache = *context.LayoutCache;
			DescriptorAllocator& descriptorAllocator = *context.DescAllocator;
			VkDescriptorSetLayoutBinding binding;
			binding.binding = 0;
			binding.pImmutableSamplers = nullptr;
			binding.descriptorCount = QuadBatch::MaxViews;
			binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			VkDescriptorSetLayoutCreateInfo layoutInfo = vkinit::DescriptorSetLayoutCreateInfo(&binding, 1);
			VkDescriptorSetLayout layout = layoutCache.CreateLayout(layoutInfo);
			check(descriptorAllocator.Allocate(&set, layout));
			DebugRender::UpdateQuadTexDescriptorSet(context, batch, set, true);
		}

		void FlushQuadBatch(const RenderContext& context, CommandBuffer cmd, QuadBatch& batch, ShaderProgram& shader, const VertexBuffer& vb, const IndexBuffer& ib, VkDescriptorSet sets[2])
		{
			GPUBuffer::SubmitBufferToGpu(vb, &DebugRender::GQuadBatch.QuadArray, sizeof(DebugRender::QuadVertex) * DebugRender::GQuadBatch.Index * 4);
			DebugRender::UpdateQuadTexDescriptorSet(context, batch, sets[1]);
			shader.UseProgram(context);
			shader.BindDescriptorSets(cmd, sets, 2);
			vb.Bind(cmd);
			ib.Bind(cmd);
			RenderAPI::CmdDrawIndexed(cmd, DebugRender::GQuadBatch.Index * 6, 1, 0, 0, 0);
			DebugRender::GQuadBatch.Index = 0;
			DebugRender::GQuadBatch.ViewIndex = 0;
		}
#endif // 0

	}


	void DebugPipeline::Init(const RenderContext& context, const RenderTarget* renderTarget)
	{
		/**********************************/
		/** Pipeline layout and pipeline **/
		/**********************************/
		{
			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = globals::LineVertexShader;
			shaderDesc.FragmentShaderFile.Filepath = globals::LineFragmentShader;
			shaderDesc.InputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float4, EAttributeType::Float4 });
			shaderDesc.Topology = PRIMITIVE_TOPOLOGY_LINE_LIST;
			shaderDesc.RenderTarget = renderTarget;
			m_lineShader = ShaderProgram::Create(context, shaderDesc);
		}

		// VertexBuffer
		BufferCreateInfo vbInfo;
		vbInfo.Size = sizeof(DebugRender::LineVertex) * DebugRender::LineBatch::MaxLines;
		vbInfo.Data = nullptr;
		m_lineVertexBuffer.Init(context, vbInfo);

		BufferCreateInfo quadInfo;
		quadInfo.Size = sizeof(DebugRender::QuadVertex) * DebugRender::QuadBatch::MaxQuads;
		quadInfo.Data = nullptr;
		m_quadVertexBuffer.Init(context, quadInfo);

		constexpr uint32_t indicesPerQuad = 6;
		constexpr uint32_t indices[] = { 0, 2, 1, 1, 2, 3 };
		uint32_t indexBuffer[DebugRender::QuadBatch::MaxQuads * 6];
		uint32_t vertexOffset = 0;
		for (uint32_t i = 0; i < DebugRender::QuadBatch::MaxQuads; ++i)
		{
			for (uint32_t j = 0; j < indicesPerQuad; ++j)
				indexBuffer[(i * indicesPerQuad) + j] = indices[j] + vertexOffset;
			vertexOffset += 4;
		}
		quadInfo.Size = sizeof(uint32_t) * DebugRender::QuadBatch::MaxQuads * indicesPerQuad;
		quadInfo.Data = indexBuffer;
		m_quadIndexBuffer.Init(context, quadInfo);

		// Quad pipeline
		{
			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("screenquad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("screenquad.frag");
			shaderDesc.InputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float2, EAttributeType::Float2, EAttributeType::Int });
			shaderDesc.RenderTarget = renderTarget;
			m_quadShader = ShaderProgram::Create(context, shaderDesc);
		}
	}

	void DebugPipeline::PushFrameData(const RenderContext& context, UniformBufferMemoryPool* buffer)
	{
	}

	void DebugPipeline::PrepareFrame(const RenderContext& context, UniformBufferMemoryPool* buffer)
	{
	}

	void DebugPipeline::Draw(const RenderContext& context, VkCommandBuffer cmd, uint32_t frameIndex)
	{
		CPU_PROFILE_SCOPE(DebugPass);
		BeginGPUEvent(context, cmd, "DebugRenderer");
		if (!DebugRender::GLineBatch.LineArray.IsEmpty())
		{
			// Flush lines to vertex buffer
			DebugRender::LineBatch& batch = DebugRender::GLineBatch;
			GPUBuffer::SubmitBufferToGpu(m_lineVertexBuffer, &batch.LineArray, sizeof(DebugRender::LineVertex) * batch.LineArray.GetSize());
			m_lineShader->UseProgram(context);
			const CameraData& cameraData = *context.GetFrameContext().CameraData;
			m_lineShader->SetBufferData(context, "camera", &cameraData, sizeof(CameraData));
			m_lineShader->FlushDescriptors(context);
			m_lineVertexBuffer.Bind(cmd);
			RenderAPI::CmdDraw(cmd, batch.LineArray.GetSize(), 1, 0, 0);
			batch.Reset();
		}

		if (!DebugRender::GQuadBatch.QuadArray.IsEmpty())
		{
			DebugRender::QuadBatch& batch = DebugRender::GQuadBatch;
			GPUBuffer::SubmitBufferToGpu(m_quadVertexBuffer, &batch.QuadArray, sizeof(DebugRender::QuadVertex) * batch.QuadArray.GetSize());
			m_quadShader->UseProgram(context);
			m_quadShader->BindTextureArraySlot(context, 1, batch.Textures.GetData(), batch.MaxViews);
			glm::mat4 orthoproj = glm::ortho(0.f, (float)context.Window->Width, 0.f, (float)context.Window->Height, -1.f, 1.f);
			m_quadShader->SetBufferData(context, "u_camera", &orthoproj, sizeof(orthoproj));
			m_quadShader->FlushDescriptors(context);
			m_quadVertexBuffer.Bind(cmd);
			m_quadIndexBuffer.Bind(cmd);
			RenderAPI::CmdDrawIndexed(cmd, batch.QuadArray.GetSize() * 6, 1, 0, 0, 0);
			batch.Reset();
		}
		EndGPUEvent(context, cmd);
	}

	void DebugPipeline::Destroy(const RenderContext& context)
	{
		m_quadVertexBuffer.Destroy(context);
		m_quadIndexBuffer.Destroy(context);
		m_lineVertexBuffer.Destroy(context);
	}

	void DebugPipeline::ImGuiDraw()
	{
	}
}
