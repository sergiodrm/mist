
#include "Material.h"
#include "Texture.h"
#include "Render/Mesh.h"
#include "Core/Debug.h"
#include "VulkanRenderEngine.h"
#include <string>
#include "RenderProcesses/GBuffer.h"
#include "Core/Logger.h"

#include "RenderSystem/RenderSystem.h"
#include "RenderSystem/TextureLoader.h"
#include "Utils/TimeUtils.h"
#include "imgui.h"

namespace Mist
{
	static cMaterial g_defaultMaterial;

	namespace mtl_serializer
	{

		static void SerializeTexture(YAML::Emitter& emitter, const char* name, const char* filepath, const render::SamplerHandle& sampler)
		{
			emitter << YAML::Key << name << YAML::BeginMap;
			emitter << YAML::Key << "Tex" << YAML::Value
				<< (filepath && *filepath ? filepath : "");
			emitter << YAML::Key << "Sampler" << YAML::Value
				<< (sampler && sampler->m_description.debugName.c_str() ? sampler->m_description.debugName.c_str() : "");
			emitter << YAML::EndMap;
		}

		static void UnserializeTexture(YAML::Node texNode, Mist::eMaterialTexture textureId, cMaterial& mtl)
		{
			render::Device* device = g_device;
			check(device && texNode);
			YAML::Node t = texNode["Tex"];
			check(t);
			std::string str = t.as<std::string>();
			if (!str.empty())
			{
				Texture* texture = _new Texture();
				Mist::Texture::TextureLoader::LoadParams loadParams;
				loadParams.filepath = str.c_str();
				loadParams.calculateMipLevels = true;
				loadParams.format = GetMaterialTextureFormat(textureId);
				texture->LoadFromFile(loadParams);
				mtl.SetTexture(textureId, texture);
			}
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

				// Shader
				check(mtl.GetShaderProgram());
				emitter << YAML::Key << "Vertex shader" << YAML::Value << (mtl.GetShaderProgram()->GetVertexShader() ? mtl.GetShaderProgram()->GetVertexShader()->m_description.name.c_str() : "None");
				emitter << YAML::Key << "Fragment shader" << YAML::Value << (mtl.GetShaderProgram()->GetFragmentShader() ? mtl.GetShaderProgram()->GetFragmentShader()->m_description.name.c_str() : "None");

				// Textures
				emitter << YAML::Key << "Textures" << YAML::BeginMap;
				for (uint32_t i = 0; i < MATERIAL_TEXTURE_COUNT; ++i)
				{
					eMaterialTexture texId = (eMaterialTexture)i;
					if (mtl.GetTexture(texId))
						SerializeTexture(emitter, GetMaterialTextureStr(texId), mtl.GetTexture(texId)->GetLoadParams().filepath.c_str(), mtl.GetSampler(texId));
				}
				emitter << YAML::EndMap;

				// Properties
				emitter << YAML::Key << "Properties" << YAML::BeginMap;
				emitter << YAML::Key << "Albedo" << YAML::Value << mtl.GetAlbedo();
				emitter << YAML::Key << "Roughness" << YAML::Value << mtl.GetRoughness();
				emitter << YAML::Key << "Metallic" << YAML::Value << mtl.GetMetallic();
				emitter << YAML::Key << "EmissiveColor" << YAML::Value << mtl.GetEmissiveColor();
				emitter << YAML::Key << "EmissiveStrength" << YAML::Value << mtl.GetEmissiveStrength();
				emitter << YAML::Key << "Specular" << YAML::Value << mtl.GetSpecular();
				emitter << YAML::Key << "AlphaCutoff" << YAML::Value << mtl.GetAlphaCutoff();
#define MATERIAL_SERIALIZE_FLAG(_flag) emitter << YAML::Key << #_flag << YAML::Value << (bool)(mtl.GetFlags() & _flag)
				MATERIAL_SERIALIZE_FLAG(MATERIAL_FLAG_UNLIT);
				MATERIAL_SERIALIZE_FLAG(MATERIAL_FLAG_OPAQUE);
				MATERIAL_SERIALIZE_FLAG(MATERIAL_FLAG_MASK);
				MATERIAL_SERIALIZE_FLAG(MATERIAL_FLAG_BLEND);
				MATERIAL_SERIALIZE_FLAG(MATERIAL_FLAG_NO_PROJECT_SHADOWS);
				MATERIAL_SERIALIZE_FLAG(MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS);
#undef MATERIAL_SERIALIZE_FLAG
				emitter << YAML::EndMap;

				emitter << YAML::EndMap;
			}
			emitter << YAML::EndSeq;
		}

