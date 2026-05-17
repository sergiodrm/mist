
#include "Model.h"
#include "Core/Debug.h"
#include "Core/Logger.h"
#include "RenderProcesses/RenderProcess.h"
#include <imgui/imgui.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/fwd.hpp>

#define CGLTF_IMPLEMENTATION
#pragma warning(disable:4996)
#include <gltf/cgltf.h>
#undef CGLTF_IMPLEMENTATION
#include "Material.h"
#include "Utils/GenericUtils.h"
#include "Utils/TimeUtils.h"
#include "Utils/FileSystem.h"
#include "VulkanRenderEngine.h"
#include "RenderSystem/TextureLoader.h"
#include "RenderSystem/RenderSystem.h"
#include "DebugRender.h"
#include "Texture.h"

#define GLTF_LOAD_GEOMETRY_POSITION 0x01
#define GLTF_LOAD_GEOMETRY_NORMAL 0x02
#define GLTF_LOAD_GEOMETRY_COLOR 0x04
#define GLTF_LOAD_GEOMETRY_TANGENT 0x08
#define GLTF_LOAD_GEOMETRY_TEXCOORDS 0x10
#define GLTF_LOAD_GEOMETRY_JOINTS 0x20
#define GLTF_LOAD_GEOMETRY_WEIGHTS 0x40
#define GLTF_LOAD_GEOMETRY_ALL 0xff

// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_sampler_magfilter
#define GLTF_DEFAULT_SAMPLER 10497
#define GLTF_MAG_FILTER_NEAREST 9728
#define GLTF_MAG_FILTER_LINEAR 9729
#define GLTF_MIN_FILTER_NEAREST GLTF_MAG_FILTER_NEAREST
#define GLTF_MIN_FILTER_LINEAR GLTF_MAG_FILTER_LINEAR
#define GLTF_MIN_FILTER_NEAREST_MIPMAP_NEAREST 9984
#define GLTF_MIN_FILTER_LINEAR_MIPMAP_NEAREST 9985
#define GLTF_MIN_FILTER_NEAREST_MIPMAP_LINEAR 9986
#define GLTF_MIN_FILTER_LINEAR_MIPMAP_LINEAR 9987
#define GLTF_WRAPPER_CLAMP_TO_EDGE 33071
#define GLTF_WRAPPER_MIRRORED_REPEAT 33648
#define GLTF_WRAPPER_REPEAT 10497


#define loadmeshlabel "[loadmesh] "
//#define MESH_DUMP_LOAD_INFO
#ifdef MESH_DUMP_LOAD_INFO
#define loadmeshlogf(fmt, ...) logfinfo(loadmeshlabel fmt, __VA_ARGS__)
#define loadmeshlog(fmt, ...) loginfo(loadmeshlabel fmt)
#else
#define loadmeshlogf(fmt, ...) DUMMY_MACRO
#define loadmeshlog(fmt, ...) DUMMY_MACRO
#endif

#define MESH_PROFILE
#ifdef MESH_PROFILE
#define loadmesh_profile_log_scope(msg) PROFILE_SCOPE_LOG(loadmesh_##msg, loadmeshlabel #msg)
#define loadmesh_profile_logf_scope(msg, fmt, ...) PROFILE_SCOPE_LOGF(loadmesh_##msg, loadmeshlabel fmt, __VA_ARGS__)
#else
#define loadmesh_profile_log_scope(msg) DUMMY_MACRO
#define loadmesh_profile_logf_scope(msg, fmt, ...) DUMMY_MACRO
#endif

#define LOAD_MESH_CHECK_READ_ACCESSOR
#ifdef LOAD_MESH_CHECK_READ_ACCESSOR
#define check_accessor(_condition) check((_condition))
#define check_accessor_call(_f) check((_f))
#else
#define check_accessor(_f_) DUMMY_MACRO
#define check_accessor_call(_f) expand(_f)
#endif

namespace gltf_api
{
	static void HandleError(cgltf_result result, const char* filepath)
	{
		switch (result)
		{
		case cgltf_result_success:
			break;
		case cgltf_result_data_too_short:
			break;
		case cgltf_result_unknown_format:
			break;
		case cgltf_result_invalid_json:
			break;
		case cgltf_result_invalid_gltf:
			break;
		case cgltf_result_invalid_options:
			break;
		case cgltf_result_file_not_found:
			break;
		case cgltf_result_io_error:
			break;
		case cgltf_result_out_of_memory:
			break;
		case cgltf_result_legacy_gltf:
			break;
		case cgltf_result_max_enum:
			break;
		default:
			break;
		}
	}

	static float Length2(const glm::vec3& vec)
	{
		return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
	}

	static float Length(const glm::vec3& vec) { return sqrtf(Length2(vec)); }

	static uint32_t GetElementCountFromType(cgltf_type type)
	{
		switch (type)
		{
		case cgltf_type_scalar: return 1;
		case cgltf_type_vec2: return 2;
		case cgltf_type_vec3: return 3;
		case cgltf_type_vec4: return 4;
		case cgltf_type_mat2: return 2 * 2;
		case cgltf_type_mat3: return 3 * 3;
		case cgltf_type_mat4: return 4 * 4;
		case cgltf_type_invalid:
		case cgltf_type_max_enum:
		default:
			check(false && "Invalid cgltf_type.");
			break;
		}
		return 0;
	}

	static void ReadValue(void* dst, const cgltf_float* data, uint32_t count)
	{
		memcpy_s(dst, sizeof(float) * count, data, sizeof(float) * count);
	}

