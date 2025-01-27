
#include "Material.h"
#include "Render/Mesh.h"
#include "Core/Debug.h"
#include "VulkanRenderEngine.h"
#include "Shader.h"
#include <string>

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
        : m_shader(nullptr), m_flags(MATERIAL_FLAG_NONE), 
        m_emissiveFactor{0.f}, m_emissiveStrength(0.f),
        m_metallicFactor(0.f), m_roughnessFactor(0.f), m_albedo(1.f)
    {
        ZeroMemory(m_textures, CountOf(m_textures) * sizeof(cTexture*));
    }

    extern RenderTarget* g_forwardRt;
    void cMaterial::SetupShader(const RenderContext& context)
    {
        check(!m_shader);
        tShaderProgramDescription desc;
        desc.VertexShaderFile.Filepath = SHADER_FILEPATH("forward_lighting.vert");
        desc.FragmentShaderFile.Filepath = SHADER_FILEPATH("forward_lighting.frag");
        desc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
        tShaderDynamicBufferDescription modelDynamicBuffer;
        modelDynamicBuffer.ElemCount = globals::MaxRenderObjects;
        modelDynamicBuffer.IsShared = true;
        modelDynamicBuffer.Name = "u_model";
        tShaderDynamicBufferDescription materialDynamicBuffer;
        materialDynamicBuffer.ElemCount = globals::MaxMaterials;
        materialDynamicBuffer.IsShared = true;
        materialDynamicBuffer.Name = "u_material";
        desc.DynamicBuffers.reserve(2);
        desc.DynamicBuffers.push_back(modelDynamicBuffer);
        desc.DynamicBuffers.push_back(materialDynamicBuffer);
        desc.RenderTarget = g_forwardRt;

        desc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.reserve(8);
        if (m_flags & MATERIAL_FLAG_UNLIT)
            desc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back({ "UNLIT" });
        if (m_flags & MATERIAL_FLAG_EMISSIVE)
            desc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back({ "EMISSIVE" });
        if (m_flags & MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS)
            desc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back({ "LIGHTING_NO_SHADOWS" });
        else
            desc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back({ "MAX_SHADOW_MAPS", MAX_SHADOW_MAPS_STR });

        m_shader = ShaderProgram::Create(context, desc);
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
    void cMaterial::BindTextures(const RenderContext& context, ShaderProgram& shader, uint32_t slot) const
    {
        cTexture* textures[MATERIAL_TEXTURE_COUNT];
        for (uint32_t i = 0; i < MATERIAL_TEXTURE_COUNT; ++i)
        {
            cTexture* tex = m_textures[i];
            if (!tex)
                tex = GetTextureCheckerboard4x4(context);
            textures[i] = tex;
        }
        //shader.BindTextureArraySlot(context, slot, textures, MATERIAL_TEXTURE_COUNT);
        shader.BindSampledTextureArray(context, "u_Textures", textures, MATERIAL_TEXTURE_COUNT);
    }
    sMaterialRenderData cMaterial::GetRenderData() const
    {
        sMaterialRenderData data;
        data.Emissive = glm::vec4(m_emissiveFactor.x, m_emissiveFactor.y, m_emissiveFactor.z, m_emissiveStrength);
        data.Albedo = glm::vec4(m_albedo.x, m_albedo.y, m_albedo.z, 1.f);
        data.Metallic = m_metallicFactor;
        data.Roughness = m_roughnessFactor;
        data.Flags = m_flags;
        return data;
    }
}