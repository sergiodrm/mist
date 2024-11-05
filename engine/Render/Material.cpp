
#include "Material.h"
#include "Render/Mesh.h"
#include "Core/Debug.h"
#include "VulkanRenderEngine.h"
#include "Shader.h"

namespace Mist
{
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
                tex = GetDefaultTexture(context);
            textures[i] = tex;
        }
        shader.BindTextureArraySlot(context, slot, textures, MATERIAL_TEXTURE_COUNT);
    }
}