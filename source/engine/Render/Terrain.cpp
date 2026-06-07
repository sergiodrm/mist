#include "Terrain.h"
#include "RenderSystem/TextureLoader.h"
#include "DebugRender.h"
#include "imgui.h"
#include "VulkanRenderEngine.h"
#include "Noise.h"
#include "RenderSystem/UI.h"

namespace Mist
{
	CIntVar CVar_Terrain("r_terrain", 1);

	extern rendersystem::RenderSystem* g_render;

	Terrain::Terrain()
		: m_vb(nullptr), m_shader(nullptr), m_heightMap(nullptr), m_noiseTex(nullptr), m_transform{ {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f} }
	{
	}

	void Terrain::Init(rendersystem::RenderSystem* rs)
	{
		// Load Iceland Heightmap test
		const char* heightMap = "textures/iceland_heightmap.png";
		rendersystem::textureloader::TextureData texData;
		rendersystem::textureloader::LoadTextureData_u8(&texData, heightMap);
		rendersystem::textureloader::LoadTextureFromFile(&m_heightMap, rs->GetDevice(), heightMap);
		rendersystem::textureloader::FreeTextureData(texData);

		InitNoiseTexture(rs);

		TerrainDescription desc;
		desc.width = 100.f;
		desc.height = 100.f;
		desc.patchCount = 10;
		desc.patchPoints = 4;
		InitTerrainVertices(rs, desc);

		rendersystem::ShaderBuildDescription shaderDesc;
		shaderDesc.SetGraphics("shaders/gbuffer_terrain_dyn.vert", "shaders/gbuffer_terrain.frag", "shaders/terrain.tcs", "shaders/terrain.tes");
		m_shader = rs->CreateShader(shaderDesc);

		rendersystem::ui::AddWindowCallback("Terrain", [](void* data)
			{
				check(data);
				Mist::Terrain* t = static_cast<Mist::Terrain*>(data);
				t->ImGuiDraw();
			}, this);
	}

	void Terrain::Destroy()
	{
		m_vb = nullptr;
		m_heightMap = nullptr;
	}

	void Terrain::Draw(rendersystem::RenderSystem* rs) const
	{
		if (!CVar_Terrain.Get())
			return;

		rs->BeginMarker("Terrain tesselation");
		rs->SetShader(m_shader);
		rs->SetDepthEnable(true, true);
		rs->SetBlendEnable(false);
		rs->SetPatchControlPoints(4);
		rs->SetPrimitive(render::PrimitiveType_PatchList);
		rs->SetVertexBuffer(m_vb);
		sMaterialRenderData materialData = m_mtl.GetRenderData();
		rs->SetShaderProperty("u_material", &materialData, sizeof(materialData));
		rs->SetTextureSlot("u_HeightMap", m_noiseTex);
		rs->SetSampler("u_HeightMap", 
			render::Filter_Linear, render::Filter_Linear, render::Filter_Linear,
			render::SamplerAddressMode_MirrorRepeat, 
			render::SamplerAddressMode_MirrorRepeat, 
			render::SamplerAddressMode_MirrorRepeat);
		rs->SetShaderProperty("u_tess", &m_controlParams, sizeof(TesselationControlParams));
		rs->SetShaderProperty("u_params", &m_evaluationParams, sizeof(TesselationEvaluationParams));
		glm::mat4 t;
		TransformComponentToMatrix(&m_transform, &t, 1);
		rs->SetShaderProperty("u_model", &t, sizeof(glm::mat4));
		rs->Draw(GetVertexCount());
		rs->EndMarker();

		if (m_noiseDesc.showTex)
		{
			float width = rs->GetRenderResolution().width;
			float height = rs->GetRenderResolution().height;
			DebugRender::DrawScreenQuad({ 0.25f * width, 0.f }, { 0.25f * width, 0.25f * height }, m_noiseTex);
		}
	}

	void Terrain::ImGuiDraw()
	{
		ImGui::Begin("Terrain");
		static bool moveWithCamera = false;
		ImGui::Checkbox("Move with camera", &moveWithCamera);
		if (!moveWithCamera)
			ImGuiUtils::EditTransform("Transform", &m_transform.Position[0], &m_transform.Rotation, &m_transform.Scale[0]);
		else
		{
			const CameraData& cameraData = *GetCameraData();
			m_transform.Position = math::GetPos(cameraData.InvView);
			m_transform.Position.y = 0.f;
		}

		m_evaluationParams.uvPadding = { m_transform.Position.x * 0.01f, m_transform.Position.z * 0.01f };

		ImGui::SeparatorText("Terrain generation");
		ImGui::Text("VB: %lld B", m_vb->m_description.size);
		ImGui::DragFloat("Width", &m_description.width, 0.5f, 0.f, FLT_MAX);
		ImGui::DragFloat("Height", &m_description.height, 0.5f, 0.f, FLT_MAX);
		ImGui::DragInt("Tile count", &m_description.patchCount, 1, 0, INT32_MAX);
		if (ImGui::Button("Regenerate terrain"))
			InitTerrainVertices(g_render, m_description);

		ImGui::SeparatorText("Height map noise");
		ImGui::DragInt("Width tex", &m_noiseDesc.width, 1, 0, INT32_MAX);
		ImGui::DragInt("Height tex", &m_noiseDesc.height, 1, 0, INT32_MAX);
		ImGui::DragFloat("Freq", &m_noiseDesc.freq, 0.25f);
		ImGui::DragFloat("FreqMult", &m_noiseDesc.freqMult, 0.25f);
		ImGui::DragFloat("Amplitude", &m_noiseDesc.amplitude, 0.25f);
		ImGui::DragFloat("AmplitudeMult", &m_noiseDesc.amplitudeMult, 0.25f);
		int layers = m_noiseDesc.layers;
		ImGui::DragInt("Layers", &layers);
		m_noiseDesc.layers = layers;
		ImGui::Checkbox("Show tex", &m_noiseDesc.showTex);
		if (ImGui::Button("Regenerate noise texture"))
			InitNoiseTexture(g_render);

		ImGui::SeparatorText("TCS");
		ImGui::DragFloat("MinTessLevel", &m_controlParams.minTesselationLevel, 0.5f, 0.f, m_controlParams.maxTesselationLevel);
		ImGui::DragFloat("MaxTessLevel", &m_controlParams.maxTesselationLevel, 0.5f, m_controlParams.minTesselationLevel, FLT_MAX);
		ImGui::DragFloat("MinDistance", &m_controlParams.minDistance, 0.1f, 0.f, FLT_MAX);
		ImGui::DragFloat("MaxDistance", &m_controlParams.maxDistance, 0.1f, 0.f, FLT_MAX);
		ImGui::SeparatorText("TES");
		ImGui::DragFloat("Height scale", &m_evaluationParams.heightScale);
		ImGui::DragFloat("Height shift", &m_evaluationParams.heightShift);
		ImGui::End();
	}

