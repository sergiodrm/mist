#pragma once

#include "Core/Types.h"
#include "Texture.h"
#include "RenderResource.h"
#include <glm/glm.hpp>

#include "RenderAPI/Device.h"

namespace rendersystem
{
	class ShaderProgram;
}

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
		MATERIAL_FLAG_NONE = 0x0000,
		MATERIAL_FLAG_HAS_ALBEDO_MAP = 0x0001,
		MATERIAL_FLAG_HAS_NORMAL_MAP = 0x0002,
		MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP = 0x0004,
		MATERIAL_FLAG_HAS_SPECULAR_GLOSSINESS_MAP = 0x0008,
		MATERIAL_FLAG_HAS_EMISSIVE_MAP = 0x0010,
		MATERIAL_FLAG_EMISSIVE = 0x0020,
		MATERIAL_FLAG_UNLIT = 0x0040,
		MATERIAL_FLAG_NO_PROJECT_SHADOWS = 0x0080,
		MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS = 0x0100
	};
	typedef uint32_t tMaterialFlags;
	const char* MaterialFlagToStr(tMaterialFlags flag);

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
		void SetupDescriptors(const RenderContext& context);
		void Destroy(const RenderContext& context);

		void BindTextures(uint32_t slot) const;
		sMaterialRenderData GetRenderData() const;

		ShaderProgram* m_shader;
		// Material flags
		tMaterialFlags m_flags;
		// Texture maps
		//cTexture* m_textures[MATERIAL_TEXTURE_COUNT];
		VkDescriptorSet m_textureSet;
		render::TextureHandle m_textures[MATERIAL_TEXTURE_COUNT];
		render::SamplerHandle m_samplers[MATERIAL_TEXTURE_COUNT];
		rendersystem::ShaderProgram* m_shaderProgram;

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
