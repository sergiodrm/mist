#pragma once
// Autogenerated code for vkmmc project
// Header file

#include "Scene.h"
//#include "VulkanRenderEngine.h"
#include "VulkanBuffer.h"
#include "Texture.h"


namespace vkmmc
{
	struct RenderContext;
	class IRenderEngine;
	class ShaderProgram;
	class DescriptorLayoutCache;
	class DescriptorAllocator;

	struct MaterialRenderData
	{
		VkDescriptorSetLayout Layout{ VK_NULL_HANDLE };
		VkDescriptorSet Set{ VK_NULL_HANDLE };
		VkSampler Sampler{ VK_NULL_HANDLE };

		static VkDescriptorSetLayout GetDescriptorSetLayout(const RenderContext& renderContext, DescriptorLayoutCache& layoutCache);
		void Init(const RenderContext& renderContext, DescriptorAllocator& descAllocator, DescriptorLayoutCache& layoutCache);
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
		std::vector<PrimitiveMeshData> PrimitiveArray;

		void BindBuffers(VkCommandBuffer cmd) const;
	};

	struct RenderDataContainer
	{
		template <typename RenderResourceType>
		using ResourceMap = std::unordered_map<RenderHandle, RenderResourceType, RenderHandle::Hasher>;
		ResourceMap<Texture> Textures;
		ResourceMap<MeshRenderData> Meshes;
		ResourceMap<MaterialRenderData> Materials;
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

	class Scene : public IScene
	{
		struct RenderObjectComponents
		{
			uint32_t MeshIndex = UINT32_MAX;
			uint32_t LightIndex = UINT32_MAX;
		};
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

		virtual bool LoadModel(const char* filepath) override;

		virtual RenderObject CreateRenderObject(RenderObject parent) override;
		virtual void DestroyRenderObject(RenderObject object) override;
		virtual bool IsValid(RenderObject object) const override;
		virtual uint32_t GetRenderObjectCount() const override;

		virtual RenderObject GetRoot() const override;
		virtual const Mesh* GetMesh(RenderObject renderObject) const override;
		virtual void SetMesh(RenderObject renderObject, const Mesh& mesh) override;
		virtual const char* GetRenderObjectName(RenderObject object) const override;
		virtual void SetRenderObjectName(RenderObject renderObject, const char* name) override;
		virtual const glm::mat4& GetTransform(RenderObject renderObject) const override;
		virtual void SetTransform(RenderObject renderObject, const glm::mat4& transform) override;
		virtual const Light* GetLight(RenderObject renderObject) const override;
		virtual void SetLight(RenderObject renderObject, const Light& light) override;
		virtual void SubmitMesh(Mesh& mesh) override;
		virtual void SubmitMaterial(Material& material) override;
		virtual RenderHandle LoadTexture(const char* texturePath) override;

		void MarkAsDirty(RenderObject renderObject);

		const glm::mat4* GetRawGlobalTransforms() const;

		const Mesh* GetMeshArray() const;
		uint32_t GetMeshCount() const;

		const Light* GetLightArray() const;
		uint32_t GetLightCount() const;

		const Material* GetMaterialArray() const;
		uint32_t GetMaterialCount() const;

		const MeshRenderData& GetMeshRenderData(RenderHandle handle) const;
		MeshRenderData& GetMeshRenderData(RenderHandle handle);
		const MaterialRenderData& GetMaterialRenderData(RenderHandle handle) const;
		MaterialRenderData& GetMaterialRenderData(RenderHandle handle);

		void UpdateRenderData(const RenderContext& renderContext, UniformBuffer* buffer, const glm::vec3& viewPosition);
		inline const EnvironmentData& GetEnvironmentData() const { return m_environmentData; }
		inline EnvironmentData& GetEnvironmentData() { return m_environmentData; }

		// Draw with materials
		void Draw(VkCommandBuffer cmd, ShaderProgram* shader, uint32_t materialSetIndex, uint32_t modelSetIndex, VkDescriptorSet modelSet) const;
		// Draw without materials
		void Draw(VkCommandBuffer cmd, ShaderProgram* shader, uint32_t modelSetIndex, VkDescriptorSet modelSet) const;

		void ImGuiDraw(bool createWindow = false);

	protected:
		void ProcessEnvironmentData(const glm::vec3& view);
		void RecalculateTransforms();

	private:
		class VulkanRenderEngine* m_engine{nullptr};
		static constexpr uint32_t MaxNodeLevel = 16;
		std::vector<glm::mat4> m_localTransforms;
		std::vector<glm::mat4> m_globalTransforms;
		std::vector<Hierarchy> m_hierarchy;
		std::vector<std::string> m_names;
		std::vector<std::string> m_materialNames;
		std::unordered_map<uint32_t, RenderObjectComponents> m_componentMap;
		std::vector<Mesh> m_meshArray;
		std::vector<Material> m_materialArray;
		std::vector<Light> m_lightArray;

		std::vector<int32_t> m_dirtyNodes[MaxNodeLevel];

		RenderDataContainer m_renderData;
		EnvironmentData m_environmentData;
	};
}
