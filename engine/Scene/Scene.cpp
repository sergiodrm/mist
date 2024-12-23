// Autogenerated code for Mist project
// Source file
#include "Scene.h"
#include "Core/Logger.h"
#include "Core/Debug.h"



#ifdef MIST_MEM_MANAGEMENT
// TODO: wtf windows declare these macros and project does not compile when MIST_MEM_MANAGEMENT is defined. WTF!?
#undef min
#undef max  
#endif // MIST_MEM_MANAGEMENT

#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/fwd.hpp>
#include "Render/Mesh.h"
#include "Utils/GenericUtils.h"
#include "Render/VulkanRenderEngine.h"
#include "Render/DebugRender.h"
#include <algorithm>
#include <imgui.h>

#define SCENE_LOAD_YAML
#ifdef SCENE_LOAD_YAML

#include <yaml-cpp/yaml.h>

#endif // SCENE_LOAD_YAML
#include <fstream>
#include "Core/SystemMemory.h"
#include "Utils/TimeUtils.h"
#include "Render/Model.h"

//#define MIST_ENABLE_LOADER_LOG

#ifdef SCENE_LOAD_YAML
YAML::Emitter& operator<<(YAML::Emitter& e, const glm::vec3& v)
{
	e << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
	return e;
}

YAML::Emitter& operator<<(YAML::Emitter& e, const glm::vec4& v)
{
	e << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	return e;
}

YAML::Emitter& operator<<(YAML::Emitter& e, const Mist::tAngles& a)
{
	e << YAML::Flow << YAML::BeginSeq << a.m_pitch << a.m_yaw << a.m_roll << YAML::EndSeq;
	return e;
}


namespace YAML
{
	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<Mist::tAngles>
	{
		static Node encode(const Mist::tAngles& rhs)
		{
			Node node;
			node.push_back(rhs.m_pitch);
			node.push_back(rhs.m_yaw);
			node.push_back(rhs.m_roll);
			return node;
		}

		static bool decode(const Node& node, Mist::tAngles& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.m_pitch = node[0].as<float>();
			rhs.m_yaw = node[1].as<float>();
			rhs.m_roll = node[2].as<float>();
			return true;
		}
	};
}

#endif


namespace Mist
{
	const char* LightTypeToStr(ELightType e)
	{
		switch (e)
		{
		case ELightType::Point: return "Point";
		case ELightType::Directional: return "Directional";
		case ELightType::Spot: return "Spot";
		}
		check(false);
		return nullptr;
	}

	ELightType StrToLightType(const char* str)
	{
		if (!strcmp(str, "Point")) return ELightType::Point;
		if (!strcmp(str, "Spot")) return ELightType::Spot;
		if (!strcmp(str, "Directional")) return ELightType::Directional;
		check(false);
		return (ELightType)0xff;
	}

	void TransformComponentToMatrix(const TransformComponent* transforms, glm::mat4* matrices, uint32_t count)
	{
		for (uint32_t i = 0; i < count; ++i)
			matrices[i] = math::ToMat4(transforms[i].Position, transforms[i].Rotation, transforms[i].Scale);
	}

	template <uint32_t N>
	void GetResourceFileId(const char* file, const char* resname, char(&buff)[N])
	{
		sprintf_s(buff, "%s|%s", file, resname);
	}

	EnvironmentData::EnvironmentData() :
		AmbientColor(0.02f, 0.02f, 0.02f),
		ActiveSpotLightsCount(0.f),
		ViewPosition(0.f),
		ActiveLightsCount(0.f)
	{
		DirectionalLight.Color = { 0.01f, 0.01f, 0.1f };
		DirectionalLight.Position = { 0.f, 0.f, 1.f };
		DirectionalLight.ShadowMapIndex = -1.f;
		DirectionalLight.Compression = 0.5f;
		for (uint32_t i = 0; i < MaxLights; ++i)
		{
			Lights[i] = DirectionalLight;
			Lights[i].Radius = 50.f;
			SpotLights[i].Direction = { 1.f, 0.f, 0.f };
			SpotLights[i].Position = { 0.f, 0.f, 0.f };
			SpotLights[i].Color = { 1.f, 1.f, 1.f };
			SpotLights[i].CosOuterCutoff = 1.f;
			SpotLights[i].CosCutoff = 1.f;
			SpotLights[i].ShadowMapIndex = -1.f;
		}
		ZeroMem(this, sizeof(*this));
		AmbientColor = { 0.02f, 0.02f, 0.02f };
	}

	Scene::Scene(IRenderEngine* engine) : m_engine(static_cast<VulkanRenderEngine*>(engine))
	{
		// Create root scene
		//CreateRenderObject(RenderObject::InvalidId);
	}

	Scene::~Scene()
	{}

	void Scene::Init()
	{
		m_globalTransforms.resize(globals::MaxRenderObjects);
		m_localTransforms.resize(globals::MaxRenderObjects);
		m_renderTransforms.resize(globals::MaxRenderObjects);
		m_transformComponents.resize(globals::MaxRenderObjects);
		m_materials.resize(globals::MaxRenderObjects);
	}

	void Scene::Destroy()
	{
#if 0
		if (m_engine->GetScene() == this)
			m_engine->SetScene(nullptr);
#endif // 0

		const RenderContext& renderContext = m_engine->GetContext();

		if (m_skybox.Tex)
			cTexture::Destroy(renderContext, m_skybox.Tex);
		for (uint32_t i = 0; i < m_models.GetSize(); ++i)
			m_models[i].Destroy(renderContext);
		m_models.Clear();
		m_localTransforms.clear();
		m_globalTransforms.clear();
		m_hierarchy.clear();
		m_names.clear();
		m_materials.clear();
		for (uint32_t i = 0; i < MaxNodeLevel; ++i)
			m_dirtyNodes[i].clear();
	}

