#pragma once

#include "Core/Types.h"
#include "Texture.h"
#include "RenderResource.h"
#include <glm/glm.hpp>

namespace Mist
{
	class ShaderProgram;
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
		MATERIAL_FLAG_NONE = 0x00,
		MATERIAL_FLAG_HAS_NORMAL_MAP = 0x01,
		MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP = 0x02,
		MATERIAL_FLAG_HAS_SPECULAR_GLOSSINESS_MAP = 0x04,
		MATERIAL_FLAG_HAS_EMISSIVE_MAP = 0x08,
		MATERIAL_FLAG_EMISSIVE = 0x10,
		MATERIAL_FLAG_UNLIT = 0x20,
		MATERIAL_FLAG_NO_PROJECT_SHADOWS = 0x40,
		MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS = 0x80
	};
	typedef uint32_t tMaterialFlags;

	struct sMaterialRenderData
	{
		// w -> emissive strength
		glm::vec4 Emissive;
		// w -> unused
		glm::vec4 Albedo;
		
		float Metallic;
		float Roughness;
		float _padding[2];
		
		tMaterialFlags Flags;
		uint32_t _padding2[3];
	};

	class cMaterial : public cRenderResource<RenderResource_Material>
	{
	public:
		cMaterial();
		void SetupShader(const RenderContext& context);
		void Destroy(const RenderContext& context);

		void BindTextures(const RenderContext& context, ShaderProgram& shader, uint32_t slot);
		sMaterialRenderData GetRenderData() const;

		ShaderProgram* m_shader;
		// Material flags
		tMaterialFlags m_flags;
		// Texture maps
		cTexture* m_textures[MATERIAL_TEXTURE_COUNT];

		// Emissive
		glm::vec3 m_emissiveFactor;
		float m_emissiveStrength;

		// Metallic roughness
		float m_metallicFactor;
		float m_roughnessFactor;

		// Albedo
		glm::vec3 m_albedo;
	};
}
