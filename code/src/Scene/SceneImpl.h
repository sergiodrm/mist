#pragma once
// Autogenerated code for Mist project
// Header file

#include "Scene/Scene.h"
//#include "Render/VulkanRenderEngine.h"
#include "Render/VulkanBuffer.h"
#include "Render/Texture.h"
#include "Render/Globals.h"

#define MIST_MAX_MATERIALS 50

namespace Mist
{
	struct RenderContext;
	struct RenderFrameContext;
	class IRenderEngine;
	class ShaderProgram;
	class DescriptorLayoutCache;
	class DescriptorAllocator;

	struct MaterialUniform
	{
		float Metallic = 0.f;
		float Roughness = 0.f;
	};

	struct MaterialRenderData
	{
		struct FrameData
		{
			VkDescriptorSet TextureSet;
		};

		VkDescriptorSetLayout Layout{ VK_NULL_HANDLE };
		tArray<FrameData, globals::MaxOverlappedFrames> MaterialSets;

		static VkDescriptorSetLayout GetDescriptorSetLayout(const RenderContext& renderContext);
		void Init(const RenderContext& renderContext);
		void Destroy(const RenderContext& renderContext);
	};

	struct PrimitiveMeshData
	{
		uint32_t FirstIndex;
		uint32_t Count;
		uint32_t MaterialIndex;
		PrimitiveMeshData() : FirstIndex(0), Count(0), MaterialIndex(UINT32_MAX) {}
	};

	struct MeshRenderData
	{
		VertexBuffer VertexBuffer;
		IndexBuffer IndexBuffer;
		uint32_t IndexCount;
		tDynArray<PrimitiveMeshData> PrimitiveArray;

		void BindBuffers(VkCommandBuffer cmd) const;
	};

	struct RenderDataContainer
	{
		template <typename RenderResourceType>
		using ResourceMap = std::unordered_map<RenderHandle, RenderResourceType, RenderHandle::Hasher>;
		ResourceMap<Texture*> Textures;
		ResourceMap<MeshRenderData> Meshes;
	};

	struct LightData
	{
		union
		{
			glm::vec3 Position;
			glm::vec3 Direction;
		};
		union
		{
			float Radius;
			float ShadowMapIndex;
		};
		glm::vec3 Color;
		float Compression;
	};

	struct SpotLightData
	{
		glm::vec3 Color;
		float ShadowMapIndex;
		glm::vec3 Direction;
		float InnerCutoff;
		glm::vec3 Position;
		float OuterCutoff;
	};

	struct EnvironmentData
	{
		glm::vec3 AmbientColor;
		float ActiveSpotLightsCount;
		glm::vec3 ViewPosition;
		float ActiveLightsCount;
		static constexpr uint32_t MaxLights = 8;
		LightData Lights[MaxLights];
		LightData DirectionalLight;
		SpotLightData SpotLights[MaxLights];

		EnvironmentData();
	};

	struct Skybox
	{
		enum
		{
			FRONT, 
			BACK, 
			LEFT, 
			RIGHT, 
			TOP, 
			BOTTOM,
			COUNT
		};
		uint32_t MeshIndex;
		VkDescriptorSet CubemapSet;

		char CubemapFiles[COUNT][256];

		Skybox() : MeshIndex(UINT32_MAX), CubemapSet(VK_NULL_HANDLE) 
		{ 
			for (uint32_t i = 0; i < COUNT; ++i)
				*CubemapFiles[i] = 0;
		}
	};

	class Scene : public IScene
	{
	protected:
		Scene(const Scene&) = delete;
		Scene(Scene&&) = delete;
		void operator=(const Scene&) = delete;
		void operator=(Scene&&) = delete;
	public:
		Scene(IRenderEngine* engine);
		~Scene();

		virtual void Init() override;
		virtual void Destroy() override;

		void InitFrameData(const RenderContext& renderContext, RenderFrameContext& frameContext);

		virtual bool LoadModel(const char* filepath) override;
		void LoadScene(const char* filepath);
		void SaveScene(const char* filepath);

		virtual RenderObject CreateRenderObject(RenderObject parent) override;
		virtual void DestroyRenderObject(RenderObject object) override;
		virtual bool IsValid(RenderObject object) const override;
		virtual uint32_t GetRenderObjectCount() const override;