	void Scene::InitFrameData(const RenderContext& renderContext, RenderFrameContext& frameContext)
	{
#if 0
		frameContext.GlobalBuffer.AllocDynamicUniform(renderContext, "Material", sizeof(MaterialUniform), MIST_MAX_MATERIALS);
		VkDescriptorBufferInfo info = frameContext.GlobalBuffer.GenerateDescriptorBufferInfo("Material");
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindBuffer(0, &info, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(renderContext, m_materialSetArray[frameContext.FrameIndex]);
#endif // 0
		//frameContext.GlobalBuffer.AllocDynamicUniform(renderContext, "u_materials", sizeof(sMaterialRenderData), globals::MaxRenderObjects);
	}

	sRenderObject Scene::CreateRenderObject(sRenderObject parent)
	{
		// Generate new node in all basics structures
		sRenderObject node = (uint32_t)m_hierarchy.size();
		m_localTransforms[node] = glm::mat4(1.f);
		m_globalTransforms[node] = glm::mat4(1.f);
		m_transformComponents[node] = { .Position = glm::vec3(0.f), .Rotation = tAngles(0.f), .Scale = glm::vec3(1.f) };
		char buff[64];
		sprintf_s(buff, "RenderObject_%u", node.Id);
		m_names.push_back(buff);
		m_hierarchy.push_back({ .Parent = parent });

		// Connect siblings
		if (parent.IsValid())
		{
			check(parent.Id < (uint32_t)m_hierarchy.size());
			sRenderObject firstSibling = m_hierarchy[parent].Child;
			if (firstSibling.IsValid())
			{
				sRenderObject sibling;
				for (sibling = firstSibling; m_hierarchy[sibling].Sibling.IsValid(); sibling = m_hierarchy[sibling].Sibling);
				m_hierarchy[sibling].Sibling = node;
			}
			else
			{
				m_hierarchy[parent].Child = node;
			}
		}

		m_hierarchy[node].Level = parent.IsValid() ? m_hierarchy[parent].Level + 1 : 0;
		m_hierarchy[node].Child = index_invalid;
		m_hierarchy[node].Sibling = index_invalid;
		return node;
	}

	index_t Scene::LoadModel(const RenderContext& context, const char* filepath)
	{
		PROFILE_SCOPE_LOGF(LoadModel, "Load model (%s)", filepath);
		cModel* model = GetModel(filepath);
		if (!model)
		{
			m_models.Push();
			model = &m_models.GetBack();
			check(model->LoadModel(context, filepath));
		}
		check(model);
		return (index_t)(model-m_models.GetData());
	}

	void Scene::LoadScene(const RenderContext& context, const char* filepath)
	{
		PROFILE_SCOPE_LOGF(LoadScene, "Load scene (%s)", filepath);
		io::File file;
		check(file.Open(filepath, "r"));
		uint32_t size = file.GetContentSize();
		char* content = _new char[size];
		uint32_t contentReaded = file.Read(content, size, sizeof(char), size);
		file.Close();
		check(contentReaded <= size);
		content[contentReaded == size ? size - 1 : contentReaded] = 0;

		YAML::Node root = YAML::Load(content);
		check(root);
		YAML::Node envNode = root["Environment"];
		check(envNode);
		m_ambientColor = envNode["Ambient"].as<glm::vec3>();
		char skyboxTextures[Skybox::COUNT][256];
		strcpy_s(skyboxTextures[Skybox::FRONT], envNode["Skybox"]["Front"].as<std::string>().c_str());
		strcpy_s(skyboxTextures[Skybox::BACK], envNode["Skybox"]["Back"].as<std::string>().c_str());
		strcpy_s(skyboxTextures[Skybox::TOP], envNode["Skybox"]["Top"].as<std::string>().c_str());
		strcpy_s(skyboxTextures[Skybox::BOTTOM], envNode["Skybox"]["Bottom"].as<std::string>().c_str());
		strcpy_s(skyboxTextures[Skybox::LEFT], envNode["Skybox"]["Left"].as<std::string>().c_str());
		strcpy_s(skyboxTextures[Skybox::RIGHT], envNode["Skybox"]["Right"].as<std::string>().c_str());
		LoadSkybox(m_engine->GetContext(), m_skybox,
			skyboxTextures[Skybox::FRONT],
			skyboxTextures[Skybox::BACK],
			skyboxTextures[Skybox::LEFT],
			skyboxTextures[Skybox::RIGHT],
			skyboxTextures[Skybox::TOP],
			skyboxTextures[Skybox::BOTTOM]);



		YAML::Node renderObjectSeq = root["RenderObjects"];
		check(renderObjectSeq);
		for (const auto& it : renderObjectSeq)
		{
			uint32_t parent = it["Parent"].as<uint32_t>();
			sRenderObject rb = CreateRenderObject(parent);
			SetRenderObjectName(rb, it["Name"].as<std::string>().c_str());

			TransformComponent t;
			YAML::Node transformNode = it["TransformComponent"];
			t.Position = transformNode["Position"].as<glm::vec3>();
			t.Rotation = transformNode["Rotation"].as<tAngles>();
			t.Scale = transformNode["Scale"].as<glm::vec3>();
			SetTransform(rb, t);

			YAML::Node lightNode = it["LightComponent"];
			if (lightNode)
			{
				LightComponent lightComponent;
				lightComponent.Color = lightNode["Color"].as<glm::vec3>();
				lightComponent.Radius = lightNode["Radius"].as<float>();
				lightComponent.Compression = lightNode["Compression"].as<float>();
				lightComponent.OuterCutoff = lightNode["OuterCutoff"].as<float>();
				lightComponent.Cutoff = lightNode["Cutoff"].as<float>();
				lightComponent.ProjectShadows = lightNode["ProjectShadows"].as<bool>();
				lightComponent.Type = StrToLightType(lightNode["Type"].as<std::string>().c_str());
				SetLight(rb, lightComponent);
			}

			YAML::Node meshNode = it["MeshComponent"];
			if (meshNode)
			{
				MeshComponent m;
				strcpy_s(m.MeshAssetPath, meshNode["MeshAssetPath"].as<std::string>().c_str());
				m.MeshIndex = LoadModel(context, m.MeshAssetPath);
				SetMesh(rb, m);
			}
		}
		delete[] content;
	}

	void Scene::SaveScene(const RenderContext& context, const char* filepath)
	{
		YAML::Emitter emitter;

		emitter << YAML::BeginMap;
		emitter << YAML::Key << "Environment";
		{
			emitter << YAML::BeginMap;
			emitter << YAML::Key << "Ambient" << YAML::Value << m_ambientColor;
			emitter << YAML::Key << "Skybox" << YAML::BeginMap;
			emitter << YAML::Key << "Front" << YAML::Value << m_skybox.CubemapFiles[Skybox::FRONT];
			emitter << YAML::Key << "Back" << YAML::Value << m_skybox.CubemapFiles[Skybox::BACK];
			emitter << YAML::Key << "Top" << YAML::Value << m_skybox.CubemapFiles[Skybox::TOP];
			emitter << YAML::Key << "Bottom" << YAML::Value << m_skybox.CubemapFiles[Skybox::BOTTOM];
			emitter << YAML::Key << "Left" << YAML::Value << m_skybox.CubemapFiles[Skybox::LEFT];
			emitter << YAML::Key << "Right" << YAML::Value << m_skybox.CubemapFiles[Skybox::RIGHT];
			emitter << YAML::EndMap;
			emitter << YAML::EndMap;
		}
		emitter << YAML::Key << "RenderObjects";
		emitter << YAML::BeginSeq;
		for (uint32_t i = 0; i < GetRenderObjectCount(); ++i)
		{
			const Hierarchy& h = m_hierarchy[i];
			emitter << YAML::BeginMap;
			emitter << YAML::Key << "RenderObject" << YAML::Value << i;
			emitter << YAML::Key << "Name" << YAML::Value << GetRenderObjectName(i);
			emitter << YAML::Key << "Parent" << YAML::Value << h.Parent;

			const TransformComponent& t = m_transformComponents[i];
			emitter << YAML::Key << "TransformComponent" << YAML::BeginMap;
			emitter << YAML::Key << "Position" << YAML::Value << t.Position;
			emitter << YAML::Key << "Rotation" << YAML::Value << t.Rotation;
			emitter << YAML::Key << "Scale" << YAML::Value << t.Scale;
			emitter << YAML::EndMap;

			if (m_lightComponentMap.contains(i))
			{
				const LightComponent& light = m_lightComponentMap[i];
				emitter << YAML::Key << "LightComponent" << YAML::BeginMap;
				emitter << YAML::Key << "Type" << YAML::Value << LightTypeToStr(light.Type);
				emitter << YAML::Key << "Color" << YAML::Value << light.Color;
				emitter << YAML::Key << "Radius" << YAML::Value << light.Radius;
				emitter << YAML::Key << "Compression" << YAML::Value << light.Radius;
				emitter << YAML::Key << "OuterCutoff" << YAML::Value << light.OuterCutoff;
				emitter << YAML::Key << "Cutoff" << YAML::Value << light.Cutoff;
				emitter << YAML::Key << "ProjectShadows" << YAML::Value << light.ProjectShadows;
				emitter << YAML::EndMap;
			}

			if (m_meshComponentMap.contains(i))
			{
				emitter << YAML::Key << "MeshComponent" << YAML::BeginMap;
				const MeshComponent& mesh = m_meshComponentMap[i];
				emitter << YAML::Key << "MeshAssetPath" << YAML::Value << mesh.MeshAssetPath;
				emitter << YAML::EndMap;
			}

			emitter << YAML::EndMap;
		}
		emitter << YAML::EndSeq;
		emitter << YAML::EndMap;

		check(emitter.good());
		const char* out = emitter.c_str();
		uint32_t size = (uint32_t)emitter.size();
		Logf(LogLevel::Info, "YAML %u b\n%s\n", size, out);

		io::File file;
		check(file.Open(filepath, "w"));
		file.Write(out, size);
		file.Close();
	}

	void Scene::DestroyRenderObject(sRenderObject object)
	{
		check(false && "not implemented yet" && __FUNCTION__ && __FILE__ && __LINE__);
	}

	const MeshComponent* Scene::GetMesh(sRenderObject renderObject) const
	{
		check(IsValid(renderObject));
		const MeshComponent* mesh = nullptr;
		if (m_meshComponentMap.contains(renderObject))
			mesh = &m_meshComponentMap.at(renderObject);
		return mesh;
	}

	void Scene::SetMesh(sRenderObject renderObject, const MeshComponent& meshComponent)
	{
		check(IsValid(renderObject));
		m_meshComponentMap[renderObject] = meshComponent;
	}

	const char* Scene::GetRenderObjectName(sRenderObject object) const
	{
		return IsValid(object) ? m_names[object.Id].c_str() : nullptr;
	}

	bool Scene::IsValid(sRenderObject object) const
	{
		return object.Id < (uint32_t)m_hierarchy.size();
	}

	uint32_t Scene::GetRenderObjectCount() const
	{
		return (uint32_t)m_hierarchy.size();
	}

	sRenderObject Scene::GetRoot() const
	{
		static sRenderObject root = 0;
		//for (; m_hierarchy[root].Parent.IsValid(); root = m_hierarchy[root].Parent);
		return root;
	}

	void Scene::SetRenderObjectName(sRenderObject renderObject, const char* name)
	{
		check(IsValid(renderObject));
		m_names[renderObject] = name;
	}

	const TransformComponent& Scene::GetTransform(sRenderObject renderObject) const
	{
		check(IsValid(renderObject));
		return m_transformComponents[renderObject];
	}

	void Scene::SetTransform(sRenderObject renderObject, const TransformComponent& transform)
	{
		check(IsValid(renderObject));
		m_transformComponents[renderObject] = transform;
		MarkAsDirty(renderObject);
	}

	const LightComponent* Scene::GetLight(sRenderObject renderObject) const
	{
		check(IsValid(renderObject));
		const LightComponent* light = nullptr;
		if (m_lightComponentMap.contains(renderObject))
			light = &m_lightComponentMap.at(renderObject);
		return light;
	}

	void Scene::SetLight(sRenderObject renderObject, const LightComponent& light)
	{
		check(IsValid(renderObject));
		m_lightComponentMap[renderObject] = light;
	}

	void Scene::MarkAsDirty(sRenderObject renderObject)
	{
		check(IsValid(renderObject));
		int32_t level = m_hierarchy[renderObject].Level;
		m_dirtyNodes[level].push_back(renderObject);
		for (sRenderObject child = m_hierarchy[renderObject].Child; child.IsValid(); child = m_hierarchy[child].Sibling)
			MarkAsDirty(child);
	}


	void Scene::RecalculateTransforms()
	{
		CPU_PROFILE_SCOPE(RecalculateTransforms);
		check(m_localTransforms.size() == m_transformComponents.size());
		check(m_globalTransforms.size() == m_transformComponents.size());
		TransformComponentToMatrix(m_transformComponents.data(), m_localTransforms.data(), (uint32_t)m_transformComponents.size());
		// Process root level first
		if (!m_dirtyNodes[0].empty())
		{
			for (uint32_t i = 0; i < m_dirtyNodes[0].size(); ++i)
			{
				uint32_t nodeIndex = m_dirtyNodes[0][i];
				m_globalTransforms[nodeIndex] = m_localTransforms[nodeIndex];
			}
			m_dirtyNodes[0].clear();
		}
		// Iterate over the deeper levels
		for (uint32_t level = 1; level < MaxNodeLevel; ++level)
		{
			for (uint32_t nodeIndex = 0; nodeIndex < m_dirtyNodes[level].size(); ++nodeIndex)
			{
				int32_t node = m_dirtyNodes[level][nodeIndex];
				int32_t parentNode = m_hierarchy[node].Parent;
				m_globalTransforms[node] = m_globalTransforms[parentNode] * m_localTransforms[node];
			}
			m_dirtyNodes[level].clear();
		}

		index_t offset = 0;
		for (index_t i = 0; i < m_hierarchy.size(); ++i)
		{
			if (m_meshComponentMap.contains(i))
			{
				index_t meshIndex = m_meshComponentMap[i].MeshIndex;
				cModel& model = m_models[meshIndex];
				model.UpdateRenderTransforms(m_renderTransforms.data() + offset, m_globalTransforms[i]);
				offset += model.GetTransformsCount();
			}
		}

	}

	bool Scene::LoadMeshesFromFile(const RenderContext& context, const char* filepath)
	{
#if 1
		check(false);
		return true;
#else

		PROFILE_SCOPE_LOG(LoadMeshes, __FUNCTION__);
		cgltf_data* data = gltf_api::ParseFile(filepath);
		char rootAssetPath[512];
		io::GetDirectoryFromFilepath(filepath, rootAssetPath, 512);
		if (!data)
		{
			Logf(LogLevel::Error, "Cannot open file to load scene model: %s.\n", filepath);
			return false;
		}

		for (uint32_t i = 0; i < data->materials_count; ++i)
		{
			cMaterial* m = CreateMaterial(data->materials[i].name);
			gltf_api::LoadMaterial(*m, context, data->materials[i], rootAssetPath);
		}

		uint32_t nodesCount = (uint32_t)data->nodes_count;
		uint32_t renderObjectOffset = GetRenderObjectCount();

		tDynArray<Vertex> tempVertices;
		tDynArray<uint32_t> tempIndices;
		for (uint32_t i = 0; i < nodesCount; ++i)
		{
			const cgltf_node& node = data->nodes[i];

			// Process mesh
			if (node.mesh)
			{
				m_meshes.push_back(cMesh());
				cMesh& mesh = m_meshes.back();
				mesh.SetName(node.mesh->name);
				tDynArray<PrimitiveMeshData>& primitives = mesh.PrimitiveArray;
				tempVertices.clear();
				tempIndices.clear();
				primitives.resize(node.mesh->primitives_count);
				for (uint32_t j = 0; j < node.mesh->primitives_count; ++j)
				{
					const cgltf_primitive& cgltfprimitive = node.mesh->primitives[j];
					PrimitiveMeshData& primitive = primitives[j];
					check(cgltfprimitive.type == cgltf_primitive_type_triangles);
					check(cgltfprimitive.indices && cgltfprimitive.indices->type == cgltf_type_scalar && cgltfprimitive.indices->count % 3 == 0);
					check(cgltfprimitive.attributes && cgltfprimitive.attributes->data);

					uint32_t indexCount = cgltfprimitive.indices->count;
					uint32_t vertexCount = cgltfprimitive.attributes[0].data->count;
					uint32_t vertexOffset = (uint32_t)tempVertices.size();
					uint32_t indexOffset = (uint32_t)tempIndices.size();
					tempIndices.resize(indexOffset + cgltfprimitive.indices->count);
					tempVertices.resize(vertexOffset + vertexCount);

					primitive.FirstIndex = indexOffset;
					primitive.Count = indexCount;

					gltf_api::LoadIndices(cgltfprimitive, tempIndices.data() + indexOffset, vertexOffset);
					gltf_api::LoadVertices(cgltfprimitive, tempVertices.data() + vertexOffset, vertexCount);

					cMaterial* material = GetMaterial(cgltfprimitive.material->name);
					check(material);
					primitive.Material = material;
					primitive.RenderFlags = RenderFlags_Fixed | RenderFlags_ShadowMap;
					if (primitive.Material->m_emissiveFactor.x || primitive.Material->m_emissiveFactor.y || primitive.Material->m_emissiveFactor.z)
						primitive.RenderFlags |= RenderFlags_Emissive;

					check(cgltfprimitive.indices->count == primitive.Count);
				}

				mesh.SetupVertexBuffer(context, tempVertices.data(), sizeof(Vertex) * (uint32_t)tempVertices.size());
				mesh.SetupIndexBuffer(context, tempIndices.data(), (uint32_t)tempIndices.size());

				char buff[256];
				GetResourceFileId(filepath, node.mesh->name, buff);
				check(!m_meshNameIndexMap.contains(buff));
				m_meshNameIndexMap[buff] = m_meshes.size() - 1;
			}
		}
		tempVertices.clear();
		tempVertices.shrink_to_fit();
		tempIndices.clear();
		tempIndices.shrink_to_fit();
		gltf_api::FreeData(data);
		return true;

#endif // 1
	}

	bool Scene::LoadSkybox(const RenderContext& context, Skybox& skybox, const char* front, const char* back, const char* left, const char* right, const char* top, const char* bottom)
	{
		PROFILE_SCOPE_LOG(LoadSkybox);
		// descriptors generator
		DescriptorBuilder builder = DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator);

		strcpy_s(skybox.CubemapFiles[Skybox::FRONT], front);
		strcpy_s(skybox.CubemapFiles[Skybox::BACK], back);
		strcpy_s(skybox.CubemapFiles[Skybox::LEFT], left);
		strcpy_s(skybox.CubemapFiles[Skybox::RIGHT], right);
		strcpy_s(skybox.CubemapFiles[Skybox::TOP], top);
		strcpy_s(skybox.CubemapFiles[Skybox::BOTTOM], bottom);

		// Load textures from files
		io::TextureRaw textureData[Skybox::COUNT];
		check(io::LoadTexture(front, textureData[Skybox::FRONT]));
		check(io::LoadTexture(back, textureData[Skybox::BACK]));
		check(io::LoadTexture(left, textureData[Skybox::LEFT]));
		check(io::LoadTexture(right, textureData[Skybox::RIGHT]));
		check(io::LoadTexture(top, textureData[Skybox::TOP]));
		check(io::LoadTexture(bottom, textureData[Skybox::BOTTOM]));
		// Size integrity check and generate an array to reference the pixels of each texture.
		const uint8_t* pixelsArray[Skybox::COUNT];
		pixelsArray[Skybox::FRONT] = textureData[Skybox::FRONT].Pixels;
		pixelsArray[Skybox::BACK] = textureData[Skybox::BACK].Pixels;
		pixelsArray[Skybox::LEFT] = textureData[Skybox::LEFT].Pixels;
		pixelsArray[Skybox::RIGHT] = textureData[Skybox::RIGHT].Pixels;
		pixelsArray[Skybox::TOP] = textureData[Skybox::TOP].Pixels;
		pixelsArray[Skybox::BOTTOM] = textureData[Skybox::BOTTOM].Pixels;

		// Create texture
		tImageDescription imageDesc;
		imageDesc.Format = FORMAT_R8G8B8A8_SRGB;
		imageDesc.Layers = Skybox::COUNT;
		imageDesc.Width = textureData[0].Width;
		imageDesc.Height = textureData[0].Height;
		imageDesc.Depth = 1;
		imageDesc.Flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		imageDesc.MipLevels = 1; // needs cubemap?
		imageDesc.SampleCount = SAMPLE_COUNT_1_BIT;
		cTexture* cubemapTex = cTexture::Create(context, imageDesc);
		// Set image layers with each cubemap texture
		cubemapTex->SetImageLayers(context, pixelsArray, Skybox::COUNT);
		cubemapTex->CreateView(context, {.ViewType = VK_IMAGE_VIEW_TYPE_CUBE });
		//SubmitTexture(cubemapTex);
		skybox.Tex = cubemapTex;

#if 0
		tViewDescription viewDesc;
		viewDesc.BaseArrayLayer = 0;
		viewDesc.LayerCount = Skybox::COUNT;
		viewDesc.ViewType = VK_IMAGE_VIEW_TYPE_CUBE;
		VkDescriptorImageInfo info;
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = cubemapTex->CreateView(context, viewDesc);
		info.sampler = cubemapTex->GetSampler();
		builder.BindImage(0, &info, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		check(builder.Build(context, skybox.CubemapSet));
#endif // 0

		return true;
	}

	cModel* Scene::GetModel(const char* modelName)
	{
		for (uint32_t i = 0; i < m_models.GetSize(); ++i)
		{
			if (!strcmp(m_models[i].GetName(), modelName))
				return &m_models[i];
		}
		return nullptr;
	}

	void Scene::Draw(const RenderContext& context, ShaderProgram* shader, uint32_t materialSetIndex, uint32_t modelSetIndex, VkDescriptorSet modelSet, uint16_t renderFlags) const
	{
		CPU_PROFILE_SCOPE(Scene_Draw);
		VkCommandBuffer cmd = context.GetFrameContext().GraphicsCommandContext.CommandBuffer;
		// Iterate scene graph to render models.
		const cMaterial* lastMaterial = nullptr;
		const cMesh* lastMesh = nullptr;
		uint32_t nodeCount = GetRenderObjectCount();
		index_t renderTransformOffset = 0;
		for (uint32_t i = 0; i < nodeCount; ++i)
		{
			sRenderObject renderObject = i;
			const MeshComponent* meshComponent = GetMesh(renderObject);
			if (meshComponent)
			{
				check(meshComponent->MeshIndex != index_invalid);
				const cModel& model = m_models[meshComponent->MeshIndex];

				// BaseOffset in buffer is already setted when descriptor was created.
				for (index_t j = 0; j < model.m_meshes.GetSize(); ++j)
				{
					uint32_t meshDynamicOffset = (renderTransformOffset+j) * RenderContext_PadUniformMemoryOffsetAlignment(context, sizeof(glm::mat4));
					shader->SetDynamicBufferOffset(context, "u_model", sizeof(glm::mat4), renderTransformOffset++);

					const cMesh& mesh = model.m_meshes[j];
					mesh.BindBuffers(cmd);

					for (index_t k = 0; k < mesh.PrimitiveArray.GetSize(); ++k)
					{
						const PrimitiveMeshData& primitive = mesh.PrimitiveArray[k];
						if (primitive.RenderFlags & renderFlags)
						{
							if (!(renderFlags & RenderFlags_NoTextures))
							{
								check(primitive.Material);
								primitive.Material->BindTextures(context, *shader, materialSetIndex);
								index_t matIndex = primitive.Material - model.m_materials.GetData();
								index_t matIndexBase = m_modelMaterialMap.at(meshComponent->MeshIndex);
								shader->SetDynamicBufferOffset(context, "u_material", sizeof(sMaterialRenderData), matIndexBase + matIndex);
							}
							shader->FlushDescriptors(context);
							RenderAPI::CmdDrawIndexed(cmd, primitive.Count, 1, primitive.FirstIndex, 0, 0);
						}
					}
				}
				++renderTransformOffset;
#if 0
				// Bind vertex/index buffers just if needed
				if (lastMesh != mesh)
				{
					lastMesh = mesh;
					mesh->BindBuffers(cmd);
				}
				// Iterate primitives of current mesh
				for (uint32_t j = 0; j < (uint32_t)mesh->PrimitiveArray.size(); ++j)
				{
					const PrimitiveMeshData& drawData = mesh->PrimitiveArray[j];
					if (drawData.RenderFlags & renderFlags)
					{
						// TODO: material by default if there is no material.
						if (lastMaterial != drawData.Material && !(renderFlags & RenderFlags_NoTextures))
						{
							lastMaterial = drawData.Material;
							drawData.Material->BindTextures(context, *shader, materialSetIndex);
						}
						shader->FlushDescriptors(context);
						RenderAPI::CmdDrawIndexed(cmd, drawData.Count, 1, drawData.FirstIndex, 0, 0);
					}
				}
#endif // 0

			}
		}
	}

	const cTexture* Scene::GetSkyboxTexture() const
	{
		return m_skybox.Tex;
	}

	void Scene::ImGuiDraw()
	{
		ImGui::Begin("SceneGraph");

		float posStep = 0.5f;
		float rotStep = 0.1f;
		float sclStep = 0.5f;

		static char sceneFile[256];
		static bool _b = false;
		if (!_b)
		{
			strcpy_s(sceneFile, "scenes/Scene.yaml");
			_b = !false;
		}
		const RenderContext& context = m_engine->GetContext();
		ImGui::Columns(3);
		if (ImGui::Button("Save"))
			SaveScene(context, sceneFile);
		ImGui::NextColumn();
		if (ImGui::Button("Load"))
			LoadScene(context, sceneFile);
		ImGui::NextColumn();
		ImGui::InputText("Scene file", sceneFile, 256);
		ImGui::Columns();

		ImGui::Separator();
		ImGui::Text("Environment");
		ImGui::ColorEdit3("Ambient color", &m_ambientColor[0]);
		ImGui::Button("Reload");

		ImGui::Separator();
		if (ImGui::TreeNode("Scene tree"))
		{
			for (uint32_t i = 0; i < GetRenderObjectCount(); ++i)
			{
				char treeId[2];
				sprintf_s(treeId, "%u", i);
				if (ImGui::TreeNode(treeId, "%s", GetRenderObjectName(i)))
				{
					glm::mat4 transform;
					TransformComponentToMatrix(&m_transformComponents[i], &transform, 1);
					DebugRender::DrawAxis(transform);
					const Hierarchy& node = m_hierarchy[i];
					ImGui::Text("Parent: %s", node.Parent != UINT32_MAX ? GetRenderObjectName(node.Parent) : "None");
					char buff[32];
					sprintf_s(buff, "##TransformComponent%u", i);
					if (ImGui::TreeNode(buff, "Transform component"))
					{
						TransformComponent& t = m_transformComponents[i];
						ImGui::Columns(2);
						ImGui::Text("Position");
						ImGui::NextColumn();
						sprintf_s(buff, "##TransformPos%d", i);
						bool dirty = ImGui::DragFloat3(buff, &t.Position[0], posStep);
						ImGui::NextColumn();
						sprintf_s(buff, "TransformRot%d", i);
						dirty |= ImGuiUtils::EditAngles(buff, "Rotation", t.Rotation);
						ImGui::NextColumn();
						ImGui::Text("Scale");
						ImGui::NextColumn();
						sprintf_s(buff, "##TransformScl%d", i);
						dirty |= ImGui::DragFloat3(buff, &t.Scale[0], sclStep);
						ImGui::Columns();
						ImGui::TreePop();

						if (dirty)
							MarkAsDirty(i);
					}
					sprintf_s(buff, "##LightComponent%u", i);
					if (m_lightComponentMap.contains(i))
					{
						if (ImGui::TreeNode(buff, "Light component"))
						{
							LightComponent& light = m_lightComponentMap[i];
							static const char* lightTypes[] = { "Point", "Directional", "Spot" };
							uint32_t lightCount = sizeof(lightTypes) / sizeof(const char*);
							if (ImGui::BeginCombo("Type", lightTypes[(uint32_t)light.Type]))
							{
								for (uint32_t j = 0; j < lightCount; ++j)
								{
									if (ImGui::Selectable(lightTypes[j], j == (uint32_t)light.Type))
										light.Type = (ELightType)j;
								}
								ImGui::EndCombo();
							}
							ImGui::Columns(2);
							ImGui::Text("Color");
							ImGui::NextColumn();
							sprintf_s(buff, "##LightColor%u", i);
							ImGui::ColorEdit3(buff, &light.Color[0]);
							ImGui::NextColumn();
							ImGui::Text("Radius");
							ImGui::NextColumn();
							sprintf_s(buff, "##LightRadius%u", i);
							ImGui::DragFloat(buff, &light.Radius, 0.5f, 0.f, FLT_MAX);
							ImGui::NextColumn();
							ImGui::Text("Compression");
							sprintf_s(buff, "##LightCompression%u", i);
							ImGui::NextColumn();
							ImGui::DragFloat(buff, &light.Compression, 0.05f, 0.f, FLT_MAX);
							ImGui::NextColumn();
							ImGui::Text("Outer cutoff");
							ImGui::NextColumn();
							sprintf_s(buff, "##LightOuterCutoff%u", i);
							ImGui::DragFloat(buff, &light.OuterCutoff, 0.1f, 0.f, FLT_MAX);
							ImGui::NextColumn();
							ImGui::Text("Cutoff");
							ImGui::NextColumn();
							sprintf_s(buff, "##LightCutoff%u", i);
							ImGui::DragFloat(buff, &light.Cutoff, 0.1f, 0.f, FLT_MAX);
							ImGui::NextColumn();
							ImGui::Text("Project shadows");
							ImGui::NextColumn();
							sprintf_s(buff, "##LightShadows%u", i);
							ImGui::Checkbox(buff, &light.ProjectShadows);
							ImGui::NextColumn();

							ImGui::Columns();
							ImGui::TreePop();
						}
					}
					if (m_meshComponentMap.contains(i))
					{
						sprintf_s(buff, "##MeshComponent%u", i);
						if (ImGui::TreeNode(buff, "Mesh component"))
						{
							const MeshComponent& meshComp = m_meshComponentMap.at(i);
							ImGui::Text("Model: [%u] %s", meshComp.MeshIndex, meshComp.MeshAssetPath);
							ImGui::Text("Model name: %s", m_models[meshComp.MeshIndex].GetName());
							ImGui::TreePop();
						}
					}

					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Model list"))
		{
			ImGui::Columns(2);
			char buff[32];
			for (index_t i = 0; i < m_models.GetSize(); ++i)
			{
				ImGui::Text("Model: %s", m_models[i].GetName());
				ImGui::NextColumn();
				sprintf_s(buff, "##modelbutton_%d", i);
				ImGui::PushID(buff);
				if (ImGui::Button("..."))
				{ 
					logfinfo("editing model %s\n", m_models[i].GetName());
					m_editingModel = i;
				}
				ImGui::PopID();
				ImGui::NextColumn();
			}
			ImGui::Columns();
			ImGui::TreePop();
		}
		if (m_editingModel != index_invalid)
		{
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
			if (ImGui::BeginChild("EditingModelChild", ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 8), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY))
			{
				ImGui::Text("Editing model %s", m_models[m_editingModel]);
				ImGui::Separator();
				m_models[m_editingModel].ImGuiDraw();
			}
			ImGui::PopStyleColor();
			ImGui::EndChild();
		}
		ImGui::End();
	}

	bool Scene::IsDirty() const
	{
		for (uint32_t i = 0; i < MaxNodeLevel; ++i)
		{
			if (!m_dirtyNodes[i].empty())
				return true;
		}
		return false;
	}

	void Scene::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& frameContext)
	{
		CPU_PROFILE_SCOPE(SceneUpdateRenderData);
		if (GetRenderObjectCount())
		{
			// Update geometry
			RecalculateTransforms();
			check(!IsDirty());
			const glm::mat4& viewMat = frameContext.CameraData->InvView;
			ProcessEnvironmentData(viewMat, m_environmentData);

			index_t offset = 0;
			for (index_t i = 0; i < m_models.GetSize(); ++i)
			{
				m_models[i].UpdateMaterials(m_materials.data() + offset);
				m_modelMaterialMap[i] = offset;
				offset += m_models[i].GetMaterialCount();
			}

			UniformBufferMemoryPool* buffer = &frameContext.GlobalBuffer;
			check(buffer->SetUniform(renderContext, UNIFORM_ID_SCENE_ENV_DATA, &m_environmentData, sizeof(EnvironmentData)));
			//check(buffer->SetUniform(renderContext, UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY, GetRawGlobalTransforms(), GetRenderObjectCount() * sizeof(glm::mat4)));
			//check(buffer->SetDynamicUniform(renderContext, UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY, GetRawGlobalTransforms(), GetRenderObjectCount(), sizeof(glm::mat4), 0));
			check(buffer->SetDynamicUniform(renderContext, UNIFORM_ID_SCENE_MODEL_TRANSFORM_ARRAY, m_renderTransforms.data(), (uint32_t)m_renderTransforms.size(), sizeof(glm::mat4), 0));
			check(buffer->SetDynamicUniform(renderContext, "u_material", m_materials.data(), (uint32_t)m_materials.size(), sizeof(sMaterialRenderData), 0));
#if 0
			// Update materials
			tArray<MaterialUniform, MIST_MAX_MATERIALS> materialUniformBuffer;
			check((uint32_t)m_materialArray.size() <= MIST_MAX_MATERIALS);
			for (uint32_t i = 0; i < (uint32_t)m_materialArray.size(); ++i)
			{
				//check(m_dirtyMaterials[i] < (uint32_t)m_materialArray.size());
				Material& material = m_materialArray[i];
				//if (material.IsDirty())
				{
					RenderHandle h = material.GetHandle();
					MaterialRenderData& mrd = m_materialRenderDataArray[h];
					//if (material.IsDirty())
					{
						auto submitTexture = [&](RenderHandle texHandle, uint32_t binding, uint32_t arrayIndex)
							{
								Texture* texture;
								if (texHandle.IsValid())
								{
									texture = m_renderData.Textures[texHandle];
								}
								else
								{
									RenderHandle defTex = m_engine->GetDefaultTexture();
									texture = m_renderData.Textures[defTex];
								}
								BindDescriptorTexture(m_engine->GetContext(), texture, mrd.MaterialSets[frameContext.FrameIndex].TextureSet, binding, arrayIndex);
								mrd.Textures[arrayIndex] = texture;
							};
						submitTexture(material.GetAlbedoTexture(), 0, 0);
						submitTexture(material.GetNormalTexture(), 0, 1);
						submitTexture(material.GetSpecularTexture(), 0, 2);
						submitTexture(material.GetOcclusionTexture(), 0, 3);
						submitTexture(material.GetMetallicTexture(), 0, 4);
						submitTexture(material.GetRoughnessTexture(), 0, 5);
					}

					check(i < MIST_MAX_MATERIALS);
					materialUniformBuffer[i].Metallic = material.GetMetallic();
					materialUniformBuffer[i].Roughness = material.GetRoughness();
					
					material.SetDirty(false);
				}
			}
			buffer->SetDynamicUniform(renderContext, "Material", materialUniformBuffer.data(), MIST_MAX_MATERIALS, sizeof(MaterialUniform), 0);

			// Integrity check
			for (uint32_t i = 0; i < (uint32_t)m_materialArray.size(); ++i)
			{
				check(!m_materialArray[i].IsDirty());
			}
#endif
		}
	}

	const glm::mat4* Scene::GetRawGlobalTransforms() const
	{
		check(!IsDirty());
		return m_globalTransforms.data();
	}


	void Scene::ProcessEnvironmentData(const glm::mat4& viewSpace, EnvironmentData& environmentData)
	{
		CPU_PROFILE_SCOPE(ProcessEnvData);
		environmentData.ViewPosition = math::GetPos(glm::inverse(viewSpace));
#if 1
		environmentData.AmbientColor = m_ambientColor;
		environmentData.ActiveLightsCount = 0.f;
		environmentData.ActiveSpotLightsCount = 0.f;
		uint32_t shadowMapIndex = 0;
		for (uint32_t i = 0; i < GetRenderObjectCount(); ++i)
		{
			if (m_lightComponentMap.contains(i))
			{
				const glm::mat4& mat = m_globalTransforms[i];
				const glm::vec3 pos = math::GetPos(viewSpace * mat);
				const glm::vec3 dir = -1.f*math::GetDir(viewSpace * mat);
				const LightComponent& light = m_lightComponentMap[i];
				switch (light.Type)
				{
				case ELightType::Point:
				{
					LightData& data = environmentData.Lights[(uint32_t)environmentData.ActiveLightsCount++];
					data.Color = light.Color;
					data.Compression = light.Compression;
					data.Position = pos;
					data.Radius = light.Radius;
				}
					break;
				case ELightType::Directional:
					environmentData.DirectionalLight.Color = light.Color;
					environmentData.DirectionalLight.ShadowMapIndex = light.ProjectShadows ? (float)shadowMapIndex++ : -1.f;
					environmentData.DirectionalLight.Direction = dir;
					break;
				case ELightType::Spot:
				{
					SpotLightData& data = environmentData.SpotLights[(uint32_t)environmentData.ActiveSpotLightsCount++];
					data.Color = light.Color;
					data.ShadowMapIndex = light.ProjectShadows ? (float)shadowMapIndex++ : -1.f;
					data.Position = pos;
					data.Direction = dir;
					data.CosOuterCutoff = cosf(glm::radians(light.OuterCutoff));
					data.CosCutoff = cosf(glm::radians(light.Cutoff));
					data.Radius = light.Radius;
					data.Compression = light.Compression;
				}
					break;
				}
			}
		}
		check(shadowMapIndex <= globals::MaxShadowMapAttachments);
#endif // 0

	}
}