	static void ToMat4(glm::mat4* mat, const cgltf_float* cgltfMat4)
	{
		ReadValue(mat, cgltfMat4, 16);
	}

	static glm::mat4 ToMat4(const cgltf_float* cgltfMat4)
	{
		glm::mat4 m;
		ToMat4(&m, cgltfMat4);
		return m;
	}

	static void ToVec2(glm::vec2& v, const cgltf_float* data)
	{
		ReadValue(&v, data, 2);
	}

	static glm::vec2 ToVec2(const cgltf_float* data)
	{
		glm::vec2 v;
		ToVec2(v, data);
		return v;
	}

	static void ToVec3(glm::vec3& v, const cgltf_float* data)
	{
		ReadValue(&v, data, 3);
	}

	static glm::vec3 ToVec3(const cgltf_float* data)
	{
		glm::vec3 v;
		ToVec3(v, data);
		return v;
	}

	static void ToVec4(glm::vec4& v, const cgltf_float* data)
	{
		ReadValue(&v, data, 4);
	}

	static glm::vec4 ToVec4(const cgltf_float* data)
	{
		glm::vec4 v;
		ToVec4(v, data);
		return v;
	}

	static void ToQuat(glm::quat& q, const cgltf_float* data)
	{
		q = glm::quat(data[3], data[0], data[1], data[2]);
	}

	static void ReadNodeLocalTransform(const cgltf_node& node, glm::mat4& t)
	{
		t = glm::mat4(1.f);
		if (node.has_matrix)
		{
			gltf_api::ToMat4(&t, node.matrix);
		}
		else
		{
			if (node.has_translation)
			{
				glm::vec3 pos;
				gltf_api::ToVec3(pos, node.translation);
				t *= glm::translate(t, pos);
			}
			if (node.has_rotation)
			{
				glm::quat quat;
				gltf_api::ToQuat(quat, node.rotation);
				t *= glm::toMat4(quat);
			}
			if (node.has_scale)
			{
				glm::vec3 scl;
				gltf_api::ToVec3(scl, node.scale);
				t = glm::scale(t, scl);
			}
		}
	}

	// Attributes are an continuous array of positions, normals, uvs...
	static void ReadAttributeArray(Mist::Vertex* vertices, const cgltf_attribute& attribute)
	{
		const cgltf_accessor* accessor = attribute.data;
		check(accessor->count < UINT32_MAX);
		uint32_t count = (uint32_t)accessor->count;
		// Get how many values has current attribute
		cgltf_size numComponents = cgltf_num_components(accessor->type);
		check_accessor(accessor->component_type == cgltf_component_type_r_32f);
        switch (attribute.type)
        {
        case cgltf_attribute_type_position:
		{
			check_accessor(accessor->has_min && accessor->has_max);
			check_accessor(accessor->type == cgltf_type_vec3 && numComponents == 3);
			check_accessor(!strcmp(attribute.name, "POSITION"));

			for (uint32_t i = 0; i < count; ++i)
				check_accessor_call(cgltf_accessor_read_float(accessor, i, &vertices[i].Position[0], numComponents));
			
            break;
		}
        case cgltf_attribute_type_texcoord:
			check_accessor(accessor->type == cgltf_type_vec2 && numComponents == 2);
			if (!strcmp(attribute.name, "TEXCOORD_0"))
			{
				for (uint32_t i = 0; i < count; ++i)
					check_accessor_call(cgltf_accessor_read_float(accessor, i, &vertices[i].TexCoords0[0], numComponents));
			}
			else if (!strcmp(attribute.name, "TEXCOORD_1"))
			{
				for (uint32_t i = 0; i < count; ++i)
					check_accessor_call(cgltf_accessor_read_float(accessor, i, &vertices[i].TexCoords1[0], numComponents));
			}
			else
				unreachable_code();
            break;
        case cgltf_attribute_type_normal:
			check_accessor(accessor->type == cgltf_type_vec3 && numComponents == 3);
			check_accessor(!strcmp(attribute.name, "NORMAL"));
			for (uint32_t i = 0; i < count; ++i)
			{
				check_accessor_call(cgltf_accessor_read_float(accessor, i, &vertices[i].Normal[0], numComponents));
				check_accessor(Length2(vertices[i].Normal) > 1e-5f);
			}
            break;
        case cgltf_attribute_type_color:
			check_accessor(accessor->type == cgltf_type_vec3 && numComponents == 3);
			check_accessor(!strcmp(attribute.name, "COLOR_0"));
            for (uint32_t i = 0; i < count; ++i)
            {
				check_accessor_call(cgltf_accessor_read_float(accessor, i, &vertices[i].Color[0], numComponents));
                check_accessor(vertices[i].Color.x >= 0.f && vertices[i].Color.x <= 1.f);
                check_accessor(vertices[i].Color.y >= 0.f && vertices[i].Color.y <= 1.f);
                check_accessor(vertices[i].Color.z >= 0.f && vertices[i].Color.z <= 1.f);
            }
            break;
        case cgltf_attribute_type_tangent:
			check_accessor(accessor->type == cgltf_type_vec4 && numComponents == 4);
			check_accessor(!strcmp(attribute.name, "TANGENT"));
            for (uint32_t i = 0; i < count; ++i)
            {
				check_accessor_call(cgltf_accessor_read_float(accessor, i, &vertices[i].Tangent[0], numComponents));
				check_accessor(vertices[i].Tangent.w == -1.f || vertices[i].Tangent.w == 1.f);
            }
            break;
        case cgltf_attribute_type_invalid:
        case cgltf_attribute_type_custom:
        case cgltf_attribute_type_max_enum:
        default:
            check(false && "Invalid attribute type to read.");
            break;
        }
	}

