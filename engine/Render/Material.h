#pragma once

#include "Core/Types.h"
#include "Texture.h"
#include "RenderResource.h"

namespace Mist
{
	class ShaderProgram;
	enum
	{
		MATERIAL_TEXTURE_ALBEDO,
		MATERIAL_TEXTURE_NORMAL,
		MATERIAL_TEXTURE_SPECULAR,
		MATERIAL_TEXTURE_OCCLUSION,
		MATERIAL_TEXTURE_METALLIC_ROUGHNESS,
		MATERIAL_TEXTURE_EMISSIVE,
		MATERIAL_TEXTURE_COUNT,
	};

	class cMaterial : public cRenderResource
	{
	public:
		cMaterial() { ZeroMem(this, sizeof(cMaterial)); }
		void Destroy(const RenderContext& context);

		void BindTextures(const RenderContext& context, ShaderProgram& shader, uint32_t slot);

		Texture* m_textures[MATERIAL_TEXTURE_COUNT];
	};
}
