#include "DebugRender.h"
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
#include "Render/RendererBase.h"
#include "CommandList.h"


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

		struct tLineVertex
		{
			glm::vec4 Position;
			glm::vec4 Color;
		};

		struct tQuadVertex
		{
			glm::vec2 ScreenPosition;
			glm::vec2 TexCoords;
			uint32_t TexIndex;
		};

		struct tLineBatch
		{
			static constexpr index_t MaxLines = 500;
			tStaticArray<tLineVertex, MaxLines> LineArray;
			VertexBuffer vb;

			inline void Reset() { LineArray.Clear(); }

			void Init(const RenderContext& context)
			{
				BufferCreateInfo vbInfo;
				vbInfo.Size = sizeof(tLineVertex) * tLineBatch::MaxLines;
				vbInfo.Data = nullptr;
				vb.Init(context, vbInfo);
			}

			void Destroy(const RenderContext& context)
			{
				vb.Destroy(context);
			}

			void PushLineVertex(const glm::vec3& pos, const glm::vec3& color)
			{
				if (LineArray.GetSize() < tLineBatch::MaxLines)
					LineArray.Push({ glm::vec4(pos, 1.f), glm::vec4(color, 1.f) });
				else
					logferror("DebugRender line overflow. Increase MaxLines (Current: %u)\n", tLineBatch::MaxLines);
			}

			void PushLine(const glm::vec3& init, const glm::vec3& end, const glm::vec3& color)
			{
				PushLineVertex(init, color);
				PushLineVertex(end, color);
			}

			bool Flush(const RenderContext& context)
			{
				if (!LineArray.IsEmpty())
				{
					// Flush lines to vertex buffer
					GPUBuffer::SubmitBufferToGpu(vb, LineArray.GetData(), sizeof(tLineVertex) * LineArray.GetSize());
					return true;
				}
				return false;
			}
		};

		struct tQuadBatch
		{
			static constexpr index_t MaxQuads = 100;
			static constexpr index_t MaxQuadVertices = MaxQuads * 4;
			static constexpr index_t MaxQuadIndices = MaxQuads * 6;
			static constexpr index_t MaxViews = 8;

			tStaticArray<tQuadVertex, MaxQuadVertices> QuadArray;
			tStaticArray<const cTexture*, MaxViews> Textures;
			VertexBuffer vb;
			IndexBuffer ib;

			inline  void Reset() { QuadArray.Clear(); Textures.Clear(); }

			void Init(const RenderContext& context)
			{
				BufferCreateInfo quadInfo;
				quadInfo.Size = sizeof(tQuadVertex) * tQuadBatch::MaxQuads;
				quadInfo.Data = nullptr;
				vb.Init(context, quadInfo);

				// Init IndexBuffer, we know which indices are to draw a quad
				constexpr uint32_t indicesPerQuad = 6;
				constexpr uint32_t indices[] = { 0, 2, 1, 1, 2, 3 };
				uint32_t indexBuffer[tQuadBatch::MaxQuads * 6];
				uint32_t vertexOffset = 0;
				for (uint32_t i = 0; i < tQuadBatch::MaxQuads; ++i)
				{
					for (uint32_t j = 0; j < indicesPerQuad; ++j)
						indexBuffer[(i * indicesPerQuad) + j] = indices[j] + vertexOffset;
					vertexOffset += 4;
				}
				quadInfo.Size = sizeof(uint32_t) * tQuadBatch::MaxQuads * indicesPerQuad;
				quadInfo.Data = indexBuffer;
				ib.Init(context, quadInfo);
			}

			void Destroy(const RenderContext& context)
			{
				vb.Destroy(context);
				ib.Destroy(context);
			}

			index_t SubmitQuadTexture(const cTexture* texture)
			{
				for (index_t i = 0; i < Textures.GetSize(); ++i)
				{
					if (texture == Textures[i])
						return i;
				}
				index_t index = index_invalid;
				if (Textures.GetSize() < MaxViews)
				{
					index = Textures.GetSize();
					Textures.Push(texture);
				}
				else
					logferror("DebugRender Quad overflow. Increase QuadBatch (Current %u).\n", MaxQuads);
				return index;
			}

			bool Flush(const RenderContext& context)
			{
				if (!QuadArray.IsEmpty())
				{
					GPUBuffer::SubmitBufferToGpu(vb, QuadArray.GetData(), sizeof(tQuadVertex) * QuadArray.GetSize());
					return true;
				}
				return false;
			}
		};

		struct tDebugRenderPipeline
		{
			tQuadBatch QuadBatch;
			tLineBatch LineBatch;

			ShaderProgram* m_lineShader;
			ShaderProgram* m_quadShader;
		};
		tDebugRenderPipeline DebugRenderPipeline;

		void Init(const RenderContext& context)
		{
			const cTexture* defaultTex = GetTextureCheckerboard4x4(context);
			DebugRenderPipeline.QuadBatch.Textures.Clear(defaultTex);
			Renderer* renderer = context.Renderer;
			{
				tShaderProgramDescription shaderDesc;
				shaderDesc.VertexShaderFile.Filepath = globals::LineVertexShader;
				shaderDesc.FragmentShaderFile.Filepath = globals::LineFragmentShader;
				shaderDesc.InputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float4, EAttributeType::Float4 });
				shaderDesc.Topology = PRIMITIVE_TOPOLOGY_LINE_LIST;
				shaderDesc.RenderTarget = &renderer->GetLDRTarget();
				DebugRenderPipeline.m_lineShader = ShaderProgram::Create(context, shaderDesc);
			}
			{
				tShaderProgramDescription shaderDesc;
				shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("screenquad.vert");
				shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("screenquad.frag");
				shaderDesc.InputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float2, EAttributeType::Float2, EAttributeType::Int });
				shaderDesc.RenderTarget = &renderer->GetLDRTarget();
				DebugRenderPipeline.m_quadShader = ShaderProgram::Create(context, shaderDesc);
			}

			DebugRenderPipeline.LineBatch.Init(context);
			DebugRenderPipeline.QuadBatch.Init(context);
		}

		void Destroy(const RenderContext& context)
		{
			DebugRenderPipeline.QuadBatch.Destroy(context);
			DebugRenderPipeline.LineBatch.Destroy(context);
		}

		void DrawLine3D(const glm::vec3& init, const glm::vec3& end, const glm::vec3& color)
		{
			DebugRenderPipeline.LineBatch.PushLine(init, end, color);
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

		void DrawScreenQuad(const glm::vec2& screenPos, const glm::vec2& size, const cTexture& texture, uint32_t view)
		{
			if (DebugRenderPipeline.QuadBatch.QuadArray.GetSize() < tQuadBatch::MaxQuadVertices)
			{
				index_t viewIndex = DebugRenderPipeline.QuadBatch.SubmitQuadTexture(&texture);
				if (viewIndex != index_invalid)
				{
					glm::vec2 diffs[4] = { {0.f, 0.f}, {size.x, 0.f}, {0.f, size.y}, {size.x, size.y} };
					index_t offset = DebugRenderPipeline.QuadBatch.QuadArray.GetSize();
					DebugRenderPipeline.QuadBatch.QuadArray.Resize(offset+4);
					for (index_t i = 0; i < 4; ++i)
					{
						tQuadVertex& vertex = DebugRenderPipeline.QuadBatch.QuadArray[offset + i];
						vertex.ScreenPosition = { screenPos.x + diffs[i].x, screenPos.y + diffs[i].y };
						vertex.TexCoords = QuadVertexUVs[i];
						vertex.TexIndex = viewIndex;
					}
				}
			}
			else
				logferror("DebugRender Quad overflow. Increate QuadBatch size (Current %u)\n", tQuadBatch::MaxQuads);
		}

		void Draw(const RenderContext& context)
		{
			CPU_PROFILE_SCOPE(DebugPass);
			Renderer* renderer = context.Renderer;
			//VkCommandBuffer cmd = context.GetFrameContext().GraphicsCommandContext.CommandBuffer;
            CommandList* commandList = context.CmdList;

			//BeginGPUEvent(context, cmd, "DebugRenderer");
            commandList->BeginMarker("DebugRenderer");

			// process batches before render passes.
			commandList->ClearState();
			const bool processQuad = DebugRenderPipeline.QuadBatch.Flush(context);
			const bool processLine = DebugRenderPipeline.LineBatch.Flush(context);

            GraphicsState state = {};
            state.Rt = &renderer->GetLDRTarget();
            commandList->SetGraphicsState(state);

			//renderer->GetLDRTarget().BeginPass(context, cmd);
			const CameraData& cameraData = *context.GetFrameContext().CameraData;
			if (processLine)
			{
                state.Program = DebugRenderPipeline.m_lineShader;
				state.Vbo = DebugRenderPipeline.LineBatch.vb;
                commandList->SetGraphicsState(state);
				
				//DebugRenderPipeline.m_lineShader->UseProgram(context);
				DebugRenderPipeline.m_lineShader->SetBufferData(context, "camera", &cameraData, sizeof(CameraData));
				//DebugRenderPipeline.m_lineShader->FlushDescriptors(context);
				//DebugRenderPipeline.LineBatch.vb.Bind(cmd);
				//RenderAPI::CmdDraw(cmd, DebugRenderPipeline.LineBatch.LineArray.GetSize(), 1, 0, 0);
				commandList->BindProgramDescriptorSets();
                commandList->Draw(DebugRenderPipeline.LineBatch.LineArray.GetSize(), 1, 0, 0);
				DebugRenderPipeline.LineBatch.Reset();
			}

			if (processQuad)
			{
				const glm::mat4 orthoproj = glm::ortho(0.f, (float)context.Window->Width, 0.f, (float)context.Window->Height, -1.f, 1.f);

                state.Program = DebugRenderPipeline.m_quadShader;
                state.Vbo = DebugRenderPipeline.QuadBatch.vb;
                state.Ibo = DebugRenderPipeline.QuadBatch.ib;
                commandList->SetGraphicsState(state);
				//DebugRenderPipeline.m_quadShader->UseProgram(context);
				DebugRenderPipeline.m_quadShader->BindSampledTextureArray(context, "u_tex", DebugRenderPipeline.QuadBatch.Textures.GetData(), tQuadBatch::MaxViews);
				DebugRenderPipeline.m_quadShader->SetBufferData(context, "u_camera", &orthoproj, sizeof(orthoproj));
				//DebugRenderPipeline.m_quadShader->FlushDescriptors(context);
				//DebugRenderPipeline.QuadBatch.vb.Bind(cmd);
				//DebugRenderPipeline.QuadBatch.ib.Bind(cmd);
				//RenderAPI::CmdDrawIndexed(cmd, DebugRenderPipeline.QuadBatch.QuadArray.GetSize() / 4 * 6, 1, 0, 0, 0);
				commandList->BindProgramDescriptorSets();
                commandList->DrawIndexed(DebugRenderPipeline.QuadBatch.QuadArray.GetSize() / 4 * 6, 1, 0, 0);
				DebugRenderPipeline.QuadBatch.Reset();
			}
			//renderer->GetLDRTarget().EndPass(cmd);
			//EndGPUEvent(context, cmd);
            commandList->EndMarker();
		}
	}
}
