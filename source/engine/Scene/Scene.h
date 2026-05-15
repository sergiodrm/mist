#pragma once

#include "Render/Globals.h"
#include "Render/Model.h"
#include "Render/Material.h"
#include "Core/Types.h"
#include <glm/glm.hpp>
#include "Utils/Angles.h"
#include "Utils/FileSystem.h"
#include "Render/Camera.h"

#define MIST_MAX_MODELS 128
#define MIST_MAX_CAMERAS 4

namespace Mist
{
	class IRenderEngine;
	class cTexture;
	class cModel;
	struct PreprocessIrradianceInfo;

	struct sRenderObject
	{
		index_t Id = index_invalid;
		sRenderObject() {}
		sRenderObject(index_t v) : Id(v) {}
		operator index_t() const { return Id; }
		inline bool IsValid() const { return Id != index_invalid; }
	};


	enum class ELightType
	{
		Point,
		Directional,
		Spot
	};

	const char* LightTypeToStr(ELightType);
	ELightType StrToLightType(const char* str);

	struct LightComponent
	{
		ELightType Type;
		glm::vec3 Color;
		float Strength;
		float Radius;
		float Compression;
		float OuterCutoff;	// Degrees
		float Cutoff;		// Degrees

		bool ProjectShadows;
		bool Enabled;
		float OrthoLeft;
		float OrthoRight;
		float OrthoBottom;
		float OrthoTop;
		float NearClip;
		float FarClip;

		LightComponent()
			: Type(ELightType::Point),
			Color({ 1.f,1.f,1.f }),
			Radius(100.f),
			Strength(1.f),
			Compression(1.f),
			OuterCutoff(30.f),
			Cutoff(30.f),
			ProjectShadows(false),
			Enabled(true),
			OrthoLeft(-160.f),
			OrthoRight(160.f),
			OrthoBottom(-120.f),
			OrthoTop(120.f),
			NearClip(1.f),
			FarClip(100.f)
		{ }
	};

	struct MeshComponent
	{
		uint32_t MeshIndex;

		MeshComponent() : MeshIndex(UINT32_MAX) { }
	};

	struct CameraComponent
	{
		bool Main;
		index_t CameraIndex;

		CameraComponent() : Main(false), CameraIndex(index_invalid) {}
		CameraComponent(index_t i) : Main(false), CameraIndex(i) {}
	};

	struct Hierarchy
	{
		sRenderObject Parent;
		sRenderObject Sibling;
		sRenderObject Child;
		int32_t Level = 0;
	};

	struct TransformComponent
	{
		glm::vec3 Position;
		tAngles Rotation;
		glm::vec3 Scale;
	};

	void TransformComponentToMatrix(const TransformComponent* transforms, glm::mat4* matrices, uint32_t count);

	struct LightData
	{
		glm::vec3 color;
		float compression;

		glm::vec3 position;
		float radius;

		glm::vec3 direction;
		float _padding;

		glm::vec2 cosCutoff;
		int shadowMapIndex;
		float lightStrength;

		inline void Set(const LightComponent& lightComponent, const glm::vec3& position, const glm::vec3& direction, int shadowMapIndex)
		{
			color = lightComponent.Color;
			compression = lightComponent.Compression;
			this->position = position;
			this->direction = direction;
			this->shadowMapIndex = shadowMapIndex;
			radius = lightComponent.Radius;
			lightStrength = lightComponent.Strength;
			if (lightComponent.Type == ELightType::Spot)
			{
				cosCutoff.y = cosf(glm::radians(lightComponent.OuterCutoff));
				cosCutoff.x = cosf(glm::radians(lightComponent.Cutoff));
			}
		}
	};

	struct EnvironmentData
	{
		glm::vec3 ambientColor;
		int spotLightsCount;
		int directionalLightsCount;
		int pointLightsCount;
		glm::vec2 _padding;
		static constexpr uint32_t MaxLights = 500;
		static constexpr uint32_t MaxPointLights = MaxLights;
		static constexpr uint32_t MaxSpotLights = MaxLights;
		static constexpr uint32_t MaxDirectionalLights = 5;
		LightData pointLights[MaxLights];
		LightData directionalLights[MaxDirectionalLights];
		LightData spotLights[MaxLights];

		EnvironmentData();

