// src file for vkmmc project 

#include "SceneLoader.h"

#define CGLTF_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)
#include <gltf/cgltf.h>
#undef _CRT_SECURE_NO_WARNINGS
#undef CGLTF_IMPLEMENTATION

#include "Debug.h"
#include <glm/gtx/transform.hpp>
#include "Mesh.h"
#include "GenericUtils.h"
#include "RenderEngine.h"

namespace gltf_api
{
	void HandleError(cgltf_result result, const char* filepath)
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

	float Length2(const glm::vec3& vec)
	{
		return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
	}

	float Length(const glm::vec3& vec) { return sqrtf(Length2(vec)); }

	uint32_t GetElementCountFromType(cgltf_type type)
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

	void ReadValues(std::vector<float>& values, const cgltf_accessor& accessor)
	{
		uint32_t elementCount = GetElementCountFromType(accessor.type);
		values.resize(accessor.count * elementCount);
		for (uint32_t i = 0; i < accessor.count; ++i)
			cgltf_accessor_read_float(&accessor, i, &values[i * elementCount], elementCount);
	}

	void ReadAttribute(vkmmc::Vertex& vertex, const float* source, uint32_t index, cgltf_attribute_type type)
	{
		const char* attributeName = nullptr;
		switch (type)
		{
		case cgltf_attribute_type_position:
			vertex.Position = glm::vec3(source[index], source[index + 1], source[index + 2]);
			break;
		case cgltf_attribute_type_texcoord:
			vertex.TexCoords = glm::vec2(source[index], source[index + 1]);
			break;
		case cgltf_attribute_type_normal:
#if 0
			vertex.Normal = glm::vec3(source[index], source[index + 1], source[index + 2]);
			if (Length2(vertex.Normal) < 1e-5f)
				vertex.Normal = glm::vec3{ 0.f, 1.f, 0.f };
			else
				vertex.Normal = glm::normalize(vertex.Normal);
			break;
#endif // 0
		case cgltf_attribute_type_color: 
			vertex.Color = glm::vec3(source[index], source[index + 1], source[index + 2]);
			break;
		case cgltf_attribute_type_tangent: attributeName = "tangent"; break;
		case cgltf_attribute_type_joints: attributeName = "joints"; break;
		case cgltf_attribute_type_weights: attributeName = "weights"; break;
		case cgltf_attribute_type_invalid:
		case cgltf_attribute_type_custom:
		case cgltf_attribute_type_max_enum:
			check(false && "Invalid attribute type to read.");
		default:
			break;
		}
		//if (attributeName)
		//	vkmmc::Logf(vkmmc::LogLevel::Error, "gltf loader: Attribute type not suported yet [%s].\n", attributeName);
	}

	// Attributes are an continuous array of positions, normals, uvs...
	// We have to map from struct of arrays to our format, array of structs (std::vector<vkmmc::Vertex>)
	void ReadAttributeArray(std::vector<vkmmc::Vertex>& vertices, const cgltf_attribute& attribute, const cgltf_node* nodes, uint32_t nodeCount)
	{
		const cgltf_accessor* accessor = attribute.data;
		uint32_t accessorCount = (uint32_t)accessor->count;
		check(accessorCount == vertices.size() && "Vertices must be preallocated.");
		// Get how many values has current attribute
		uint32_t elementCount = GetElementCountFromType(accessor->type);
		// Read data from accessor
		std::vector<float> values;
		ReadValues(values, *accessor);
		// Map to internal format
		for (uint32_t i = 0; i < accessorCount; ++i)
		{
			vkmmc::Vertex& vertex = vertices[i];
			uint32_t indexValue = i * elementCount;
			ReadAttribute(vertex, values.data(), indexValue, attribute.type);
		}
	}

	void FreeData(cgltf_data* data)
	{
		cgltf_free(data);
	}

	cgltf_data* ParseFile(const char* filepath)
	{
		cgltf_options options;
		memset(&options, 0, sizeof(cgltf_options));
		cgltf_data* data{nullptr};
		cgltf_result result = cgltf_parse_file(&options, filepath, &data);
		if (result != cgltf_result_success)
		{
			HandleError(result, filepath);
			return nullptr;
		}
		result = cgltf_load_buffers(&options, data, filepath);
		if (result != cgltf_result_success)
		{
			HandleError(result, filepath);
			return nullptr;
		}
		result = cgltf_validate(data);
		if (result != cgltf_result_success)
		{
			HandleError(result, filepath);
			FreeData(data);
			return nullptr;
		}
		return data;
	}