	static void FreeData(cgltf_data* data)
	{
		cgltf_free(data);
	}

	static cgltf_data* ParseFile(const char* filepath)
	{
		char assetPath[Mist::MaxFilenameLength];
		Mist::FileSystem::BuildFilepathInWorkspace(filepath, assetPath, sizeof(assetPath));
		cgltf_options options;
		memset(&options, 0, sizeof(cgltf_options));
		cgltf_data* data{ nullptr };
		cgltf_result result = cgltf_parse_file(&options, assetPath, &data);
		if (result != cgltf_result_success)
		{
			HandleError(result, assetPath);
			return nullptr;
		}
		result = cgltf_load_buffers(&options, data, assetPath);
		if (result != cgltf_result_success)
		{
			HandleError(result, assetPath);
			return nullptr;
		}
		result = cgltf_validate(data);
		if (result != cgltf_result_success)
		{
			HandleError(result, assetPath);
			FreeData(data);
			return nullptr;
		}
		return data;
	}

	static void LoadVertices(const cgltf_primitive& primitive, Mist::Vertex* verticesOut, uint32_t vertexCount)
	{
		uint32_t attributeCount = (uint32_t)primitive.attributes_count;
		check(primitive.attributes[0].data->count < UINT32_MAX);
		uint32_t accessorCount = (uint32_t)primitive.attributes[0].data->count;
		for (uint32_t i = 0; i < attributeCount; ++i)
		{
			const cgltf_attribute& attribute = primitive.attributes[i];
			check(attribute.data->count <= vertexCount);
			check(attribute.data->count == accessorCount);
			ReadAttributeArray(verticesOut, attribute);
		}
	}

	static void LoadIndices(const cgltf_primitive& primitive, uint32_t* indicesOut, uint32_t offset)
	{
		check(primitive.indices && primitive.type == cgltf_primitive_type_triangles);
		check(primitive.indices->count < UINT32_MAX);
		uint32_t indexCount = (uint32_t)primitive.indices->count;
		for (uint32_t i = 0; i < indexCount; ++i)
			indicesOut[i] = (uint32_t)cgltf_accessor_read_index(primitive.indices, i) + offset;
	}

	static render::Filter GetSamplerMagFilter(int filter)
	{
		switch (filter)
		{
		case GLTF_MAG_FILTER_NEAREST:
			return render::Filter_Nearest;
		case GLTF_MAG_FILTER_LINEAR:
			return render::Filter_Linear;
		}
		return render::Filter_Linear;
	}

	static void GetSamplerMinFilterAndMipmapMode(int mode, render::Filter* minFilterOut, render::Filter* mipmapModeOut)
	{
		switch (mode)
		{
		case GLTF_MIN_FILTER_NEAREST:
			*minFilterOut = render::Filter_Nearest;
			break;
		case GLTF_MIN_FILTER_LINEAR:
			*minFilterOut = render::Filter_Linear;
			break;
		case GLTF_MIN_FILTER_LINEAR_MIPMAP_NEAREST:
			*minFilterOut = render::Filter_Linear;
			*mipmapModeOut = render::Filter_Nearest;
			break;
		case GLTF_MIN_FILTER_LINEAR_MIPMAP_LINEAR:
			*minFilterOut = render::Filter_Linear;
			*mipmapModeOut = render::Filter_Linear;
			break;
		case GLTF_MIN_FILTER_NEAREST_MIPMAP_NEAREST:
			*minFilterOut = render::Filter_Nearest;
			*mipmapModeOut = render::Filter_Nearest;
			break;
		case GLTF_MIN_FILTER_NEAREST_MIPMAP_LINEAR:
			*minFilterOut = render::Filter_Nearest;
			*mipmapModeOut = render::Filter_Linear;
			break;
		}
	}

	static render::SamplerAddressMode GetSamplerAddressMode(int mode)
	{
		switch (mode)
		{
		case GLTF_WRAPPER_CLAMP_TO_EDGE: return render::SamplerAddressMode_ClampToEdge;
		case GLTF_WRAPPER_MIRRORED_REPEAT: return render::SamplerAddressMode_MirrorRepeat;
		case GLTF_WRAPPER_REPEAT: return render::SamplerAddressMode_Repeat;
		}
		return render::SamplerAddressMode_Repeat;
	}

	static render::SamplerHandle LoadSampler(render::Device* device, const cgltf_sampler* sampler)
	{
		render::SamplerDescription desc;
		GetSamplerMinFilterAndMipmapMode(sampler->min_filter, &desc.minFilter, &desc.mipmapMode);
		desc.magFilter = GetSamplerMagFilter(sampler->mag_filter);
		desc.addressModeU = GetSamplerAddressMode(sampler->wrap_s);
		desc.addressModeV = GetSamplerAddressMode(sampler->wrap_t);
		desc.addressModeW = GetSamplerAddressMode(sampler->wrap_t);
		if (sampler->name && sampler->name[0])
			desc.debugName = sampler->name;
		return device->CreateSampler(desc);
	}