		inline void PushLight(const LightComponent& lightComponent, const glm::vec3& position, const glm::vec3& direction, int shadowMapIndex)
		{
			LightData* data = nullptr;
			switch (lightComponent.Type)
			{
			case ELightType::Point:
				if (pointLightsCount < MaxPointLights)
					data = &pointLights[pointLightsCount++];
				break;
			case ELightType::Directional:
				if (directionalLightsCount < MaxDirectionalLights)
					data = &directionalLights[directionalLightsCount++];
				break;
			case ELightType::Spot:
				if (spotLightsCount < MaxSpotLights)
					data = &spotLights[spotLightsCount++];
				break;
			}
			if (data)
				data->Set(lightComponent, position, direction, shadowMapIndex);
		}

		inline void Reset() { spotLightsCount = 0; directionalLightsCount = 0; pointLightsCount = 0; }
	};

	struct CameraData
	{
		glm::mat4 View;
		glm::mat4 InvView;
		glm::mat4 Projection;
		glm::mat4 JitteredProjection;
		glm::mat4 InvProjection;
		glm::mat4 ViewProjection;
		glm::mat4 InvViewProjection;
		glm::mat4 JitteredViewProjection;

		inline void Set(const glm::mat4& view, const glm::mat4& projection)
		{
			View = view;
			InvView = glm::inverse(view);
			Projection = projection;
			InvProjection = glm::inverse(projection);
			ViewProjection = projection * view;
			InvViewProjection = glm::inverse(ViewProjection);
		}

		inline void Set(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& jitteredProjection)
		{
			Set(view, projection);
			JitteredProjection = jitteredProjection;
			JitteredViewProjection = jitteredProjection * view;
		}
	};

	struct Skybox
	{
		enum
		{
			FRONT, 
			BACK, 
			TOP, 
			BOTTOM,
			RIGHT, 
			LEFT, 
			COUNT
		};
		render::TextureHandle texture;
		char CubemapFiles[COUNT][256];

		Skybox()
		{ 
			texture = nullptr;
			for (uint32_t i = 0; i < COUNT; ++i)
				*CubemapFiles[i] = 0;
		}
	};

	struct IrradianceCube
	{
		render::TextureHandle brdf;
		render::TextureHandle cubemap;
		render::TextureHandle irradiance;
		render::TextureHandle specular;
		char filepath[MaxFilenameLength];
	};

	class Scene
	{
	protected:
		Scene(const Scene&) = delete;
		Scene(Scene&&) = delete;
		void operator=(const Scene&) = delete;
		void operator=(Scene&&) = delete;
	public:
		Scene(IRenderEngine* engine);
		~Scene();

		void Init();
		void Destroy();

		void Tick(float deltaTime);

		const CameraController& GetCamera() const;
		CameraController& GetCamera();

		void LoadScene(const char* filepath);
		void SaveScene(const char* filepath);

		sRenderObject CreateRenderObject(sRenderObject parent);
		void DestroyRenderObject(sRenderObject object);
		bool IsValid(sRenderObject object) const;
		uint32_t GetRenderObjectCount() const;

		sRenderObject GetRoot() const;

		const MeshComponent* GetMesh(sRenderObject renderObject) const;
		void SetMesh(sRenderObject renderObject, const MeshComponent& meshComponent);
		const cModel* GetModel(uint32_t index) const { return &m_models[index]; }

		const char* GetRenderObjectName(sRenderObject object) const;
		void SetRenderObjectName(sRenderObject renderObject, const char* name);

		const TransformComponent& GetTransform(sRenderObject renderObject) const;
		void SetTransform(sRenderObject renderObject, const TransformComponent& transform);

		const LightComponent* GetLight(sRenderObject renderObject) const;
		void SetLight(sRenderObject renderObject, const LightComponent& light);

		void MarkAsDirty(sRenderObject renderObject);

		const glm::mat4* GetRawGlobalTransforms() const;

		void UpdateRenderData();
		
		// can be nullptr
		render::TextureHandle GetSkyboxTexture() const;
		void SetSkyboxTexture(const render::TextureHandle& t) { m_skybox.texture = t; }
		const IrradianceCube& GetIrradianceCube() const { return m_irradianceCube; }

		void ImGuiDraw();
		bool IsDirty() const;
		const EnvironmentData& GetEnvironmentData() const { return m_environmentData; }