		static void Unserialize(YAML::Node n, cMaterial*& outMtls, uint32_t& outCount)
		{
			check(n.IsSequence());
			outCount = n.size();
			outMtls = (cMaterial*)_malloc(sizeof(cMaterial) * outCount);
			for (uint32_t i = 0; i < outCount; ++i)
			{
				YAML::Node it = n[i];
				check(it);
				cMaterial& mtl = outMtls[i];
				new(&mtl)cMaterial();

				mtl.SetName(it["Name"].as<std::string>().c_str());

				cAssetPath vs;
				cAssetPath fs;

				YAML::Node shader = it["Vertex shader"];
				if (shader && shader.IsScalar())
					vs = shader.as<std::string>().c_str();
				shader = it["Fragment shader"];
				if (shader && shader.IsScalar())
					fs = shader.as<std::string>().c_str();

				// textures
				YAML::Node texNode = it["Textures"];
				check(texNode && texNode.IsMap());
				for (uint32_t i = 0; i < MATERIAL_TEXTURE_COUNT; ++i)
				{
					eMaterialTexture texId = (eMaterialTexture)i;
					YAML::Node texIt = texNode[GetMaterialTextureStr(texId)];
					if (texIt)
					{
						check(texIt.IsMap());
						UnserializeTexture(texIt, texId, mtl);
					}
				}

				// properties
				YAML::Node properties = it["Properties"];
				check(properties);
				mtl.SetAlbedo(properties["Albedo"].as<glm::vec4>());
				mtl.SetRoughness(properties["Roughness"].as<float>());
				mtl.SetMetallic(properties["Metallic"].as<float>());
				mtl.SetEmissiveColor(properties["EmissiveColor"].as<glm::vec3>());
				mtl.SetEmissiveStrength(properties["EmissiveStrength"].as<float>());
				mtl.SetSpecular(properties["Specular"].as<float>());
				mtl.SetAlphaCutoff(properties["AlphaCutoff"].as<float>());
#define MATERIAL_UNSERIALIZE_FLAG(_flag) mtl.SetFlags(mtl.GetFlags() | (properties[#_flag].as<bool>() ? _flag : 0))
				MATERIAL_UNSERIALIZE_FLAG(MATERIAL_FLAG_UNLIT);
				MATERIAL_UNSERIALIZE_FLAG(MATERIAL_FLAG_OPAQUE);
				MATERIAL_UNSERIALIZE_FLAG(MATERIAL_FLAG_MASK);
				MATERIAL_UNSERIALIZE_FLAG(MATERIAL_FLAG_BLEND);
				MATERIAL_UNSERIALIZE_FLAG(MATERIAL_FLAG_NO_PROJECT_SHADOWS);
				MATERIAL_UNSERIALIZE_FLAG(MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS);
#undef MATERIAL_UNSERIALIZE_FLAG

				mtl.SetupShader(g_render, vs.c_str(), fs.c_str());
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
			size = f.GetContentSize() + 1;
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

	render::Format GetMaterialTextureFormat(eMaterialTexture type)
	{
		switch (type)
		{
		case MATERIAL_TEXTURE_ALBEDO:
		case MATERIAL_TEXTURE_EMISSIVE: return render::Format_R8G8B8A8_SRGB;
		case MATERIAL_TEXTURE_NORMAL:
		case MATERIAL_TEXTURE_SPECULAR:
		case MATERIAL_TEXTURE_OCCLUSION:
		case MATERIAL_TEXTURE_METALLIC_ROUGHNESS: return render::Format_R8G8B8A8_UNorm;
		default:
			unreachable_code();
		}
		return render::Format_Undefined;
	}

	const char* MaterialFlagToStr(tMaterialFlags flag)
	{
		switch (flag)
		{
		case MATERIAL_FLAG_NONE: return "MATERIAL_FLAG_NONE ";
		case MATERIAL_FLAG_UNLIT: return "MATERIAL_FLAG_UNLIT ";
		case MATERIAL_FLAG_NO_PROJECT_SHADOWS: return "MATERIAL_FLAG_NO_PROJECT_SHADOWS ";
		case MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS: return "MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS ";
		case MATERIAL_FLAG_OPAQUE: return "MATERIAL_FLAG_OPAQUE ";
		case MATERIAL_FLAG_MASK: return "MATERIAL_FLAG_MASK ";
		case MATERIAL_FLAG_BLEND: return "MATERIAL_FLAG_BLEND ";
		}
		unreachable_code();
		return nullptr;
	}

	void cMaterial::ConfigureShaderDescription(tMaterialFlags flags, rendersystem::ShaderBuildDescription& shaderDesc)
	{
#define DECLARE_MACRO_ENUM(_flag) if (flags & _flag) shaderDesc.fsDesc.options.PushMacroDefinition(#_flag)
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_NONE);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_UNLIT);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_NO_PROJECT_SHADOWS);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_NO_PROJECTED_BY_SHADOWS);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_OPAQUE);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_MASK);
		DECLARE_MACRO_ENUM(MATERIAL_FLAG_BLEND);
#undef DECLARE_MACRO_ENUM

