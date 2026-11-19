#pragma once

#include "Core/Types.h"
#include "Scene.h"

namespace Mist
{
	class cModel;
	class cMaterial;
	class cMesh;

    struct RenderItem
	{
		uint16_t primitive;
	};

	struct RenderSortId
	{
		uint64_t id;

		inline void Set(uint16_t material, uint16_t mesh, uint16_t sort, uint16_t index)
		{
			id = uint64_t(material) << 48 | uint64_t(mesh) << 32 | uint64_t(sort) << 16 | uint64_t(index);
		}

		inline uint16_t GetMaterial() const { return (id & 0xffff000000000000) >> 48; }
		inline uint16_t GetMesh() const { return (id & 0x0000ffff00000000) >> 32; }
		inline uint16_t GetIndex() const { return (id & 0x000000000000ffff); }
	};

	struct RenderPass
	{
		tFixedHeapArray<RenderItem> m_items;
		tFixedHeapArray<RenderSortId> m_sortIds;

		RenderPass(uint32_t size)
		{
			m_items.Allocate(size);
			m_sortIds.Allocate(size);
		}

		~RenderPass()
		{
			m_items.Delete();
			m_sortIds.Delete();
		}

		DELETE_COPY_CONSTRUCTORS(RenderPass);

		inline void Clear()
		{
			m_items.Clear();
			m_sortIds.Clear();
		}
	};

	struct RenderData_Id
	{
		int modelId;
		int materialId;
		int _padding[2];
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
		struct RenderState
		{
			const cMesh* mesh = nullptr;
			const cMaterial* material = nullptr;
			uint32_t transformIndex = UINT32_MAX;

			RenderState() = default;
			inline void Invalidate() { *this = RenderState(); }
		};

		class RenderListBuilder
		{
		public:
			RenderListBuilder(SceneRenderer& renderer) : m_sceneRenderer(renderer) {}
			void Build(const Scene* scene);
		private:
			void ProcessModelNode(const cModel* model, index_t nodeIndex, const glm::mat4& parentTransform, uint16_t materialRoot);
			void ProcessMesh(const cModel& model, const cMesh& mesh, const glm::mat4& nodeWorldTransform, uint16_t materialRoot);
			uint16_t ProcessMaterials(const cModel* model);
		private:
			SceneRenderer& m_sceneRenderer;
		};

	public:

		SceneRenderer(rendersystem::RenderSystem* rs, uint32_t passCount = 8, uint32_t maxItemCount = 1 << 12);
		~SceneRenderer();

		uint32_t CreateRenderList(const RenderPassInfo& info);
		void SetRenderListInfo(uint32_t id, const RenderPassInfo& info);

		void BuildRenderLists(rendersystem::RenderSystem* rs, const Scene* scene);
		void DrawList(const RenderContext& renderContext);

		inline const cMaterial* GetDebugMaterial() const { return m_debugMaterial; }
		inline void SetDebugMaterial(const cMaterial* material) { m_debugMaterial = material; }

		void ImGuiDraw();

		static void Init(rendersystem::RenderSystem* rs);
		static void Destroy();
		static SceneRenderer* GetSceneRenderer();
	private:

		void BindMesh(const RenderContext& rc, const cMesh& mesh);
		void BindMaterial(const RenderContext& rc, const cMaterial& material);

		void DoCulling();

		void DrawItem(const RenderContext& renderContext, const RenderSortId& id);
		void DrawGeometryItem(const RenderContext& renderContext, const RenderSortId& id);

		bool IsVisible(const AABB_t& aabb, const Frustum& frustum) const
		{
			if (m_enableCulling)
				return IsAABBVisibleConditional(aabb, frustum);
			return true;
		}

	private:
		/**
		 * MaxItems = N = 4096
		 * size of RenderSortId = 8 bytes
		 * size of RenderItem = 2 bytes
		 * ~size per RenderPass = N * 8 + N * 2 = 32768 + 8192
		 *
		 * transforms = N * 16 * 4 = N * 64 = 262144
		 * materialRD = N * 64 = 262144
		 * materials = N * 8 = 32768
		 * meshes = N * 8 = 32768
		 * cullingD = N * 24 = 98304
		 *
		 * total = 729088 bytes = 712 Kb
		 */

		tFixedHeapArray<RenderPassInfo> m_creationInfo;
		tFixedHeapArray<RenderPass> m_renderPasses;
		RenderState m_state;

		tFixedHeapArray<glm::mat4> m_transforms;
		tFixedHeapArray<sMaterialRenderData> m_materialRenderData;
		tFixedHeapArray<const cMaterial*> m_materials;
		tFixedHeapArray<const cMesh*> m_meshes;
		tFixedHeapArray<AABB_t> m_cullingData;

		RingBufferContainer<render::BufferHandle> m_ssboTransforms;
		RingBufferContainer<render::BufferHandle> m_ssboMaterials;

		const cMaterial* m_debugMaterial;

		bool m_enableSorting = true;
		bool m_enableCulling = true;
	};
}