#pragma once

#include "Core/Types.h"
#include "RenderResource.h"
#include <glm/glm.hpp>

#include "RenderAPI/Device.h"

namespace rendersystem
{
	class RenderSystem;
	class ShaderProgram;
	struct ShaderBuildDescription;
}

namespace Mist
{
	class Texture;

	enum eMaterialTexture
	{
		MATERIAL_TEXTURE_ALBEDO,
		MATERIAL_TEXTURE_NORMAL,
		MATERIAL_TEXTURE_SPECULAR,
		MATERIAL_TEXTURE_OCCLUSION,
		MATERIAL_TEXTURE_METALLIC_ROUGHNESS,
		MATERIAL_TEXTURE_EMISSIVE,
		MATERIAL_TEXTURE_COUNT,
	};
	const char* GetMaterialTextureStr(eMaterialTexture type);

	enum eMaterialFlags
	{
		MATERIAL_FLAG_NONE = 0x0000,
		MATERIAL_FLAG_HAS_ALBEDO_MAP = 0x0001,
		MATERIAL_FLAG_HAS_NORMAL_MAP = 0x0002,
		MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP = 0x0004,
		MATERIAL_FLAG_HAS_SPECULAR_GLOSSINESS_MAP = 0x0008,
		MATERIAL_FLAG_HAS_EMISSIVE_MAP = 0x0010,
		MATERIAL_FLAG_EMISSIVE = 0x0020,
		MATERIAL_FLAG_UNLIT = 0x0040,
		MATERIAL_FLAG_NO_PROJECT_SHADOWS = 0x0080,
		MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS = 0x0100,
		MATERIAL_FLAG_OPAQUE = 0x0200,
		MATERIAL_FLAG_MASK = 0x0400,
		MATERIAL_FLAG_BLEND = 0x0800,
	};
	typedef uint32_t tMaterialFlags;
	const char* MaterialFlagToStr(tMaterialFlags flag);

	struct sMaterialRenderData
	{
		// w -> emissive strength
		glm::vec4 emissive;
		glm::vec4 albedo;
		
		float metallic;
		float roughness;
		float specular;
		float alphaCutoff;
		
		tMaterialFlags flags;
		uint32_t _padding[3];
	};

	class cMaterial : public cRenderResource<RenderResource_Material>
	{
	public:

		static void ConfigureShaderDescription(rendersystem::ShaderBuildDescription& shaderDesc);
		static bool SerializeMaterials(const char* filepath, const cMaterial* mtls, uint32_t count);
		static bool UnserializeMaterials(const char* filepath, cMaterial*& mtls, uint32_t& count);

		static cMaterial* GetDefaultMaterial();

		cMaterial();

		void Invalidate();
		void SetupShader(rendersystem::RenderSystem* renderSystem);

		void BindTextures(rendersystem::RenderSystem* renderSystem) const;
		sMaterialRenderData GetRenderData() const;

		Texture* GetTexture(eMaterialTexture textureType) const { check(textureType < MATERIAL_TEXTURE_COUNT); return m_textures[textureType]; }
		void SetTexture(eMaterialTexture textureType, Texture* texture);
		const render::SamplerHandle& GetSampler(eMaterialTexture textureType) const { check(textureType < MATERIAL_TEXTURE_COUNT); return m_samplers[textureType]; }
		void SetSampler(eMaterialTexture textureType, const render::SamplerHandle& texture);

		rendersystem::ShaderProgram* GetShaderProgram() const { return m_shaderProgram; }

		inline tMaterialFlags GetFlags() const { return m_flags; }
		inline void SetFlags(tMaterialFlags flags) { m_flags = flags; }

		inline void SetEmissiveColor(const glm::vec3& value) { m_emissiveFactor = value; }
		inline void SetEmissiveStrength(float value) { m_emissiveStrength = value; }
		inline void SetMetallic(float value) { m_metallicFactor = value; }
		inline void SetRoughness(float value) { m_roughnessFactor = value; }
		inline void SetSpecular(float value) { m_specularFactor = value; }
		inline void SetAlphaCutoff(float value) { m_alphaCutoff = value; }
		inline void SetAlbedo(const glm::vec4& value) { m_albedo = value; }

		inline const glm::vec3& GetEmissiveColor() const { return m_emissiveFactor; }
		inline float GetEmissiveStrength() const { return m_emissiveStrength; }
		inline float GetMetallic() const { return m_metallicFactor; }
		inline float GetRoughness() const { return m_roughnessFactor; }
		inline float GetSpecular() const { return m_specularFactor; }
		inline float GetAlphaCutoff() const { return m_alphaCutoff; }
		inline const glm::vec4& GetAlbedo() const { return m_albedo; }

		void ImGuiDraw();

	private:
		// Material flags
		tMaterialFlags m_flags;
		// Texture maps
		Texture* m_textures[MATERIAL_TEXTURE_COUNT];
		render::SamplerHandle m_samplers[MATERIAL_TEXTURE_COUNT];
		rendersystem::ShaderProgram* m_shaderProgram;

		glm::vec3 m_emissiveFactor;
		float m_emissiveStrength;

		float m_metallicFactor;
		float m_roughnessFactor;
		float m_specularFactor;
		float m_alphaCutoff;

		glm::vec4 m_albedo;
	};
}
