#pragma once

#include "Core/Types.h"
#include "RenderSystem/RenderSystem.h"
#include "Scene/Scene.h"
#include "Render/Material.h"

namespace Mist
{
	class Terrain
	{
		struct TerrainDescription
		{
			float width = 0.f;
			float height = 0.f;
			int32_t patchCount = 0;
			int32_t patchPoints = 4;
		};
		struct NoiseDescription
		{
			int32_t width = 64;
			int32_t height = 64;
			float freq = 0.02f;
			float freqMult = 1.8f;
			float amplitude = 2.f;
			float amplitudeMult = 0.35f;
			uint32_t layers = 5;
			bool showTex = true;
		};
		struct TesselationControlParams
		{
			float minTesselationLevel = 4.f;
			float maxTesselationLevel = 16.f;
			float minDistance = 20.f;
			float maxDistance = 800.f;
		};
		struct TesselationEvaluationParams
		{
			float heightScale = 64.f;
			float heightShift = 16.f;
			glm::vec2 uvPadding = {};
		};
	public:
		Terrain();
		void Init(rendersystem::RenderSystem* rs);
		void Destroy();

		void Draw(rendersystem::RenderSystem* rs) const;
		void ImGuiDraw();

		inline uint32_t GetVertexCount() const { return m_description.patchPoints * m_description.patchCount * m_description.patchCount; }
	private:
		void InitTerrainVertices(rendersystem::RenderSystem* rs, const TerrainDescription& desc);
		void InitNoiseTexture(rendersystem::RenderSystem* rs);

	private:
		render::BufferHandle m_vb;
		rendersystem::ShaderProgram* m_shader;
		TerrainDescription m_description;
		TesselationControlParams m_controlParams;
		TesselationEvaluationParams m_evaluationParams;
		NoiseDescription m_noiseDesc;
		cMaterial m_mtl;
		render::TextureHandle m_heightMap;
		TransformComponent m_transform;
		render::TextureHandle m_noiseTex;

	};
}