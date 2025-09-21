
#include "Material.h"
#include "Render/Mesh.h"
#include "Core/Debug.h"
#include "VulkanRenderEngine.h"
#include <string>
#include "RenderProcesses/GBuffer.h"
#include "Core/Logger.h"

#include "RenderSystem/RenderSystem.h"

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

    const char* MaterialFlagToStr(tMaterialFlags flag)
    {
        switch (flag)
        {
            case MATERIAL_FLAG_NONE: return "MATERIAL_FLAG_NONE";
            case MATERIAL_FLAG_HAS_ALBEDO_MAP: return "MATERIAL_FLAG_HAS_ALBEDO_MAP";
            case MATERIAL_FLAG_HAS_NORMAL_MAP: return "MATERIAL_FLAG_HAS_NORMAL_MAP";
            case MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP: return "MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP";
            case MATERIAL_FLAG_HAS_SPECULAR_GLOSSINESS_MAP: return "MATERIAL_FLAG_HAS_SPECULAR_GLOSSINESS_MAP";
            case MATERIAL_FLAG_HAS_EMISSIVE_MAP: return "MATERIAL_FLAG_HAS_EMISSIVE_MAP";
            case MATERIAL_FLAG_EMISSIVE: return "MATERIAL_FLAG_EMISSIVE";
            case MATERIAL_FLAG_UNLIT: return "MATERIAL_FLAG_UNLIT";
            case MATERIAL_FLAG_NO_PROJECT_SHADOWS: return "MATERIAL_FLAG_NO_PROJECT_SHADOWS";
            case MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS: return "MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS";
        }
        unreachable_code();
        return nullptr;
    }

    cMaterial::cMaterial()
        : m_shader(nullptr), m_flags(MATERIAL_FLAG_NONE), 
        m_emissiveFactor{0.f}, m_emissiveStrength(0.f),
        m_metallicFactor(0.f), m_roughnessFactor(0.f), m_albedo(1.f)
    {
        Invalidate();
    }

    void cMaterial::Invalidate()
    {
        for (uint32_t i = 0; i < MATERIAL_TEXTURE_COUNT; ++i)
        {
            m_textures[i] = nullptr;
            m_samplers[i] = nullptr;
        }
    }

    void cMaterial::BindTextures(rendersystem::RenderSystem* renderSystem) const
    {
        g_render->SetTextureSlot("u_Textures", m_textures, MATERIAL_TEXTURE_COUNT);
        g_render->SetSampler("u_Textures", m_samplers, MATERIAL_TEXTURE_COUNT);
    }

    sMaterialRenderData cMaterial::GetRenderData() const
    {
        sMaterialRenderData data = {};
        data.Emissive = glm::vec4(m_emissiveFactor.x, m_emissiveFactor.y, m_emissiveFactor.z, m_emissiveStrength);
        data.Albedo = glm::vec4(m_albedo.x, m_albedo.y, m_albedo.z, 1.f);
        data.Metallic = m_metallicFactor;
        data.Roughness = m_roughnessFactor;
        data.Flags = m_flags;
        return data;
    }
}