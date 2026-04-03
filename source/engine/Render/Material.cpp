
#include "Material.h"
#include "Render/Mesh.h"
#include "Core/Debug.h"
#include "VulkanRenderEngine.h"
#include <string>
#include "RenderProcesses/GBuffer.h"
#include "Core/Logger.h"

#include "RenderSystem/RenderSystem.h"
#include "RenderSystem/TextureLoader.h"
#include "Utils/TimeUtils.h"

namespace Mist
{
    namespace mtl_serializer
    {

        static void SerializeTexture(YAML::Emitter& emitter, const char* name, const render::TextureHandle& texture, const render::SamplerHandle& sampler)
        {
            emitter << YAML::Key << name << YAML::BeginMap;
			emitter << YAML::Key << "Tex" << YAML::Value
				<< (texture ? texture->m_description.debugName.c_str() : "");
			emitter << YAML::Key << "Sampler" << YAML::Value
				<< (sampler && sampler->m_description.debugName.c_str() ? sampler->m_description.debugName.c_str() : "");
            emitter << YAML::EndMap;
        }

        static void UnserializeTexture(YAML::Node texNode, uint32_t textureId, cMaterial& mtl)
        {
            render::Device* device = g_device;
            check(device && texNode);
            YAML::Node t = texNode["Tex"];
            check(t);
            std::string str = t.as<std::string>();
            if (!str.empty())
                check(rendersystem::textureloader::LoadTextureFromFile(&mtl.m_textures[textureId], device, str.c_str()));
            YAML::Node s = texNode["Sampler"];
            check(s);
            str = s.as<std::string>();
            if (!str.empty())
                logfwarn("Load sampler from mtl file pending: %s\n", str.c_str());
        }

        static void Serialize(YAML::Emitter& emitter, const cMaterial* mtls, uint32_t count)
        {
            emitter << YAML::BeginSeq;
            for (uint32_t i = 0; i < count; ++i)
            {
                const cMaterial& mtl = mtls[i];
                emitter << YAML::BeginMap;
                emitter << YAML::Key << "Name" << YAML::Value << mtl.GetName();

                // Textures
                emitter << YAML::Key << "Textures" << YAML::BeginMap;
                SerializeTexture(emitter, "Albedo", mtl.m_textures[MATERIAL_TEXTURE_ALBEDO], mtl.m_samplers[MATERIAL_TEXTURE_ALBEDO]);
                SerializeTexture(emitter, "Normal", mtl.m_textures[MATERIAL_TEXTURE_NORMAL], mtl.m_samplers[MATERIAL_TEXTURE_NORMAL]);
                SerializeTexture(emitter, "Specular", mtl.m_textures[MATERIAL_TEXTURE_SPECULAR], mtl.m_samplers[MATERIAL_TEXTURE_SPECULAR]);
                SerializeTexture(emitter, "MetallicRoughness", mtl.m_textures[MATERIAL_TEXTURE_METALLIC_ROUGHNESS], mtl.m_samplers[MATERIAL_TEXTURE_METALLIC_ROUGHNESS]);
                SerializeTexture(emitter, "Emissive", mtl.m_textures[MATERIAL_TEXTURE_EMISSIVE], mtl.m_samplers[MATERIAL_TEXTURE_EMISSIVE]);
                SerializeTexture(emitter, "Occlusion", mtl.m_textures[MATERIAL_TEXTURE_OCCLUSION], mtl.m_samplers[MATERIAL_TEXTURE_OCCLUSION]);
                emitter << YAML::EndMap;

                // Properties
                emitter << YAML::Key << "Properties" << YAML::BeginMap;
                emitter << YAML::Key << "Albedo" << YAML::Value << mtl.m_albedo;
                emitter << YAML::Key << "Roughness" << YAML::Value << mtl.m_roughnessFactor;
                emitter << YAML::Key << "Metallic" << YAML::Value << mtl.m_metallicFactor;
                emitter << YAML::Key << "EmissiveColor" << YAML::Value << mtl.m_emissiveFactor;
                emitter << YAML::Key << "EmissiveStrength" << YAML::Value << mtl.m_emissiveStrength;
                emitter << YAML::Key << "Specular" << YAML::Value << mtl.m_specularFactor;
                emitter << YAML::Key << "AlphaCutoff" << YAML::Value << mtl.m_alphaCutoff;
                emitter << YAML::Key << "Flags" << YAML::Value << mtl.m_flags;
                emitter << YAML::EndMap;

                emitter << YAML::EndMap;
            }
            emitter << YAML::EndSeq;
        }