	static bool LoadTexture(render::Device* device, const char* rootAssetPath, const cgltf_texture_view& texView, render::TextureHandle* texOut, render::SamplerHandle* samplerOut)
	{
		if (!texView.texture)
			return false;
		check(texView.texcoord == 0);
		check(texView.scale == 1.f && !texView.has_transform);
		char texturePath[512];
		sprintf_s(texturePath, "%s%s", rootAssetPath, texView.texture->image->uri);
		check(rendersystem::textureloader::LoadTextureFromFile(texOut, device, texturePath));
		if (texView.texture->sampler)
			*samplerOut = LoadSampler(device, texView.texture->sampler);
		loadmeshlogf("Load texture: %s\n", texView.texture->image->uri);
		return true;
	}

	static bool LoadTexture(render::Device* device, const char* rootAssetPath, const cgltf_texture_view& texView, Mist::Texture** texOut, render::SamplerHandle* samplerOut, render::Format format)
	{
		check(texOut && !*texOut);
		if (!texView.texture)
			return false;
		check(texView.texcoord == 0);

		// Get full texture path
		char texturePath[Mist::MaxFilenameLength];
		sprintf_s(texturePath, "%s%s", rootAssetPath, texView.texture->image->uri);

		if (texView.has_transform)
			logfwarn("Texture view with transform: %s (Not supported yet)\n", texturePath);

		// Create and load texture
		Mist::Texture::TextureLoader::LoadParams loadParams;
		loadParams.format = format;
		loadParams.calculateMipLevels = true;
		loadParams.flipVertical = false;
		strcpy_s(loadParams.filepath, texturePath);
		(*texOut) = _new Mist::Texture();
		(*texOut)->LoadFromFile(loadParams);

		// Load sampler
		if (texView.texture->sampler)
			*samplerOut = LoadSampler(device, texView.texture->sampler);

		loadmeshlogf("Load texture: %s\n", texView.texture->image->uri);
		return true;
	}

	static bool LoadTexture(render::Device* device, const char* rootAssetPath, const cgltf_texture_view& gltftextureView, Mist::eMaterialTexture textureType, Mist::cMaterial& material)
	{
		Mist::Texture* texture = nullptr;
		render::SamplerHandle sampler = nullptr;
		const bool ret = LoadTexture(device, rootAssetPath, gltftextureView, &texture, &sampler, Mist::GetMaterialTextureFormat(textureType));
		material.SetTexture(textureType, texture);
		material.SetSampler(textureType, sampler);
		return ret;
	}

	template <typename T>
	static inline Mist::index_t GetArrayElementOffset(const T* root, const T* item) { check(item >= root); return Mist::index_t(item - root); }

	static void LoadMaterial(Mist::cMaterial& material, render::Device* device, const cgltf_material& cgltfmtl, const char* rootAssetPath)
	{
		Mist::tMaterialFlags flags = Mist::MATERIAL_FLAG_NONE;
		// Emissive
		if (cgltfmtl.has_emissive_strength)
		{
			material.SetEmissiveColor(ToVec3(cgltfmtl.emissive_factor));
			material.SetEmissiveStrength(cgltfmtl.emissive_strength.emissive_strength);

			LoadTexture(device, rootAssetPath, cgltfmtl.emissive_texture, Mist::MATERIAL_TEXTURE_EMISSIVE, material);
		}

		// Metallic roughness
		if (cgltfmtl.has_pbr_metallic_roughness)
		{
			material.SetMetallic(cgltfmtl.pbr_metallic_roughness.metallic_factor);
			material.SetRoughness(cgltfmtl.pbr_metallic_roughness.roughness_factor);

			LoadTexture(device, rootAssetPath, cgltfmtl.pbr_metallic_roughness.metallic_roughness_texture, Mist::MATERIAL_TEXTURE_METALLIC_ROUGHNESS, material);
		}

		// Specular
		if (cgltfmtl.has_specular)
		{
			material.SetSpecular(cgltfmtl.specular.specular_factor);

			if (LoadTexture(device, rootAssetPath, cgltfmtl.specular.specular_texture, Mist::MATERIAL_TEXTURE_SPECULAR, material))
				check(!material.GetTexture(Mist::MATERIAL_TEXTURE_METALLIC_ROUGHNESS));
		}

		if (cgltfmtl.has_pbr_specular_glossiness)
			check(false && "has pbr specular glossiness");

		// Unlit
		if (cgltfmtl.unlit)
			flags |= Mist::MATERIAL_FLAG_UNLIT;

		// Normal
		LoadTexture(device, rootAssetPath, cgltfmtl.normal_texture, Mist::MATERIAL_TEXTURE_NORMAL, material);

		// Albedo
		material.SetAlbedo(ToVec4(cgltfmtl.pbr_metallic_roughness.base_color_factor));
		LoadTexture(device, rootAssetPath, cgltfmtl.pbr_metallic_roughness.base_color_texture, Mist::MATERIAL_TEXTURE_ALBEDO, material);

		// Alpha cutoff
		check_accessor(cgltfmtl.alpha_cutoff >= 0.f);
		material.SetAlphaCutoff(cgltfmtl.alpha_cutoff);

		// Alpha mode
		switch (cgltfmtl.alpha_mode)
		{
		case cgltf_alpha_mode_opaque: flags |= Mist::MATERIAL_FLAG_OPAQUE; material.SetAlphaCutoff(1.f); break;
		case cgltf_alpha_mode_mask: flags |= Mist::MATERIAL_FLAG_MASK; break;
		case cgltf_alpha_mode_blend: flags |= Mist::MATERIAL_FLAG_BLEND; break;
		}

		material.SetFlags(flags);
	}

}