		virtual Material* CreateMaterial() override;
		virtual Material* GetMaterial(uint32_t index) override;
		virtual const Material* GetMaterial(uint32_t index) const override;

		virtual RenderObject GetRoot() const override;
		virtual const MeshComponent* GetMesh(RenderObject renderObject) const override;
		virtual void SetMesh(RenderObject renderObject, const MeshComponent& meshComponent) override;
		virtual const char* GetRenderObjectName(RenderObject object) const override;
		virtual void SetRenderObjectName(RenderObject renderObject, const char* name) override;
		virtual const TransformComponent& GetTransform(RenderObject renderObject) const override;
		virtual void SetTransform(RenderObject renderObject, const TransformComponent& transform) override;
		virtual const LightComponent* GetLight(RenderObject renderObject) const override;
		virtual void SetLight(RenderObject renderObject, const LightComponent& light) override;
		virtual uint32_t SubmitMesh(Mesh& mesh) override;
		virtual uint32_t SubmitMaterial(Material& material) override;
		virtual RenderHandle LoadTexture(const char* texturePath) override;
		RenderHandle LoadTexture(const RenderContext& context, const char* texturePath, EFormat format);
		RenderHandle SubmitTexture(Texture* tex);
		void UpdateMaterialBindings(Material& material);

		void MarkAsDirty(RenderObject renderObject);

		const glm::mat4* GetRawGlobalTransforms() const;

		const Mesh* GetMeshArray() const;
		uint32_t GetMeshCount() const;

		const Material* GetMaterialArray() const;
		uint32_t GetMaterialCount() const;

		const MeshRenderData& GetMeshRenderData(RenderHandle handle) const;
		MeshRenderData& GetMeshRenderData(RenderHandle handle);
		const MaterialRenderData& GetMaterialRenderData(RenderHandle handle) const;
		MaterialRenderData& GetMaterialRenderData(RenderHandle handle);

		void UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& frameContext);

		// Draw with materials
		void Draw(VkCommandBuffer cmd, ShaderProgram* shader, uint32_t materialSetIndex, uint32_t modelSetIndex, VkDescriptorSet modelSet) const;
		// Draw without materials
		void Draw(VkCommandBuffer cmd, ShaderProgram* shader, uint32_t modelSetIndex, VkDescriptorSet modelSet) const;
		void DrawSkybox(CommandBuffer cmd, ShaderProgram* shader);

		void ImGuiDraw();
		bool IsDirty() const;
		const EnvironmentData& GetEnvironmentData() const { return m_environmentData; }
	protected:
		void ProcessEnvironmentData(const glm::mat4& viewMatrix, EnvironmentData& environmentData);
		void RecalculateTransforms();
		bool LoadMeshesFromFile(const char* filepath);
		bool LoadSkybox(const RenderContext& context, Skybox& skybox, const char* front, const char* back, const char* left, const char* right, const char* top, const char* bottom);

	private:
		class VulkanRenderEngine* m_engine{nullptr};
		static constexpr uint32_t MaxNodeLevel = 16;
		tDynArray<tString> m_names;
		tDynArray<tString> m_materialNames;
		tDynArray<Hierarchy> m_hierarchy;
		tDynArray<TransformComponent> m_transformComponents;
		tMap<uint32_t, MeshComponent> m_meshComponentMap;
		tMap<uint32_t, LightComponent> m_lightComponentMap;
		tDynArray<Material> m_materialArray;
		tDynArray<MaterialRenderData> m_materialRenderDataArray;
		tDynArray<uint32_t> m_dirtyMaterials;
		tDynArray<Mesh> m_meshArray;

		tDynArray<glm::mat4> m_localTransforms;
		tDynArray<glm::mat4> m_globalTransforms;
		
		tDynArray<int32_t> m_dirtyNodes[MaxNodeLevel];

		RenderDataContainer m_renderData;
		glm::vec3 m_ambientColor = {0.05f, 0.05f, 0.05f};

		tMap<tString, tDynArray<uint32_t>> m_meshNameIndexMap;
		Skybox m_skybox;
		uint32_t m_defaultMaterialIndex = 0;
		tArray<VkDescriptorSet, globals::MaxOverlappedFrames> m_materialSetArray;
		EnvironmentData m_environmentData;
	};
}
