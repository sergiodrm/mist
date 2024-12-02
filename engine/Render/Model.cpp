
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
#include "Texture.h"
#include "Material.h"
#include "Utils/GenericUtils.h"
#include "Utils/TimeUtils.h"
#include "Utils/FileSystem.h"
#undef CGLTF_IMPLEMENTATION

#define GLTF_LOAD_GEOMETRY_POSITION 0x01
#define GLTF_LOAD_GEOMETRY_NORMAL 0x02
#define GLTF_LOAD_GEOMETRY_COLOR 0x04
#define GLTF_LOAD_GEOMETRY_TANGENT 0x08
#define GLTF_LOAD_GEOMETRY_TEXCOORDS 0x10
#define GLTF_LOAD_GEOMETRY_JOINTS 0x20
#define GLTF_LOAD_GEOMETRY_WEIGHTS 0x40
#define GLTF_LOAD_GEOMETRY_ALL 0xff

//#define MESH_DUMP_LOAD_INFO
#ifdef MESH_DUMP_LOAD_INFO
#define loadmeshlabel "[loadmesh] "
#define loadmeshlogf(fmt, ...) logfinfo(loadmeshlabel fmt, __VA_ARGS__)
#define loadmeshlog(fmt, ...) loginfo(loadmeshlabel fmt)
#else
#define loadmeshlogf(fmt, ...) DUMMY_MACRO
#define loadmeshlog(fmt, ...) DUMMY_MACRO
#endif

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

	void ReadValue(void* dst, const cgltf_float* data, uint32_t count)
	{
		memcpy_s(dst, sizeof(float) * count, data, sizeof(float) * count);
	}

	void ToMat4(glm::mat4* mat, const cgltf_float* cgltfMat4)
	{
		ReadValue(mat, cgltfMat4, 16);
	}

	void ToVec2(glm::vec2& v, const cgltf_float* data)
	{
		ReadValue(&v, data, 2);
	}

	void ToVec3(glm::vec3& v, const cgltf_float* data)
	{
		ReadValue(&v, data, 3);
	}

	void ToVec4(glm::vec4& v, const cgltf_float* data)
	{
		ReadValue(&v, data, 4);
	}

	void ToQuat(glm::quat& q, const cgltf_float* data)
	{
		q = glm::quat(data[3], data[0], data[1], data[2]);
	}

	void ReadNodeLocalTransform(const cgltf_node& node, glm::mat4& t)
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

	void ReadValues(Mist::tDynArray<float>& values, const cgltf_accessor& accessor)
	{
		uint32_t elementCount = GetElementCountFromType(accessor.type);
		values.resize(accessor.count * elementCount);
		for (uint32_t i = 0; i < accessor.count; ++i)
			cgltf_accessor_read_float(&accessor, i, &values[i * elementCount], elementCount);
	}

	void ReadAttribute(Mist::Vertex& vertex, const float* source, uint32_t index, cgltf_attribute_type type)
	{
		const char* attributeName = nullptr;
		const float* data = &source[index];
		switch (type)
		{
		case cgltf_attribute_type_position:
			ToVec3(vertex.Position, data);
			break;
		case cgltf_attribute_type_texcoord:
			ToVec2(vertex.TexCoords, data);
			break;
		case cgltf_attribute_type_normal:
			ToVec3(vertex.Normal, data);
			if (Length2(vertex.Normal) < 1e-5f)
				vertex.Normal = glm::vec3{ 0.f, 1.f, 0.f };
			else
				vertex.Normal = glm::normalize(vertex.Normal);
			break;
		case cgltf_attribute_type_color:
			ToVec3(vertex.Color, data);
			break;
		case cgltf_attribute_type_tangent:
			ToVec3(vertex.Tangent, data);
			break;
		case cgltf_attribute_type_joints:
#ifdef MIST_ENABLE_LOADER_LOG
			attributeName = "joints";
#endif // MIST_ENABLE_LOADER_LOG

			break;
		case cgltf_attribute_type_weights:
#ifdef MIST_ENABLE_LOADER_LOG
			attributeName = "weights";
#endif // MIST_ENABLE_LOADER_LOG
			break;
		case cgltf_attribute_type_invalid:
		case cgltf_attribute_type_custom:
		case cgltf_attribute_type_max_enum:
			check(false && "Invalid attribute type to read.");
		default:
			break;
		}
#ifdef MIST_ENABLE_LOADER_LOG
		if (attributeName)
			Mist::Logf(Mist::LogLevel::Error, "gltf loader: Attribute type not suported yet [%s].\n", attributeName);
#endif // MIST_ENABLE_LOADER_LOG

	}

	// Attributes are an continuous array of positions, normals, uvs...
	// We have to map from struct of arrays to our format, array of structs (tDynArray<Mist::Vertex>)
	void ReadAttributeArray(Mist::Vertex* vertices, const cgltf_attribute& attribute)
	{
		const cgltf_accessor* accessor = attribute.data;
		uint32_t accessorCount = (uint32_t)accessor->count;
		// Get how many values has current attribute
		uint32_t elementCount = GetElementCountFromType(accessor->type);
		// Read data from accessor
		Mist::tDynArray<float> values;
		ReadValues(values, *accessor);
		// Map to internal format
		for (uint32_t i = 0; i < accessorCount; ++i)
		{
			Mist::Vertex& vertex = vertices[i];
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
		cgltf_data* data{ nullptr };
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

	void LoadVertices(const cgltf_primitive& primitive, Mist::Vertex* verticesOut, uint32_t vertexCount)
	{
		uint32_t attributeCount = (uint32_t)primitive.attributes_count;
		for (uint32_t i = 0; i < attributeCount; ++i)
		{
			const cgltf_attribute& attribute = primitive.attributes[i];
			check(attribute.data->count <= vertexCount);
			ReadAttributeArray(verticesOut, attribute);
		}
	}

	void LoadIndices(const cgltf_primitive& primitive, uint32_t* indicesOut, uint32_t offset)
	{
		check(primitive.indices && primitive.type == cgltf_primitive_type_triangles);
		uint32_t indexCount = (uint32_t)primitive.indices->count;
		for (uint32_t i = 0; i < indexCount; ++i)
			indicesOut[i] = (uint32_t)cgltf_accessor_read_index(primitive.indices, i) + offset;
	}

	bool LoadTexture(const Mist::RenderContext& context, const char* rootAssetPath, const cgltf_texture_view& texView, Mist::EFormat format, Mist::cTexture** texOut)
	{
		if (!texView.texture)
			return false;
		char texturePath[512];
		sprintf_s(texturePath, "%s%s", rootAssetPath, texView.texture->image->uri);
		check(Mist::LoadTextureFromFile(context, texturePath, texOut, format));
		loadmeshlogf("Load texture: %s\n", texView.texture->image->uri);
		return true;
	}

	template <typename T>
	inline Mist::index_t GetArrayElementOffset(const T* root, const T* item) { check(item >= root); return Mist::index_t(item - root); }

	void LoadMaterial(Mist::cMaterial& material, const Mist::RenderContext& context, const cgltf_material& cgltfmtl, const char* rootAssetPath)
	{
		const cgltf_texture_view* views[] = {
			&cgltfmtl.pbr_metallic_roughness.base_color_texture,
			&cgltfmtl.normal_texture,
			&cgltfmtl.pbr_specular_glossiness.specular_glossiness_texture,
			&cgltfmtl.occlusion_texture,
			&cgltfmtl.pbr_metallic_roughness.metallic_roughness_texture,
			&cgltfmtl.emissive_texture
		};
		const Mist::EFormat formats[] =
		{
			Mist::FORMAT_R8G8B8A8_SRGB,
			Mist::FORMAT_R8G8B8A8_UNORM,
			Mist::FORMAT_R8G8B8A8_UNORM,
			Mist::FORMAT_R8G8B8A8_UNORM,
			Mist::FORMAT_R8G8B8A8_UNORM,
			Mist::FORMAT_R8G8B8A8_SRGB,
		};
		static_assert(sizeof(views) / sizeof(cgltf_texture_view*) == Mist::MATERIAL_TEXTURE_COUNT);
		static_assert(sizeof(formats) / sizeof(Mist::EFormat) == Mist::MATERIAL_TEXTURE_COUNT);

		loadmeshlogf("Loading material %s\n", material.GetName());
		for (uint32_t i = 0; i < Mist::MATERIAL_TEXTURE_COUNT; ++i)
		{
			if (LoadTexture(context, rootAssetPath, *views[i],
				formats[i], &material.m_textures[i]))
			{
				material.m_textures[i]->CreateView(context, Mist::tViewDescription());
			}
		}
		material.m_emissiveFactor = {cgltfmtl.emissive_factor[0], cgltfmtl.emissive_factor[1] , cgltfmtl.emissive_factor[2], cgltfmtl.emissive_strength.emissive_strength };
	}

}

namespace Mist
{
	void CalculateTangent(glm::vec3& t, const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2)
	{
		glm::vec3 e0 = p1 - p0;
		glm::vec3 e1 = p2 - p0;
		glm::vec2 duv0 = uv1 - uv0;
		glm::vec2 duv1 = uv2 - uv0;
		float f = 1.f / (duv0.x * duv1.y - duv1.x * duv0.y);

		t.x = f * (duv1.y * e0.x - duv0.y * e1.x);
		t.y = f * (duv1.y * e0.y - duv0.y * e1.y);
		t.z = f * (duv1.y * e0.z - duv0.y * e1.z);
	}

	void BuildTangents(Vertex* vertices, lindex_t vertexCount, lindex_t* indices, lindex_t indexCount)
	{
		check(indexCount % 3 == 0);
		check(vertices && vertexCount && indices);
		for (lindex_t i = 0; i < indexCount; i+=3)
		{
			Vertex& v0 = vertices[indices[i]];
			Vertex& v1 = vertices[indices[i+1]];
			Vertex& v2 = vertices[indices[i+2]];
			glm::vec3 t(0.f);
			CalculateTangent(t, v0.Position, v1.Position, v2.Position, v0.TexCoords, v1.TexCoords, v2.TexCoords);
			t = glm::normalize(t);
			v0.Tangent = t;
			v1.Tangent = t;
			v2.Tangent = t;
		}
	}


	void cModel::Destroy(const RenderContext& context)
	{
		for (index_t i = 0; i < m_meshes.GetSize(); ++i)
			m_meshes[i].Destroy(context);
		for (index_t i = 0; i < m_materials.GetSize(); ++i)
			m_materials[i].Destroy(context);
		m_nodes.Delete();
		m_meshes.Delete();
		m_materials.Delete();
		m_transforms.Delete();
		m_root = index_invalid;
	}

	void cModel::LoadModel(const RenderContext& context, const char* filepath)
	{
		PROFILE_SCOPE_LOGF(LoadModel, "Load model (%s)", filepath);
		check(m_materials.IsEmpty() && m_meshes.IsEmpty());
		cAssetPath assetPath(filepath);
		cgltf_data* data = gltf_api::ParseFile(assetPath);
		char rootAssetPath[512];
		io::GetDirectoryFromFilepath(assetPath, rootAssetPath, 512);
		if (!data)
		{
			Logf(LogLevel::Error, "Cannot open file to load scene model: %s.\n", assetPath);
			return;
		}

		if (!data->nodes_count)
		{
			Logf(LogLevel::Error, "Model file without nodes in scene: %s.\n", assetPath);
			gltf_api::FreeData(data);
			return;
		}
		SetName(assetPath);

		loadmeshlogf("Loading model from file: %s\n", assetPath);
		loadmeshlogf("* nodes: %d\n", data->nodes_count);
		loadmeshlogf("* materials: %d\n", data->materials_count);
		loadmeshlogf("* meshes: %d\n", data->meshes_count);

		InitMaterials((index_t)data->materials_count);
		for (uint32_t i = 0; i < data->materials_count; ++i)
		{
			m_materials[i].SetName(data->materials[i].name && *data->materials[i].name ? data->materials[i].name : "unknown");
			gltf_api::LoadMaterial(m_materials[i], context, data->materials[i], rootAssetPath);
		}

		InitNodes((index_t)data->nodes_count);
		InitMeshes((index_t)data->meshes_count);

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
			loadmeshlogf("node %d %s child of %d\n", i, m_nodeNames[i].CStr(), parentIndex);
			loadmeshlogf("node %d %s [pos %.3f, %.3f, %.3f][rot %.3f, %.3f, %.3f][scl %.3f, %.3f, %.3f]\n", i, m_nodeNames[i].CStr(),
				pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, scl.x, scl.y, scl.z);

			// Process mesh
			if (node.mesh)
			{
				index_t meshIndex = CreateMesh();
				m_nodes[nodeIndex].MeshId = meshIndex;
				cMesh& mesh = m_meshes[meshIndex];
				mesh.SetName(node.mesh->name && *node.mesh->name ? node.mesh->name : "unknown");

				loadmeshlogf("node %d %s has mesh %s\n", i, m_nodeNames[i].CStr(), mesh.GetName());

				tFixedHeapArray<PrimitiveMeshData>& primitives = mesh.PrimitiveArray;
				primitives.Allocate((index_t)node.mesh->primitives_count);
				primitives.Resize((index_t)node.mesh->primitives_count);
				loadmeshlogf("* primitives: %d\n", node.mesh->primitives_count);
				for (uint32_t j = 0; j < node.mesh->primitives_count; ++j)
				{
					const cgltf_primitive& cgltfprimitive = node.mesh->primitives[j];
					PrimitiveMeshData& primitive = primitives[j];
					check(cgltfprimitive.type == cgltf_primitive_type_triangles);
					check(cgltfprimitive.indices && cgltfprimitive.indices->type == cgltf_type_scalar && cgltfprimitive.indices->count % 3 == 0);
					check(cgltfprimitive.attributes && cgltfprimitive.attributes->data);

					uint32_t indexCount = (uint32_t)cgltfprimitive.indices->count;
					uint32_t vertexCount = (uint32_t)cgltfprimitive.attributes[0].data->count;
					uint32_t vertexOffset = (uint32_t)tempVertices.size();
					uint32_t indexOffset = (uint32_t)tempIndices.size();
					tempIndices.resize(indexOffset + indexCount);
					tempVertices.resize(vertexOffset + vertexCount);

					loadmeshlogf("** primitive %d: [vertices %d | %d bytes][indices %d | %d bytes]\n", j, vertexCount, sizeof(Vertex) * vertexCount, indexCount, sizeof(uint32_t) * indexCount);

					primitive.FirstIndex = indexOffset;
					primitive.Count = indexCount;

					gltf_api::LoadIndices(cgltfprimitive, tempIndices.data() + indexOffset, vertexOffset);
					gltf_api::LoadVertices(cgltfprimitive, tempVertices.data() + vertexOffset, vertexCount);

					check(cgltfprimitive.material);
					index_t materialIndex = gltf_api::GetArrayElementOffset(data->materials, cgltfprimitive.material);
					cMaterial* material = &m_materials[materialIndex];
					primitive.Material = material;
					primitive.RenderFlags = RenderFlags_Fixed | RenderFlags_ShadowMap;
					if (material->m_textures[MATERIAL_TEXTURE_EMISSIVE] || material->m_emissiveFactor != glm::vec4(0.f))
						primitive.RenderFlags |= RenderFlags_Emissive;

					check(cgltfprimitive.indices->count == primitive.Count);
				}

				//BuildTangents(tempVertices.data(), tempVertices.size(), tempIndices.data(), tempIndices.size());

				mesh.SetupIndexBuffer(context, tempIndices.data(), (uint32_t)tempIndices.size());
				mesh.SetupVertexBuffer(context, tempVertices.data(), (uint32_t)tempVertices.size() * sizeof(Vertex));
				tempIndices.clear();
				tempVertices.clear();
			}
			else
				loadmeshlogf("node %d %s has no mesh\n", i, m_nodeNames[i].CStr());
		}
		loadmeshlog("=== End loading model ===\n");
		gltf_api::FreeData(data);
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

	void cModel::ImGuiDraw()
	{
		if (ImGui::TreeNode("Model tree"))
		{
			for (index_t i = 0; i < m_nodes.GetSize(); ++i)
			{
				const sNode& node = m_nodes[i];
				char buff[16];
				sprintf_s(buff, "##node%d", i);
				if (ImGui::TreeNode(buff, m_nodeNames[i].CStr()))
				{
					sprintf_s(buff, "##mesh%d", i);
					if (node.MeshId != index_invalid)
					{
						if (ImGui::TreeNode(buff, m_meshes[node.MeshId].GetName()))
						{
							cMesh& mesh = m_meshes[node.MeshId];
							ImGui::Text("Primitives: %5d", mesh.PrimitiveArray.GetSize());
							ImGui::Text("Triangles:  %5d", mesh.IndexCount / 3);
							ImGui::Text("Gpu memory: %5.2f MB", (float)mesh.IndexCount / 3.f * sizeof(Vertex) / 1024.f / 1024.f);
							sprintf_s(buff, "##prim%d", i);
							if (ImGui::TreeNode(buff, "Primitives"))
							{
								for (index_t i = 0; i < mesh.PrimitiveArray.GetSize(); ++i)
								{
									PrimitiveMeshData& primitive = mesh.PrimitiveArray[i];
									ImGui::Text("Material:   %s", primitive.Material ? primitive.Material->GetName() : "none");
									ImGui::Text("Triangles: %4d", primitive.Count/3);
									int flags = primitive.RenderFlags;
									if (ImGui::CheckboxFlags("No shadows", &flags, RenderFlags_ShadowMap))
										primitive.RenderFlags = (uint16_t)flags;
									if (ImGui::CheckboxFlags("No visible", &flags, RenderFlags_Fixed))
										primitive.RenderFlags = (uint16_t)flags;
								}
								ImGui::TreePop();
							}
							ImGui::TreePop();
						}
					}
					else
						ImGui::Text("No mesh");
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Material list"))
		{
			for (index_t i = 0; i < m_materials.GetSize(); ++i)
			{
				if (ImGui::TreeNode(&m_materials[i], "Material %d: %s", i, m_materials[i].GetName()))
				{
					cMaterial& material = m_materials[i];
					for (index_t j = 0; j < MATERIAL_TEXTURE_COUNT; ++j)
						ImGui::Text("%s: %s", 
							GetMaterialTextureStr((eMaterialTexture)j), material.m_textures[j] ?material.m_textures[j]->GetName() : "none");
					ImGui::ColorEdit3("Emissive", &material.m_emissiveFactor[0]);
					ImGui::DragFloat("Emissive strength", &material.m_emissiveFactor[3], 0.1f, 0.f, FLT_MAX);
					
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
	}

	void cModel::InitNodes(index_t n)
	{
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

		// Build default root node
		m_root = n - 1;
		sNode* node = GetNode(m_root);
		check(node);
		SetNodeName(m_root, "_root_");
		SetNodeTransform(m_root, glm::mat4(1.f));
	}

	void cModel::InitMeshes(index_t n)
	{
		check(m_meshes.IsEmpty());
		m_meshes.Allocate(n);
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
		const sNode& n = m_nodes[node];

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
		sNode& node = m_nodes[nodeIndex];
		SetNodeName(nodeIndex, nodeName && *nodeName ? nodeName : "unknown");
		sNode* nodeParent = GetNode(parentIndex);
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
				sNode* sibling = GetNode(nodeParent->Child);
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

	cModel::sNode* cModel::GetNode(index_t i)
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
}