	void Terrain::InitTerrainVertices(rendersystem::RenderSystem* rs, const TerrainDescription& desc)
	{
		if (desc.width <= 0.f || desc.height <= 0.f || desc.patchPoints <= 0 || desc.patchCount <= 0)
		{
			logerror("Invalid terrain description.\n");
			return;
		}
		if (m_vb)
			m_vb = nullptr;

		m_description = desc;

		float halfWidth = m_description.width * 0.5f;
		float halfHeight = m_description.height * 0.5f;

		struct TerrainVertex
		{
			glm::vec3 p;
			glm::vec2 uv;
		};

		float invPatchCount = 1.f / (float)m_description.patchCount;
		float patchesInWidth = desc.width * invPatchCount;
		float patchesInHeight = desc.height * invPatchCount;

		uint32_t vertexCount = GetVertexCount();
		uint64_t bufferSize = vertexCount * sizeof(TerrainVertex);
		TerrainVertex* vertices = (TerrainVertex*)_malloc(bufferSize);
		logfinfo("Terrain: %d vertices -> %lld B\n", vertexCount, bufferSize);
		uint32_t c = 0;
		auto computeVertex = [&](uint32_t i, uint32_t j) -> TerrainVertex {
			return { { -halfWidth + (float)i * patchesInWidth, 0.f, -halfHeight + (float)j * patchesInHeight}, { (float)i * invPatchCount, (float)j * invPatchCount } };
			};
		for (uint32_t i = 0; i < m_description.patchCount; ++i)
		{
			for (uint32_t j = 0; j < m_description.patchCount; ++j)
			{
				vertices[c++] = computeVertex(i, j);
				vertices[c++] = computeVertex(i + 1, j);
				vertices[c++] = computeVertex(i, j + 1);
				vertices[c++] = computeVertex(i + 1, j + 1);
			}
		}
		m_vb = render::utils::CreateVertexBuffer(rs->GetDevice(), vertices, bufferSize);
		_free(vertices);
	}

	void Terrain::InitNoiseTexture(rendersystem::RenderSystem* rs)
	{
		uint32_t size = m_noiseDesc.width * m_noiseDesc.height;
		float* noise = (float*)_malloc(sizeof(float) * size);

		float maxNoise = 0.f;
		ValueNoise2D noiseGenerator;
		for (uint32_t i = 0; i < m_noiseDesc.width; ++i)
		{
			for (uint32_t j = 0; j < m_noiseDesc.height; ++j)
			{
				uint32_t index = (j * m_noiseDesc.width + i);

				glm::vec2 pointNoise = { i * m_noiseDesc.freq, j * m_noiseDesc.freq };
				float amplitude = m_noiseDesc.amplitude;
				noise[index] = 0.f;
				for (uint32_t i = 0; i < m_noiseDesc.layers; ++i)
				{
					noise[index] += noiseGenerator.Evaluate(pointNoise) * amplitude;
					pointNoise *= m_noiseDesc.freqMult;
					amplitude *= m_noiseDesc.amplitudeMult;
				}
				maxNoise = __max(maxNoise, noise[index]);
			}
		}

		uint16_t* data = (uint16_t*)_malloc(sizeof(uint16_t) * size);
		for (uint32_t i = 0; i < size; ++i)
			data[i] = uint16_t((float)(UINT16_MAX) * noise[i] / maxNoise);
		_free(noise);

		render::TextureDescription desc;
		desc.extent = { (uint32_t)m_noiseDesc.width, (uint32_t)m_noiseDesc.height, 1 };
		desc.format = render::Format_R16_UNorm;
		desc.memoryUsage = render::MemoryUsage_Gpu;
		desc.isRenderTarget = false;
		desc.isShaderResource = true;
		m_noiseTex = rs->GetDevice()->CreateTexture(desc);
		render::utils::UploadContext uploadCtx(rs->GetDevice());
		uploadCtx.WriteTexture(m_noiseTex, 0, 0, data, size * sizeof(uint16_t));
		uploadCtx.SetTextureLayout(m_noiseTex, render::ImageLayout_ShaderReadOnly);
		uploadCtx.Submit();
		_free(data);
	}
}