
#include "Material.h"
#include "Render/Mesh.h"
#include "Core/Debug.h"
#include "VulkanRenderEngine.h"
#include "Shader.h"

namespace Mist
{
    const char* GetMaterialTextureStr(eMaterialTexture type)
    {
        switch (type)
        {
        case MATERIAL_TEXTURE_ALBEDO: return "MATERIAL_TEXTURE_ALBEDO";
        case MATERIAL_TEXTURE_NORMAL: return "MATERIAL_TEXTURE_NORMAL";
        case MATERIAL_TEXTURE_SPECULAR: return "MATERIAL_TEXTURE_SPECULAR";
        case MATERIAL_TEXTURE_OCCLUSION: return "MATERIAL_TEXTURE_OCCLUSION";
        case MATERIAL_TEXTURE_METALLIC_ROUGHNESS: return "MATERIAL_TEXTURE_METALLIC_ROUGHNESS";
        case MATERIAL_TEXTURE_EMISSIVE: return "MATERIAL_TEXTURE_EMISSIVE";
        }
        check(false);
        return nullptr;
    }

    cMaterial::cMaterial()
    {
        ZeroMemory(m_textures, CountOf(m_textures) * sizeof(cTexture*));
    }

    void cMaterial::Destroy(const RenderContext& context)
    {
        for (uint32_t i = 0; i < MATERIAL_TEXTURE_COUNT; ++i)
        {
            if (m_textures[i])
                cTexture::Destroy(context, m_textures[i]);
            m_textures[i] = nullptr;
        }
    }
    void cMaterial::BindTextures(const RenderContext& context, ShaderProgram& shader, uint32_t slot)
    {
        cTexture* textures[MATERIAL_TEXTURE_COUNT];
        for (uint32_t i = 0; i < MATERIAL_TEXTURE_COUNT; ++i)
        {
            cTexture* tex = m_textures[i];
            if (!tex)
                tex = GetTextureCheckerboard4x4(context);
            textures[i] = tex;
        }
        shader.BindTextureArraySlot(context, slot, textures, MATERIAL_TEXTURE_COUNT);
    }
    sMaterialRenderData cMaterial::GetRenderData() const
    {
        sMaterialRenderData data { .Emissive = m_emissiveFactor, .TextureFlags = 0 };
        data.TextureFlags = m_textures[MATERIAL_TEXTURE_NORMAL] ? 1 : 0;
        return data;
    }
}