        static void Unserialize(YAML::Node n, cMaterial*& outMtls, uint32_t& outCount)
        {
            check(n.IsSequence());
            outCount = n.size();
            outMtls = (cMaterial*)_malloc(sizeof(cMaterial)*outCount);
            for (uint32_t i = 0; i < outCount; ++i)
            {
                YAML::Node it = n[i];
                check(it);
                cMaterial& mtl = outMtls[i];
                new(&mtl)cMaterial();

                mtl.SetName(it["Name"].as<std::string>().c_str());
                
                // textures
                YAML::Node texNode = it["Textures"];
                check(texNode);
                UnserializeTexture(texNode["Albedo"], MATERIAL_TEXTURE_ALBEDO, mtl);
                UnserializeTexture(texNode["Normal"], MATERIAL_TEXTURE_NORMAL, mtl);
                UnserializeTexture(texNode["Specular"], MATERIAL_TEXTURE_SPECULAR, mtl);
                UnserializeTexture(texNode["MetallicRoughness"], MATERIAL_TEXTURE_METALLIC_ROUGHNESS, mtl);
                UnserializeTexture(texNode["Emissive"], MATERIAL_TEXTURE_EMISSIVE, mtl);
                UnserializeTexture(texNode["Occlusion"], MATERIAL_TEXTURE_OCCLUSION, mtl);

                // properties
                YAML::Node properties = it["Properties"];
                check(properties);
                mtl.m_albedo = properties["Albedo"].as<glm::vec4>();
                mtl.m_roughnessFactor = properties["Roughness"].as<float>();
                mtl.m_metallicFactor = properties["Metallic"].as<float>();
                mtl.m_emissiveFactor = properties["EmissiveColor"].as<glm::vec3>();
                mtl.m_emissiveStrength = properties["EmissiveStrength"].as<float>();
                mtl.m_specularFactor = properties["Specular"].as<float>();
                mtl.m_alphaCutoff = properties["AlphaCutoff"].as<float>();
                mtl.m_flags = properties["Flags"].as<float>();

                mtl.SetupShader(g_render);
            }
        }

        static bool Serialize(const char* filepath, const cMaterial* mtls, uint32_t count)
        {
            YAML::Emitter e;
            Serialize(e, mtls, count);
            check(e.good());
            cFile f;
            cFile::eResult result = f.OpenBinary(filepath, cFile::FileMode_Write);
            if (result != cFile::Result_Ok)
                return false;
            f.Write(e.c_str(), e.size());
            f.Close();
            logfok("%d materials saved to: %s [%lld b]\n", count, cAssetPath(filepath), e.size());
            return true;
        }

        static bool Unserialize(const char* filepath, cMaterial*& outMtls, uint32_t& outCount)
        {
            char* buffer = nullptr;
            size_t size = 0;
            cFile f;
            check(f.OpenBinary(filepath, cFile::FileMode_Read) == cFile::Result_Ok);
            size = f.GetContentSize()+1;
            buffer = (char*)_malloc(size);
            f.Read(buffer, size, 1, size);
            buffer[size - 1] = 0;
            f.Close();
            check(buffer && size);

            YAML::Node root = YAML::Load(buffer);
            check(root);
            Unserialize(root, outMtls, outCount);
            _free(buffer);
            return true;
        }
    }


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