	void LoadMesh(std::vector<vkmmc::Vertex>& vertices, std::vector<uint32_t>& indices, const cgltf_primitive* primitive, const cgltf_node* nodes, uint32_t nodeCount)
	{
		uint32_t attributeCount = (uint32_t)primitive->attributes_count;
		vertices.resize(primitive->attributes[0].data->count);
		for (uint32_t i = 0; i < attributeCount; ++i)
		{
			const cgltf_attribute& attribute = primitive->attributes[i];
			ReadAttributeArray(vertices, attribute, nodes, nodeCount);
		}

		if (primitive->indices)
		{
			uint32_t indexCount = (uint32_t)primitive->indices->count;
			indices.resize(indexCount);
			for (uint32_t i = 0; i < indexCount; ++i)
			{
				indices[i] = (uint32_t)cgltf_accessor_read_index(primitive->indices, i);
			}
		}
	}

	vkmmc::RenderHandle LoadTexture(vkmmc::IRenderEngine* engine, const char* rootAssetPath, const cgltf_texture_view& texView)
	{
		char texturePath[512];
		sprintf_s(texturePath, "%s%s", rootAssetPath, texView.texture->image->uri);
		vkmmc::RenderHandle handle = engine->LoadTexture(texturePath);
		return handle;
	}

	void LoadMaterial(vkmmc::IRenderEngine* engine, const char* rootAssetPath, vkmmc::Material& material, const cgltf_primitive& primitive)
	{
		if (primitive.material)
		{
			const cgltf_material& mtl = *primitive.material;
			if (mtl.has_pbr_metallic_roughness)
			{
				vkmmc::RenderHandle diffHandle = LoadTexture(engine,
					rootAssetPath,
					mtl.pbr_metallic_roughness.base_color_texture);
				material.SetDiffuseTexture(diffHandle);
			}
			else
				vkmmc::Log(vkmmc::LogLevel::Warn, "Diffuse material texture not found.\n");
			if (mtl.has_pbr_specular_glossiness)
			{
				vkmmc::RenderHandle handle = LoadTexture(engine,
					rootAssetPath, 
					mtl.pbr_specular_glossiness.diffuse_texture);
				material.SetSpecularTexture(handle);
			}
			else
				vkmmc::Log(vkmmc::LogLevel::Warn, "Specular material texture not found.\n");
			if (mtl.normal_texture.texture)
			{
				vkmmc::RenderHandle handle = LoadTexture(engine,
					rootAssetPath, 
					mtl.normal_texture);
				material.SetNormalTexture(handle);
			}
			else
				vkmmc::Log(vkmmc::LogLevel::Warn, "Normal material texture not found.\n");
		}
	}
}

namespace vkmmc
{
	Scene SceneLoader::LoadScene(IRenderEngine* engine, const char* sceneFilepath)
	{
		Scene scene;
		cgltf_data* data = gltf_api::ParseFile(sceneFilepath);
		char rootAssetPath[512];
		vkmmc_utils::GetRootDir(sceneFilepath, rootAssetPath, 512);
		check(data);
		uint32_t nodesCount = (uint32_t)data->nodes_count;
		for (uint32_t i = 0; i < nodesCount; ++i)
		{
			const cgltf_node& node = data->nodes[i];
			if (node.mesh)
			{
				scene.Meshes.push_back(SceneNodeMesh());
				SceneNodeMesh& mesh = scene.Meshes.back();
				mesh.Meshes.resize(node.mesh->primitives_count);
				mesh.Materials.resize(node.mesh->primitives_count);
				for (uint32_t j = 0; j < node.mesh->primitives_count; ++j)
				{
					const cgltf_primitive& primitive = node.mesh->primitives[j];
					std::vector<Vertex> vertices;
					std::vector<uint32_t> indices;
					gltf_api::LoadMesh(vertices, indices, &primitive, data->nodes, nodesCount);
					gltf_api::LoadMaterial(engine, rootAssetPath, mesh.Materials[j], primitive);

					mesh.Meshes[j].SetVertices(vertices.data(), vertices.size());
					mesh.Meshes[j].SetIndices(indices.data(), indices.size());

					engine->UploadMesh(mesh.Meshes[j]);
					engine->UploadMaterial(mesh.Materials[j]);
				}
			}
		}
		gltf_api::FreeData(data);
		return scene;
	}
}