#define DECLARE_MACRO_ENUM(_flag) shaderDesc.fsDesc.options.PushMacroDefinition(#_flag, _flag)
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

	cMaterial* cMaterial::GetDefaultMaterial()
	{
		g_defaultMaterial.SetName("DefaultMaterial");
		g_defaultMaterial.m_albedo = glm::vec4(1.f, 0.f, 1.f, 1.f);
		return &g_defaultMaterial;
	}

	cMaterial::cMaterial()
		: m_shaderProgram(nullptr), m_flags(MATERIAL_FLAG_NONE),
		m_emissiveFactor{ 0.f }, m_emissiveStrength(0.f), m_specularFactor(0.f),
		m_metallicFactor(0.f), m_roughnessFactor(0.f), m_albedo(1.f)
	{
		memset(m_textures, 0x0000, sizeof(m_textures));
		Invalidate();
	}

	void cMaterial::Invalidate()
	{
		for (uint32_t i = 0; i < MATERIAL_TEXTURE_COUNT; ++i)
		{
			m_samplers[i] = nullptr;
			if (m_textures[i])
			{
				delete m_textures[i];
				m_textures[i] = nullptr;
			}
		}
	}

	void cMaterial::SetupShader(rendersystem::RenderSystem* renderSystem)
	{
		static const char* gbuffervs = "shaders/gbuffer_main.vert";
		static const char* gbufferfs = "shaders/gbuffer_main.frag";
		static const char* forwardvs = "shaders/forward_lighting.vert";
		static const char* forwardfs = "shaders/forward_lighting.frag";
		if (m_flags & MATERIAL_FLAG_BLEND)
			SetupShader(renderSystem, forwardvs, forwardfs);
		else
			SetupShader(renderSystem, gbuffervs, gbufferfs);
	}

	void cMaterial::SetupShader(rendersystem::RenderSystem* renderSystem, const char* vsFile, const char* fsFile)
	{
		static const char* alphaTestFlag = "ALPHA_TEST";
		rendersystem::ShaderBuildDescription desc;
		desc.type = rendersystem::ShaderProgram_Graphics;
		desc.vsDesc.filePath = vsFile;
		desc.fsDesc.filePath = fsFile;
		if (m_flags & MATERIAL_FLAG_BLEND)
		{
			check(!(m_flags & MATERIAL_FLAG_OPAQUE) && !(m_flags & MATERIAL_FLAG_MASK));
			desc.fsDesc.options.PushMacroDefinition("MAX_SHADOW_MAPS", static_cast<int>(globals::MaxShadowMapAttachments));
		}
		else
		{
			if (m_flags & MATERIAL_FLAG_MASK)
			{
				check(!(m_flags & MATERIAL_FLAG_OPAQUE));
				desc.fsDesc.options.PushMacroDefinition(alphaTestFlag);
			}
		}

		ConfigureShaderDescription(m_flags, desc);
		m_shaderProgram = renderSystem->CreateShader(desc);
		check(m_shaderProgram);
	}

	void cMaterial::BindTextures(rendersystem::RenderSystem* renderSystem) const
	{
		render::TextureHandle textures[MATERIAL_TEXTURE_COUNT];
		for (uint32_t i = 0; i < MATERIAL_TEXTURE_COUNT; ++i)
			textures[i] = m_textures[i] ? m_textures[i]->GetDeviceTexture() : nullptr;
		g_render->SetTextureSlot("u_Textures", textures, MATERIAL_TEXTURE_COUNT);
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
		data.flags = (m_flags << 16);
		static_assert(MATERIAL_TEXTURE_COUNT < 16);
		for (uint32_t i = 0; i < MATERIAL_TEXTURE_COUNT; ++i)
			data.flags |= (eMaterialTexture)(m_textures[i] ? (1 << i) : 0);
		return data;
	}

	void cMaterial::SetTexture(eMaterialTexture textureType, Texture* texture)
	{
		check(textureType < MATERIAL_TEXTURE_COUNT);
		if (m_textures[textureType])
			delete m_textures[textureType];
		m_textures[textureType] = texture;
	}

	void cMaterial::SetSampler(eMaterialTexture textureType, const render::SamplerHandle& texture)
	{
		check(textureType < MATERIAL_TEXTURE_COUNT);
		m_samplers[textureType] = texture;
	}

	void cMaterial::ImGuiDraw()
	{
		for (index_t j = 0; j < MATERIAL_TEXTURE_COUNT; ++j)
		{
			const char* texName = (m_textures[j] && m_textures[j]->GetDeviceTexture()) ? m_textures[j]->GetDeviceTexture()->m_description.debugName.c_str() : "None";
			ImGui::Text("%s: %s", GetMaterialTextureStr((eMaterialTexture)j), texName);
		}
		ImGui::ColorEdit3("Albedo", &m_albedo[0]);
		ImGui::DragFloat("Metallic", &m_metallicFactor, 0.05f, 0.f, 1.f);
		ImGui::DragFloat("Roughness", &m_roughnessFactor, 0.05f, 0.f, 1.f);
		ImGui::ColorEdit3("Emissive", &m_emissiveFactor[0]);
		ImGui::DragFloat("Emissive strength", &m_emissiveStrength, 0.1f, 0.f, FLT_MAX);

	}
}