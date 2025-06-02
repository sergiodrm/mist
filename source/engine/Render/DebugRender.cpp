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
#include "RenderSystem/RenderSystem.h"


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
			render::BufferHandle vertexBuffer;

			inline void Reset() { LineArray.Clear(); }

			void Init(const RenderContext& context)
			{
#if 0
				BufferCreateInfo vbInfo;
				vbInfo.Size = sizeof(tLineVertex) * tLineBatch::MaxLines;
				vbInfo.Data = nullptr;
				vb.Init(context, vbInfo);
#endif // 0


				vertexBuffer = render::utils::CreateDynamicVertexBuffer(g_device, sizeof(tLineVertex) * tLineBatch::MaxLines, "DebugRenderLineBatchVB");
			}

			void Destroy(const RenderContext& context)
			{
				//vb.Destroy(context);
				vertexBuffer = nullptr; 
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
					//GPUBuffer::SubmitBufferToGpu(vb, LineArray.GetData(), sizeof(tLineVertex) * LineArray.GetSize());
					g_device->WriteBuffer(vertexBuffer, LineArray.GetData(), sizeof(tLineVertex) * LineArray.GetSize());
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
			tStaticArray<render::TextureHandle, MaxViews> Textures;
			VertexBuffer vb;
			IndexBuffer ib;
			render::BufferHandle vertexBuffer;
			render::BufferHandle indexBuffer;

			inline  void Reset() { QuadArray.Clear(); Textures.Clear(); }

			void Init(const RenderContext& context)
			{
				// Init IndexBuffer, we know which indices are to draw a quad
				constexpr uint32_t indicesPerQuad = 6;
				constexpr uint32_t indices[] = { 0, 2, 1, 1, 2, 3 };
				uint32_t buffer[tQuadBatch::MaxQuads * 6];
				uint32_t vertexOffset = 0;
				for (uint32_t i = 0; i < tQuadBatch::MaxQuads; ++i)
				{
					for (uint32_t j = 0; j < indicesPerQuad; ++j)
						buffer[(i * indicesPerQuad) + j] = indices[j] + vertexOffset;
					vertexOffset += 4;
				}
#if 0
				BufferCreateInfo quadInfo;
				quadInfo.Size = sizeof(tQuadVertex) * tQuadBatch::MaxQuads;
				quadInfo.Data = nullptr;
				vb.Init(context, quadInfo);
				quadInfo.Size = sizeof(uint32_t) * tQuadBatch::MaxQuads * indicesPerQuad;
				quadInfo.Data = buffer;
				ib.Init(context, quadInfo);
#endif // 0

				vertexBuffer = render::utils::CreateDynamicVertexBuffer(g_device, sizeof(tQuadVertex) * tQuadBatch::MaxQuads, "DebugRenderQuadBatchVB");
				indexBuffer = render::utils::CreateIndexBuffer(g_device, buffer, sizeof(uint32_t) * tQuadBatch::MaxQuads * indicesPerQuad, nullptr, "DebugRenderQuadBatchIB");
			}

			void Destroy(const RenderContext& context)
			{
				//vb.Destroy(context);
				//ib.Destroy(context);
				vertexBuffer = nullptr;
				indexBuffer = nullptr;
			}

			index_t SubmitQuadTexture(render::TextureHandle texture)
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
					logferror("[tQuadBatch::SubmitQuadTexture] Max textures reached per quad batch (MaxTextures: %d)\n", MaxViews);
				return index;
			}

			bool Flush(const RenderContext& context)
			{
				if (!QuadArray.IsEmpty())
				{
					//GPUBuffer::SubmitBufferToGpu(vb, QuadArray.GetData(), sizeof(tQuadVertex) * QuadArray.GetSize());
					g_device->WriteBuffer(vertexBuffer, QuadArray.GetData(), sizeof(tQuadVertex) * QuadArray.GetSize());
					return true;
				}
				return false;
			}
		};

		struct tDebugRenderPipeline
		{
			tQuadBatch QuadBatch;
			tLineBatch LineBatch;

			rendersystem::ShaderProgram* m_lineShader;
			rendersystem::ShaderProgram* m_quadShader;
		};
		tDebugRenderPipeline DebugRenderPipeline;

		void Init(const RenderContext& context)
		{
			//const cTexture* defaultTex = GetTextureCheckerboard4x4(context);
			//DebugRenderPipeline.QuadBatch.Textures.Clear(defaultTex);
			DebugRenderPipeline.QuadBatch.Textures.Clear();
			//Renderer* renderer = context.Renderer;
			{
				rendersystem::ShaderBuildDescription shaderDesc;
				shaderDesc.vsDesc.filePath = "shaders/line.vert";
				shaderDesc.fsDesc.filePath = "shaders/line.frag";
				DebugRenderPipeline.m_lineShader = _new rendersystem::ShaderProgram(g_device, shaderDesc);
			}
			{
                rendersystem::ShaderBuildDescription shaderDesc;
                shaderDesc.vsDesc.filePath = "shaders/screenquad.vert";
                shaderDesc.fsDesc.filePath = "shaders/screenquad.frag";
                DebugRenderPipeline.m_quadShader = _new rendersystem::ShaderProgram(g_device, shaderDesc);
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

		void DrawScreenQuad(const glm::vec2& screenPos, const glm::vec2& size, render::TextureHandle texture, uint32_t view)
		{
            if (DebugRenderPipeline.QuadBatch.QuadArray.GetSize() < tQuadBatch::MaxQuadVertices)
            {
                index_t viewIndex = DebugRenderPipeline.QuadBatch.SubmitQuadTexture(texture);
                if (viewIndex != index_invalid)
                {
                    glm::vec2 diffs[4] = { {0.f, 0.f}, {size.x, 0.f}, {0.f, size.y}, {size.x, size.y} };
                    index_t offset = DebugRenderPipeline.QuadBatch.QuadArray.GetSize();
                    DebugRenderPipeline.QuadBatch.QuadArray.Resize(offset + 4);
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

			// process batches before render passes.
			g_render->SetDefaultState();
			const bool processQuad = DebugRenderPipeline.QuadBatch.Flush(context);
			const bool processLine = DebugRenderPipeline.LineBatch.Flush(context);


			//renderer->GetLDRTarget().BeginPass(context, cmd);
			const CameraData& cameraData = *GetCameraData();
			if (processLine)
			{
				g_render->SetShader(DebugRenderPipeline.m_lineShader);
				g_render->SetVertexBuffer(DebugRenderPipeline.LineBatch.vertexBuffer);
				g_render->SetIndexBuffer(nullptr);
				g_render->SetShaderProperty("camera", &cameraData, sizeof(cameraData));
				g_render->Draw(DebugRenderPipeline.LineBatch.LineArray.GetSize());
				DebugRenderPipeline.LineBatch.Reset();
			}

			if (processQuad)
			{
				const glm::mat4 orthoproj = glm::ortho(0.f, (float)context.Window->Width, 0.f, (float)context.Window->Height, -1.f, 1.f);
				g_render->SetShader(DebugRenderPipeline.m_quadShader);
				g_render->SetVertexBuffer(DebugRenderPipeline.QuadBatch.vertexBuffer);
				g_render->SetIndexBuffer(DebugRenderPipeline.QuadBatch.indexBuffer);
				g_render->SetShaderProperty("u_camera", &orthoproj, sizeof(orthoproj));
				for (uint32_t i = 0; i < DebugRenderPipeline.QuadBatch.Textures.GetSize(); ++i)
					g_render->SetTextureSlot(DebugRenderPipeline.QuadBatch.Textures[i], 0, i);
				g_render->DrawIndexed(DebugRenderPipeline.QuadBatch.QuadArray.GetSize() / 4 * 6);
				DebugRenderPipeline.QuadBatch.Reset();
			}
		}
	}
}