namespace Mist
{
	static const char* g_validModelExtensions[] =
	{
		".gltf",
		".glb", 
	};
	
	static void CalculateTangent(glm::vec4& t, const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2)
	{
		glm::vec3 e0 = p1 - p0;
		glm::vec3 e1 = p2 - p0;
		glm::vec2 duv0 = uv1 - uv0;
		glm::vec2 duv1 = uv2 - uv0;
		float f = 1.f / (duv0.x * duv1.y - duv1.x * duv0.y);

		t.x = f * (duv1.y * e0.x - duv0.y * e1.x);
		t.y = f * (duv1.y * e0.y - duv0.y * e1.y);
		t.z = f * (duv1.y * e0.z - duv0.y * e1.z);
		t.w = 1.f;
	}

	static void BuildTangents(Vertex* vertices, lindex_t vertexCount, lindex_t* indices, lindex_t indexCount)
	{
		check(indexCount % 3 == 0);
		check(vertices && vertexCount && indices);
		for (lindex_t i = 0; i < indexCount; i+=3)
		{
			Vertex& v0 = vertices[indices[i]];
			Vertex& v1 = vertices[indices[i+1]];
			Vertex& v2 = vertices[indices[i+2]];
			glm::vec4 t(0.f);
			CalculateTangent(t, v0.Position, v1.Position, v2.Position, v0.TexCoords0, v1.TexCoords0, v2.TexCoords0);
			t = glm::normalize(t);
			v0.Tangent = t;
			v1.Tangent = t;
			v2.Tangent = t;
		}
	}
	
	static bool ValidateModelExtension(const char* filepath)
	{
		char extension[8];
		Mist::FileSystem::GetFileExtension(filepath, extension, Mist::CountOf(extension));
		for (uint32_t i =0; i < Mist::CountOf(g_validModelExtensions); ++i)
		{
			if (!strcmp(g_validModelExtensions[i], extension))
				return true;
		}
		return false;
	}

	void cModel::Destroy()
	{
		for (index_t i = 0; i < m_meshes.GetSize(); ++i)
			m_meshes[i].Destroy();
		for (index_t i = 0; i < m_materials.GetSize(); ++i)
			m_materials[i].Invalidate();
		m_nodes.Delete();
		m_meshes.Delete();
		m_materials.Delete();
		m_transforms.Delete();
		m_root = index_invalid;
	}