	protected:
		void ProcessEnvironmentData(EnvironmentData& environmentData);
		void RecalculateTransforms();
		bool LoadSkybox(Skybox& skybox, const char* front, const char* back, const char* left, const char* right, const char* top, const char* bottom);
		bool LoadIrradianceCube(const PreprocessIrradianceInfo& info);

		const cModel* GetModel(const char* modelName) const { return const_cast<Scene*>(this)->GetModel(modelName); }
		cModel* GetModel(const char* modelName);
		index_t LoadModel(const char* filepath);
		void ImGuiDrawModel(const cModel* model, const glm::mat4& transform);

		index_t NewCamera();
		void SetCamera(sRenderObject r, const CameraComponent& cameraIndex);

	private:
		class VulkanRenderEngine* m_engine{nullptr};
		static constexpr index_t MaxNodeLevel = 16;
		char m_sceneFile[MaxFilenameLength];
		tFixedHeapArray<String> m_names;
		tFixedHeapArray<Hierarchy> m_hierarchy;
		tFixedHeapArray<TransformComponent> m_transformComponents;
		tMap<index_t, MeshComponent> m_meshComponentMap;
		tMap<index_t, LightComponent> m_lightComponentMap;
		tMap<index_t, CameraComponent> m_cameraComponentMap;

		tStaticArray<cModel, MIST_MAX_MODELS> m_models;
		tStaticArray<CameraController, MIST_MAX_CAMERAS> m_cameras;

		tFixedHeapArray<glm::mat4> m_localTransforms;
		tFixedHeapArray<glm::mat4> m_globalTransforms;
		tFixedHeapArray<glm::mat4> m_renderTransforms;
		index_t m_editingModel = index_invalid;
		
		tFixedHeapArray<index_t> m_dirtyNodes[MaxNodeLevel];

		glm::vec3 m_ambientColor = {0.05f, 0.05f, 0.05f};

		Skybox m_skybox;
		IrradianceCube m_irradianceCube;
		EnvironmentData m_environmentData;

		index_t m_cameraIndex = index_invalid;

		PreprocessIrradianceInfo* m_irradianceRequestInfo;
	};

	struct RenderItem
	{
		uint32_t primitive;
		const cMesh* mesh;
		glm::mat4 transform;
	};

	struct RenderPass
	{
		tDynArray<RenderItem> items;
		tDynArray<AABB_t> cullingData;
		tDynArray<uint32_t> drawList;

		RenderPass() = default;
		DELETE_COPY_CONSTRUCTORS(RenderPass);

		inline void Clear()
		{
			items.clear();
			cullingData.clear();
			drawList.clear();
		}
	};

	struct RenderPassInfo
	{
		RenderPassType pass{ RenderPass_None };
		CameraData cameraData {};
	};

	struct RenderContext
	{
		uint32_t passId = UINT32_MAX;
		rendersystem::RenderSystem* rs = nullptr;
	};

	class SceneRenderer
	{
	public:

		SceneRenderer(uint32_t size = 4);
		~SceneRenderer();

		uint32_t CreateRenderList(const RenderPassInfo& info);
		void SetRenderListInfo(uint32_t id, const RenderPassInfo& info);

		void BuildRenderLists(const Scene* scene);
		void DrawList(const RenderContext& renderContext);

		void ImGuiDraw();

		static void Init();
		static void Destroy();
		static SceneRenderer* GetSceneRenderer();
	private:
		void ProcessModelNode(const cModel* model, index_t nodeIndex, const glm::mat4& parentTransform, const glm::mat4& worldTransform);
		void ProcessMesh(const cMesh& mesh, const glm::mat4& nodeWorldTransform, const glm::mat4& modelWorldTransform);

		void BindMesh(rendersystem::RenderSystem* rs, const RenderItem& item);
		void BindMaterial(rendersystem::RenderSystem* rs, const cMaterial& material);

		void DoCulling();

		void DrawItem(const RenderContext& renderContext, const RenderItem& item, const cMesh*& lastMesh, const cMaterial*& lastMaterial);
		void DrawGeometryItem(const RenderContext& renderContext, const RenderItem& item);

	private:
		tFixedHeapArray<RenderPassInfo> m_creationInfo;
		tFixedHeapArray<RenderPass> m_renderPasses;
	};
}
