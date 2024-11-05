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

	struct sMaterialRenderData
	{
		glm::vec4 Emissive;
	};

	class cMaterial : public cRenderResource<RenderResource_Material>
	{
	public:
		cMaterial() { ZeroMem(this, sizeof(cMaterial)); }
		void Destroy(const RenderContext& context);

		void BindTextures(const RenderContext& context, ShaderProgram& shader, uint32_t slot);
		inline sMaterialRenderData GetRenderData() const { return { .Emissive = m_emissiveFactor }; }

		cTexture* m_textures[MATERIAL_TEXTURE_COUNT];
		glm::vec4 m_emissiveFactor;
	};
}