	bool cModel::LoadModel(render::Device* device, const char* filepath)
	{
		PROFILE_SCOPE_LOGF(LoadModel, "Load model (%s)", filepath);
		check(m_materials.IsEmpty() && m_meshes.IsEmpty());
		
		if (!ValidateModelExtension(filepath))
		{
			logferror("Model file extension not recognized: %s.\n", filepath);
			return false;
		}
		
		cgltf_data* data = gltf_api::ParseFile(filepath);
		if (!data)
		{
			logferror("Cannot open file to load scene model: %s.\n", filepath);
			return false;
		}
		if (!data->nodes_count)
		{
			logferror("Model file without nodes in scene: %s.\n", filepath);
			gltf_api::FreeData(data);
			return false;
		}
		SetName(filepath);

		char rootFilePath[512];
		FileSystem::GetDirectoryFromFilepath(filepath, rootFilePath, 512);
		
		char nameFile[256];
		*nameFile = 0;
		FileSystem::GetFileNameFromFilepath(filepath, nameFile, sizeof(nameFile));
		check(*nameFile);

		loadmeshlog("=== Begin loading model ===\n");
		loadmeshlogf("Loading model from file: %s\n", filepath);
		loadmeshlogf("* nodes:		%4d\n", data->nodes_count);
		loadmeshlogf("* materials:	%4d\n", data->materials_count);
		loadmeshlogf("* meshes:		%4d\n", data->meshes_count);
		loadmeshlogf("* textures:	%4d\n", data->textures_count);

		{
			loadmesh_profile_logf_scope(LoadMaterials, "Load materials (%s)(%d)", filepath, data->materials_count);
			if (data->materials_count)
			{
				char mtlFilepath[256];
				sprintf_s(mtlFilepath, "%s%s.mtl", rootFilePath, nameFile);
				uint32_t materialsCount = UINT32_MAX;
				cMaterial* materials = nullptr;
				if (!cMaterial::UnserializeMaterials(mtlFilepath, materials, materialsCount))
				{
					InitMaterials((index_t)data->materials_count);
					for (uint32_t i = 0; i < data->materials_count; ++i)
					{
						m_materials[i].SetName(data->materials[i].name && *data->materials[i].name ? data->materials[i].name : "unknown");					
						gltf_api::LoadMaterial(m_materials[i], device, data->materials[i], rootFilePath);
						m_materials[i].SetupShader(g_render);
					}
					
					check(cMaterial::SerializeMaterials(mtlFilepath, m_materials.GetData(), m_materials.GetSize()));
				}
				else
				{
					check(materialsCount == data->materials_count);
					m_materials = Mist::tFixedHeapArray<cMaterial>(&materials, &materialsCount);
				}	
			}
			else
			{
				logfwarn("Model without materials: %s\n", filepath);
				InitMaterials(1);
				m_materials[0] = *cMaterial::GetDefaultMaterial();
			}
		}

		{
			loadmesh_profile_logf_scope(LoadMesh, "Load meshes (%s)(%d)", filepath, data->meshes_count);
			InitNodes((index_t)data->nodes_count);
			InitMeshes((index_t)data->meshes_count);
			m_aabb = AABB_t::InvalidAABB();
			m_renderPassMask = RenderPass_None;

			tDynArray<Vertex> tempVertices;
			tDynArray<uint32_t> tempIndices;
			for (index_t i = 0; i < (index_t)data->nodes_count; ++i)
			{
				const cgltf_node& node = data->nodes[i];
				index_t parentIndex = index_invalid;
				if (node.parent)
					parentIndex = gltf_api::GetArrayElementOffset(data->nodes, node.parent);
				index_t nodeIndex = BuildNode(i, parentIndex, node.name);

				// Process transform
				gltf_api::ReadNodeLocalTransform(node, m_transforms[i]);
				glm::vec3 pos, rot, scl;
				math::DecomposeMatrix(m_transforms[i], pos, rot, scl);
				loadmeshlogf("node %4d %s child of %4d\n", i, m_nodeNames[i].CStr(), parentIndex);
				loadmeshlogf("node %4d %s [pos %4.3f, %4.3f, %4.3f][rot %2.3f, %2.3f, %2.3f][scl %2.3f, %2.3f, %2.3f]\n", i, m_nodeNames[i].CStr(),
					pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, scl.x, scl.y, scl.z);

				// Process mesh
				if (node.mesh)
				{
					check(node.mesh >= data->meshes);
					index_t meshIndex = Mist::limits_cast<index_t>(uint64_t(node.mesh - data->meshes));
					check(meshIndex < m_meshes.GetSize());

					LinkNodeToMesh(nodeIndex, meshIndex);

					cMesh& mesh = m_meshes[meshIndex];
					
					// mesh could be previously loaded in another node. 
					if (mesh.GetIndexCount())
						continue;

					// Do actual mesh loading.
					mesh.SetRenderPassMask(RenderPass_None);
					mesh.SetName(node.mesh->name && *node.mesh->name ? node.mesh->name : "unknown");
					loadmeshlogf("node %d %s has mesh %s\n", i, m_nodeNames[i].CStr(), mesh.GetName());

					mesh.SetAABB(AABB_t::InvalidAABB());
					mesh.InitPrimitives(node.mesh->primitives_count);
					check(mesh.GetPrimitiveCount() <= node.mesh->primitives_count);
					loadmeshlogf("* primitives: %d\n", node.mesh->primitives_count);

					// Get vertex attribute mask. All primitives should have the same mask as the first one
					VertexAttributeMask mask = VertexAttribute_None;
					{
						const cgltf_primitive& primitive = node.mesh->primitives[0];
						for (uint32_t j = 0; j < primitive.attributes_count; ++j)
						{
							switch (primitive.attributes[j].type)
							{
							case cgltf_attribute_type_position: mask |= VertexAttribute_Position; break;
							case cgltf_attribute_type_normal: mask |= VertexAttribute_Normal; break;
							case cgltf_attribute_type_tangent: mask |= VertexAttribute_Tangent; break;
							case cgltf_attribute_type_texcoord: 
								if (!strcmp(primitive.attributes[j].name, "TEXCOORD_0"))
									mask |= VertexAttribute_TexCoord0;
								else if (!strcmp(primitive.attributes[j].name, "TEXCOORD_1"))
									mask |= VertexAttribute_TexCoord1;
								else
									unreachable_code();
								break;
							case cgltf_attribute_type_color: mask |= VertexAttribute_Color; break;
							case cgltf_attribute_type_joints:
							case cgltf_attribute_type_weights:
							case cgltf_attribute_type_custom:
							case cgltf_attribute_type_invalid:
							case cgltf_attribute_type_max_enum:
							default:
								unreachable_code();
								break;
							}
						}
					}

					// Load primitives
					for (uint32_t j = 0; j < node.mesh->primitives_count; ++j)
					{
						const cgltf_primitive& cgltfprimitive = node.mesh->primitives[j];
						PrimitiveMeshData& primitive = mesh.GetPrimitiveArray()[j];

						check(cgltfprimitive.type == cgltf_primitive_type_triangles);
						check(cgltfprimitive.indices && cgltfprimitive.indices->type == cgltf_type_scalar && cgltfprimitive.indices->count % 3 == 0);
						check(cgltfprimitive.attributes && cgltfprimitive.attributes->data);

						// Reserve size in temporal buffers
						uint32_t indexCount = (uint32_t)cgltfprimitive.indices->count;
						uint32_t vertexCount = (uint32_t)cgltfprimitive.attributes[0].data->count;
						uint32_t vertexOffset = (uint32_t)tempVertices.size();
						uint32_t indexOffset = (uint32_t)tempIndices.size();
						tempIndices.resize(indexOffset + indexCount);
						tempVertices.resize(vertexOffset + vertexCount);
						loadmeshlogf("** primitive %2d: [vertices %6d | %6d bytes][indices %4d | %6d bytes]\n",
							j, vertexCount, sizeof(Vertex) * vertexCount, indexCount, sizeof(uint32_t) * indexCount);

						// Read gltf primitive in temporal buffers
						gltf_api::LoadIndices(cgltfprimitive, tempIndices.data() + indexOffset, vertexOffset);
						gltf_api::LoadVertices(cgltfprimitive, tempVertices.data() + vertexOffset, vertexCount);

						// Calculate AABB
						// calculate min and max of vertices in mesh space. After load all vertices and nodes, aabb will be transformed to model space.
						// only calculate primitives bounding boxes.
						primitive.aabb = AABB_t::InvalidAABB();
						const Vertex* vertices = tempVertices.data() + vertexOffset;
						for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
							primitive.aabb = primitive.aabb.Join(vertices[vertexIndex].Position, vertices[vertexIndex].Position);

						// Acumulate aabb in mesh
						mesh.SetAABB(mesh.GetAABB().Join(primitive.aabb));

						// Set primitive
						primitive.renderPassMask = 0;
						primitive.firstIndex = indexOffset;
						primitive.count = indexCount;
						if (cgltfprimitive.material)
						{
							index_t materialIndex = gltf_api::GetArrayElementOffset(data->materials, cgltfprimitive.material);
							check(materialIndex < m_materials.GetSize());
							cMaterial* material = &m_materials[materialIndex];
							primitive.material = material;
							if (!(material->GetFlags() & MATERIAL_FLAG_NO_PROJECT_SHADOWS))
								primitive.renderPassMask |= RenderPass_ShadowMap;
							//if (material->m_flags & MATERIAL_FLAG_EMISSIVE)
							//	primitive.RenderFlags |= RenderFlags_Emissive;
							if (material->GetFlags() & (MATERIAL_FLAG_OPAQUE | MATERIAL_FLAG_MASK))
								primitive.renderPassMask |= RenderPass_Opaque;
							if (material->GetFlags() & MATERIAL_FLAG_BLEND)
								primitive.renderPassMask |= RenderPass_Transparent;
						}
						else
						{
							logfwarn("Primitive mesh without material: %s (Primitive %d)\n", mesh.GetName(), j);
							check(!m_materials.IsEmpty());
							primitive.material = &m_materials[0];
						}
						mesh.SetRenderPassMask(mesh.GetRenderPassMask() | primitive.renderPassMask);

						check(cgltfprimitive.indices->count == primitive.count);
					}

					loadmeshlogf("* mesh %d: %d vertices (%lld b), %d indices (%lld b), render mask %d\n",
						meshIndex, tempVertices.size(), tempVertices.size() * sizeof(Vertex), tempIndices.size(), tempIndices.size() * sizeof(uint32_t), mesh.GetRenderPassMask());

					if (!(mask & VertexAttribute_Tangent))
					{
						logfwarn("Mesh %s from model %s without tangent vertices.\n", mesh.GetName(), GetName());
						BuildTangents(tempVertices.data(), tempVertices.size(), tempIndices.data(), tempIndices.size());
					}

					// Create mesh resources.
					mesh.InitBuffers(device, tempVertices.data(), tempVertices.size() * sizeof(Vertex), tempIndices.data(), tempIndices.size());

					// Update model render pass from mesh render pass mask
					m_renderPassMask |= mesh.GetRenderPassMask();

					// Clear temp buffers without release memory
					tempIndices.clear();
					tempVertices.clear();

					m_aabb = m_aabb.Join(mesh.GetAABB());
				}
				else
					loadmeshlogf("node %d %s has no mesh\n", i, m_nodeNames[i].CStr());
			}

			// Transform AABB to model space to calculate model AABB
			{
				glm::mat4* modelTransforms = _new glm::mat4[m_transforms.GetSize()];
				UpdateRenderTransforms(modelTransforms, glm::mat4(1.f));
				m_aabb.Invalidate();
				for (uint32_t i = 0; i < m_nodeMeshInfoArray.GetSize(); ++i)
				{
					const NodeMeshInfo& meshInfo = m_nodeMeshInfoArray[i];
					check(meshInfo.IsValid());
					const cMesh& mesh = m_meshes[meshInfo.mesh];
					m_aabb = m_aabb.Join(mesh.GetAABB().ApplyTransform(modelTransforms[meshInfo.node]));
				}
				delete[] modelTransforms;
			}
		}
		loadmeshlog("=== End loading model ===\n\n");
		gltf_api::FreeData(data);


		return true;
	}