    void cMaterial::ConfigureShaderDescription(rendersystem::ShaderBuildDescription& shaderDesc)
    {
#define DECLARE_MACRO_ENUM(_flag) shaderDesc.fsDesc.options.PushMacroDefinition(#_flag, _flag)
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_NONE);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_HAS_ALBEDO_MAP);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_HAS_NORMAL_MAP);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_HAS_SPECULAR_GLOSSINESS_MAP);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_HAS_EMISSIVE_MAP);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_EMISSIVE);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_UNLIT);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_NO_PROJECT_SHADOWS);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS);

		DECLARE_MACRO_ENUM(MATERIAL_TEXTURE_ALBEDO);
		DECLARE_MACRO_ENUM(MATERIAL_TEXTURE_NORMAL);
		DECLARE_MACRO_ENUM(MATERIAL_TEXTURE_SPECULAR);
		DECLARE_MACRO_ENUM(MATERIAL_TEXTURE_OCCLUSION);
		DECLARE_MACRO_ENUM(MATERIAL_TEXTURE_METALLIC_ROUGHNESS);
		DECLARE_MACRO_ENUM(MATERIAL_TEXTURE_EMISSIVE);
#undef DECLARE_MACRO_ENUM
    }

    bool cMaterial::SerializeMaterials(const char* filepath, const cMaterial* mtls, uint32_t count)
    {
        if (!mtls || !count) 
            return false;
        PROFILE_SCOPE_LOGF(SerializeMaterials, "SerializeMaterials (%s|%d)", filepath, count);
        return mtl_serializer::Serialize(filepath, mtls, count);
    }

    bool cMaterial::UnserializeMaterials(const char* filepath, cMaterial*& mtls, uint32_t& count)
    {
        if (!filepath || !*filepath || !FileSystem::FileExists(filepath))
            return false;
        PROFILE_SCOPE_LOGF(UnserializeMaterials, "UnserializeMaterials (%s)", filepath);
        return mtl_serializer::Unserialize(filepath, mtls, count);
    }

    cMaterial::cMaterial()
        : m_shaderProgram(nullptr), m_flags(MATERIAL_FLAG_NONE), 
        m_emissiveFactor{0.f}, m_emissiveStrength(0.f), m_specularFactor(0.f),
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

    void cMaterial::SetupShader(rendersystem::RenderSystem* renderSystem)
    {
        static const char* gbuffervs = "shaders/gbuffer_main.vert";
        static const char* gbufferfs = "shaders/gbuffer_main.frag";
        static const char* forwardvs = "shaders/forward_lighting.vert";
        static const char* forwardfs = "shaders/forward_lighting.frag";
        static const char* alphaTestFlag = "ALPHA_TEST";

        check(!m_shaderProgram);
        rendersystem::ShaderBuildDescription desc;
        desc.type = rendersystem::ShaderProgram_Graphics;
        if (m_flags & MATERIAL_FLAG_BLEND)
        {
            check(!(m_flags & MATERIAL_FLAG_OPAQUE) && !(m_flags & MATERIAL_FLAG_MASK));
            desc.vsDesc.filePath = forwardvs;
            desc.fsDesc.filePath = forwardfs;
            desc.fsDesc.options.PushMacroDefinition("MAX_SHADOW_MAPS", static_cast<int>(globals::MaxShadowMapAttachments));
        }
        else
        {
            desc.vsDesc.filePath = gbuffervs;
            desc.fsDesc.filePath = gbufferfs;
            if (m_flags & MATERIAL_FLAG_MASK)
            {
                check(!(m_flags & MATERIAL_FLAG_OPAQUE));
                desc.fsDesc.options.PushMacroDefinition(alphaTestFlag);
            }
        }
        ConfigureShaderDescription(desc);
        m_shaderProgram = renderSystem->CreateShader(desc);
        check(m_shaderProgram);
    }

    void cMaterial::BindTextures(rendersystem::RenderSystem* renderSystem) const
    {
        g_render->SetTextureSlot("u_Textures", m_textures, MATERIAL_TEXTURE_COUNT);
        g_render->SetSampler("u_Textures", m_samplers, MATERIAL_TEXTURE_COUNT);
    }

    sMaterialRenderData cMaterial::GetRenderData() const
    {
        sMaterialRenderData data = {};
        data.emissive = glm::vec4(m_emissiveFactor.x, m_emissiveFactor.y, m_emissiveFactor.z, m_emissiveStrength);
        data.albedo = m_albedo;
        data.metallic = m_metallicFactor;
        data.roughness = m_roughnessFactor;
        data.specular = m_specularFactor;
        data.alphaCutoff = m_alphaCutoff;
        data.flags = m_flags;
        return data;
    }
}