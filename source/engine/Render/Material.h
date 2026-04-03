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

		cMaterial();

		void Invalidate();
		void SetupShader(rendersystem::RenderSystem* renderSystem);

		void BindTextures(rendersystem::RenderSystem* renderSystem) const;
		sMaterialRenderData GetRenderData() const;

		// Material flags
		tMaterialFlags m_flags;
		// Texture maps
		render::TextureHandle m_textures[MATERIAL_TEXTURE_COUNT];
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