	void cModel::UpdateRenderTransforms(glm::mat4* globalTransforms, const glm::mat4& worldTransform) const
	{
		check(m_root != index_invalid);
		globalTransforms[m_root] = worldTransform * m_transforms[m_root];
		check(m_nodes[m_root].Sibling == index_invalid);
		if (m_nodes[m_root].Child != index_invalid)
			CalculateGlobalTransforms(globalTransforms, m_nodes[m_root].Child);
	}

	void cModel::UpdateMaterials(sMaterialRenderData* materials) const
	{
		for (index_t i = 0; i < m_materials.GetSize(); ++i)
		{
			materials[i] = m_materials[i].GetRenderData();
		}
	}

	void cModel::InitNodes(index_t n)
	{
		// Add model root node
		++n;
		check(m_nodes.IsEmpty());
		m_nodes.Allocate(n);
		m_nodes.Resize(n);

		check(m_nodeNames.IsEmpty());
		m_nodeNames.Allocate(n);
		m_nodeNames.Resize(n);

		check(m_transforms.IsEmpty());
		m_transforms.Allocate(n);
		m_transforms.Resize(n);

		check(m_nodeMeshInfoArray.IsEmpty());
		m_nodeMeshInfoArray.Allocate(n);

		// Build default root node
		m_root = n - 1;
		Node* node = GetNode(m_root);
		check(node);
		SetNodeName(m_root, "_root_");
		SetNodeTransform(m_root, glm::mat4(1.f));
	}

	void cModel::InitMeshes(index_t n)
	{
		check(m_meshes.IsEmpty());
		m_meshes.Allocate(n);
		m_meshes.Resize(n);
	}

	void cModel::InitMaterials(index_t n)
	{
		check(m_materials.IsEmpty());
		m_materials.Allocate(n);
		m_materials.Resize(n);
	}

	void cModel::CalculateGlobalTransforms(glm::mat4* transforms, index_t node) const
	{
		check(node != index_invalid);
		const Node& n = m_nodes[node];

		// calculate current
		if (n.Parent != index_invalid)
			transforms[node] = transforms[n.Parent] * m_transforms[node];
		else
			transforms[node] = m_transforms[node];

		// calculate siblings
		if (n.Sibling != index_invalid)
			CalculateGlobalTransforms(transforms, n.Sibling);

		// calculate child
		if (n.Child != index_invalid)
			CalculateGlobalTransforms(transforms, n.Child);
	}

	index_t cModel::BuildNode(index_t nodeIndex, index_t parentIndex, const char* nodeName)
	{
		Node& node = m_nodes[nodeIndex];
		SetNodeName(nodeIndex, nodeName && *nodeName ? nodeName : "unknown");
		Node* nodeParent = GetNode(parentIndex);
		if (!nodeParent)
		{
#if 0
			if (m_root == index_invalid)
				m_root = nodeIndex;
			else
#endif // 0
			{
				check(m_root != index_invalid);
				Logf(LogLevel::Warn, "Node (%d %s) of scene has no parent after found a root node (%d %s) in scene graph: %s\n",
					nodeIndex, m_nodeNames[nodeIndex].CStr(), m_root, m_nodeNames[m_root].CStr(), GetName());
				nodeParent = GetNode(m_root);
				parentIndex = m_root;
			}
		}

		if (nodeParent)
		{
			if (nodeParent->Child != index_invalid)
			{
				Node* sibling = GetNode(nodeParent->Child);
				for (; sibling->Sibling != index_invalid; sibling = GetNode(sibling->Sibling));
				check(sibling && sibling->Sibling == index_invalid);
				sibling->Sibling = nodeIndex;
			}
			else
				nodeParent->Child = nodeIndex;
		}
		node.Parent = parentIndex;
		return nodeIndex;
	}

	cModel::Node* cModel::GetNode(index_t i)
	{
		if (i < m_nodes.GetSize())
			return &m_nodes[i];
		return nullptr;
	}

	void cModel::SetNodeName(index_t i, const char* name)
	{
		check(i < m_nodeNames.GetSize());
		m_nodeNames[i] = name;
	}

	void cModel::SetNodeTransform(index_t i, const glm::mat4& transform)
	{
		check(i < m_transforms.GetSize());
		m_transforms[i] = transform;
	}

	index_t cModel::CreateMesh()
	{
		check(m_meshes.GetReservedSize());
		m_meshes.Push();
		return m_meshes.GetSize() - 1;
	}

	index_t cModel::CreateMaterial()
	{
		check(!m_materials.IsEmpty());
		m_materials.Push();
		return m_materials.GetSize() - 1;
	}

	index_t cModel::LinkNodeToMesh(index_t node, index_t meshId)
	{
		check(node < m_nodes.GetSize());
		check(meshId < m_meshes.GetSize());

		// Create new NodeMeshInfo and fill data with indices
		m_nodeMeshInfoArray.Push();
		NodeMeshInfo& nodeMeshInfo = m_nodeMeshInfoArray.Back();
		nodeMeshInfo.node = node;
		nodeMeshInfo.mesh = meshId;

		// Link node to new NodeMeshInfo
		check(m_nodes[node].meshInfoIndex == index_invalid);
		m_nodes[node].meshInfoIndex = m_nodeMeshInfoArray.GetSize() - 1;
		return m_nodeMeshInfoArray.GetSize() - 1;
	}

	void cModel::DumpInfo() const
	{
		index_t it = m_root;
		while (it != index_invalid)
		{
			const Node& node = m_nodes[it];
			logfinfo("* node: %d (%s) (Parent: %d)\n", it, m_nodeNames[it].CStr(), node.Parent);
			logfinfo("* mesh: %d\n", node.meshInfoIndex != index_invalid ? m_nodeMeshInfoArray[node.meshInfoIndex].mesh : -1);
			logfinfo("** transform: \n");
			const glm::mat4& m = m_transforms[it];
			logfinfo("** [%6.3f %6.3f %6.3f %6.3f]\n", m[0][0], m[1][0], m[2][0], m[3][0]);
			logfinfo("** [%6.3f %6.3f %6.3f %6.3f]\n", m[0][1], m[1][1], m[2][1], m[3][1]);
			logfinfo("** [%6.3f %6.3f %6.3f %6.3f]\n", m[0][2], m[1][2], m[2][2], m[3][2]);
			logfinfo("** [%6.3f %6.3f %6.3f %6.3f]\n", m[0][3], m[1][3], m[2][3], m[3][3]);

			if (node.Child != index_invalid)
				it = node.Child;
			else if (node.Sibling != index_invalid)
				it = node.Sibling;
			else if (node.Parent != index_invalid)
			{
				const Node& parent = m_nodes[node.Parent];
				if (parent.Sibling != index_invalid)
					it = parent.Sibling;
				else
					it = index_invalid;
			}
			else
				it = index_invalid;
		}
	}
}