#include "SceneRenderer.h"
#include "RenderSystem/RenderSystem.h"
#include "RenderSystem/UI.h"
#include "imgui.h"


namespace Mist
{

	CStrVar CVar_RenderInclude("r_renderInclude", "");
	CStrVar CVar_RenderExclude("r_renderExclude", "");
	CFloatVar CVar_RenderWireframe("r_wireframe", 0.f);


	bool SceneFilter(const char* name)
	{
		if (*CVar_RenderInclude.Get() && !WildStricmp(CVar_RenderInclude.Get(), name))
			return false;
		if (*CVar_RenderExclude.Get() && WildStricmp(CVar_RenderExclude.Get(), name))
			return false;
		return true;
	}

	SceneRenderer* g_sceneRenderer = nullptr;

    static int CompareRenderSortId(void* context, const void* item0, const void* item1)
	{
		const RenderSortId& id0 = *static_cast<const RenderSortId*>(item0);
		const RenderSortId& id1 = *static_cast<const RenderSortId*>(item1);
		if (id0.id == id1.id) return 0;
		if (id0.id > id1.id) return 1;
		return -1;
	}

	SceneRenderer::SceneRenderer(rendersystem::RenderSystem* rs, uint32_t passCount, uint32_t maxItemCount)
	{
		m_creationInfo.Allocate(passCount);
		m_renderPasses.Allocate(passCount);
		m_meshes.Allocate(maxItemCount);
		m_transforms.Allocate(maxItemCount);
		m_materials.Allocate(maxItemCount);
		m_materialRenderData.Allocate(maxItemCount);
		m_cullingData.Allocate(maxItemCount);
		m_ssboTransforms.Init(rs->GetFramesInFlightCount());
		m_ssboMaterials.Init(rs->GetFramesInFlightCount());

		render::MemoryUsage memUsage = render::MemoryUsage_CpuToGpu;
		render::BufferUsage bufferUsage = render::BufferUsage_StorageBuffer;

		for (uint32_t i = 0; i < m_ssboMaterials.GetCount(); ++i)
		{
			m_ssboTransforms.Get(i) = render::utils::CreateStorageBuffer(rs->GetDevice(), sizeof(glm::mat4) * m_transforms.GetReservedSize(), bufferUsage, memUsage, "Transforms SSBO");
			m_ssboMaterials.Get(i) = render::utils::CreateStorageBuffer(rs->GetDevice(), sizeof(sMaterialRenderData) * m_materialRenderData.GetReservedSize(), bufferUsage, memUsage, "Materials SSBO");
		}
	}

	SceneRenderer::~SceneRenderer()
	{
		m_renderPasses.Delete();
		m_creationInfo.Delete();
		m_meshes.Delete();
		m_transforms.Delete();
		m_materials.Delete();
		m_materialRenderData.Delete();
		m_cullingData.Delete();
		for (uint32_t i = 0; i < m_ssboMaterials.GetCount(); ++i)
		{
			m_ssboTransforms.Get(i) = nullptr;
			m_ssboMaterials.Get(i) = nullptr;
		}
		m_ssboTransforms.Destroy();
		m_ssboMaterials.Destroy();
	}

	uint32_t SceneRenderer::CreateRenderList(const RenderPassInfo& info)
	{
		check(((info.pass & (RenderPass_Opaque|RenderPass_Transparent)) && !(info.pass & RenderPass_ShadowMap)) || (info.pass & RenderPass_ShadowMap) == info.pass);
		m_creationInfo.Push(info);
		m_renderPasses.Push(m_transforms.GetReservedSize());
		return m_renderPasses.GetSize()-1;
	}

	void SceneRenderer::SetRenderListInfo(uint32_t id, const RenderPassInfo& info)
	{
		check(id < m_creationInfo.GetSize());
		m_creationInfo[id] = info;
	}

	void SceneRenderer::BuildRenderLists(rendersystem::RenderSystem* rs, const Scene* scene)
	{
		PROF_SCOPE(BuildRenderLists);
		if (!scene)
			return;
		check(m_renderPasses.GetSize() == m_creationInfo.GetSize());

		// Clear
		for (uint32_t i = 0; i < m_renderPasses.GetSize(); ++i)
			m_renderPasses[i].Clear();
		m_transforms.Clear();
		m_materialRenderData.Clear();
		m_materials.Clear();
		m_cullingData.Clear();
		m_meshes.Clear();

		// Build render lists
		RenderListBuilder builder(*this);
		builder.Build(scene);

		// Submit buffers to gpu
		m_ssboTransforms.Next();
		m_ssboMaterials.Next();
		rs->GetDevice()->WriteBuffer(m_ssboTransforms.GetHead(), m_transforms.GetData(), m_transforms.GetSize() * sizeof(glm::mat4));
		rs->GetDevice()->WriteBuffer(m_ssboMaterials.GetHead(), m_materialRenderData.GetData(), m_materialRenderData.GetSize() * sizeof(sMaterialRenderData));
	}

	void SceneRenderer::DrawList(const RenderContext& renderContext)
	{
		check(renderContext.rs);
		check(renderContext.passId < m_renderPasses.GetSize());
		check(renderContext.passId < m_creationInfo.GetSize());

		RenderPass& pass = m_renderPasses[renderContext.passId];
		check(pass.m_items.GetSize() == pass.m_sortIds.GetSize());
		if (pass.m_sortIds.IsEmpty())
			return;

		renderContext.rs->BindUAV("u_modelBuffer", m_ssboTransforms.GetHead());
		renderContext.rs->BindUAV("u_prevModelBuffer", m_ssboTransforms.GetHead());
		renderContext.rs->BindUAV("u_materialBuffer", m_ssboMaterials.GetHead());

		m_state.Invalidate();
		if (!IsGeometryPass(m_creationInfo[renderContext.passId].pass))
		{
			PROF_SCOPE(Scene_Draw);

			if (CVar_RenderWireframe.Get())
			{
				renderContext.rs->SetFillMode(render::RasterFillMode_Line);
				renderContext.rs->SetLineWidth(Mist::math::Clamp(CVar_RenderWireframe.Get(), 0.0001f, FLT_MAX));
			}

			for (uint32_t i = 0; i < pass.m_sortIds.GetSize(); ++i)
				DrawItem(renderContext, pass.m_sortIds[i]);
		}
		else
		{
			PROF_SCOPE(Scene_DrawGeometry);
			for (uint32_t i = 0; i < pass.m_sortIds.GetSize(); ++i)
				DrawGeometryItem(renderContext, pass.m_sortIds[i]);
		}
	}

	void SceneRenderer::ImGuiDraw()
	{
		ImGui::Begin("SceneRenderer");
		{
			ImGui::Checkbox("Enable sorting", &m_enableSorting);
			ImGui::Checkbox("Enable culling", &m_enableCulling);
			char buff[256];
			for (uint32_t i = 0; i < m_renderPasses.GetSize(); ++i)
			{
				sprintf_s(buff, "RenderPass_%d ( ", i);
				if (m_creationInfo[i].pass & RenderPass_Opaque) strcat_s(buff, "Opaque");
				if (m_creationInfo[i].pass & RenderPass_Transparent) strcat_s(buff, "|Transparent");
				if (m_creationInfo[i].pass & RenderPass_ShadowMap) strcat_s(buff, "|ShadowMap");
				strcat_s(buff, " )");
				ImGui::PushID(buff);
				if (ImGui::TreeNode(buff))
				{
					ImGui::Text("Mask			: %d", m_creationInfo[i].pass);
					ImGui::Text("Item count		: %d", m_renderPasses[i].m_items.GetSize());
					if (ImGui::TreeNode("Draw list"))
					{
						for (uint32_t j = 0; j < m_renderPasses[i].m_sortIds.GetSize(); ++j)
						{
							const RenderSortId& id = m_renderPasses[i].m_sortIds[j];
							ImGui::Text("%4d - sort_id: 0x%016llx; mesh: %4d '%16s'; primitive: %4d; material: %4d",
								j,
								id.id,
								id.GetMesh(),
								m_meshes[id.GetMesh()]->GetName(),
								m_renderPasses[i].m_items[id.GetIndex()].primitive,
								id.GetMaterial());
						}
						ImGui::TreePop();
					}

					ImGui::TreePop();
				}
				ImGui::PopID();
			}
		}
		ImGui::End();
	}

	void SceneRenderer::Init(rendersystem::RenderSystem* rs)
	{
		check(!g_sceneRenderer);
		g_sceneRenderer = _new SceneRenderer(rs, 8, 1<<12);
	}

	void SceneRenderer::Destroy()
	{
		check(g_sceneRenderer);
		delete g_sceneRenderer;
	}

	SceneRenderer* SceneRenderer::GetSceneRenderer()
	{
		check(g_sceneRenderer);
		return g_sceneRenderer;
	}

	void SceneRenderer::BindMesh(const RenderContext& rc, const cMesh& mesh)
	{
		if (m_state.mesh != &mesh)
		{
			rc.rs->SetVertexBuffer(mesh.GetVertexBuffer());
			rc.rs->SetIndexBuffer(mesh.GetIndexBuffer());
			m_state.mesh = &mesh;
		}
	}

	void SceneRenderer::BindMaterial(const RenderContext& rc, const cMaterial& material)
	{
		if (m_state.material != &material)
		{
			if (!m_state.material || m_state.material->GetShaderProgram() != material.GetShaderProgram())
				rc.rs->SetShader(material.GetShaderProgram());

			material.BindTextures(rc.rs);
			m_state.material = &material;
		}
	}

	void SceneRenderer::DoCulling()
	{
#if 0
		PROF_SCOPE(Culling);
		if (!IsCullingEnabled())
			return;
		for (uint32_t i = 0; i < m_creationInfo.GetSize(); ++i)
		{
			// Culling already done before insert mesh on the list
			if (IsGeometryPass(m_creationInfo[i].pass))
				continue;
			const CameraData& cd = m_creationInfo[i].cameraData;
			Frustum f(cd.ViewProjection);
			RenderPass& pass = m_renderPasses[i];
			check(pass.cullingData.size() == pass.items.size());

			for (uint32_t j = 0; j < pass.cullingData.size(); ++j)
			{
				const AABB_t& aabb = pass.cullingData[j];
				if (IsAABBVisible(aabb, f))
					pass.drawList.emplace_back(j);
			}
		}
#endif // 0

	}

	void SceneRenderer::DrawItem(const RenderContext& renderContext, const RenderSortId& id)
	{
		const cMesh* mesh = m_meshes[id.GetMesh()];
		const RenderItem& item = m_renderPasses[renderContext.passId].m_items[id.GetIndex()];
		const PrimitiveMeshData& primitive = mesh->GetPrimitiveArray()[item.primitive];
		BindMesh(renderContext, *mesh);
		BindMaterial(renderContext, *primitive.material);

		RenderData_Id rdid;
		rdid.modelId = id.GetMesh();
		rdid.materialId = id.GetMaterial();
		renderContext.rs->PushConstant("u_instance", &rdid, sizeof(rdid));
		renderContext.rs->DrawIndexed(primitive.count, 1, primitive.firstIndex);
	}

	void SceneRenderer::DrawGeometryItem(const RenderContext& renderContext, const RenderSortId& id)
	{
		const cMesh* mesh = m_meshes[id.GetMesh()];
		BindMesh(renderContext, *mesh);
		RenderData_Id rdid;
		rdid.modelId = id.GetMesh();
		rdid.materialId = id.GetMaterial();
		renderContext.rs->PushConstant("u_instance", &rdid, sizeof(rdid));
		renderContext.rs->DrawIndexed(mesh->GetIndexCount());
	}

	void SceneRenderer::RenderListBuilder::Build(const Scene* scene)
	{
		const glm::mat4* worldTransforms = scene->GetRawGlobalTransforms();
		for (uint32_t i = 0; i < scene->GetRenderObjectCount(); ++i)
		{
			const ModelComponent* mc = scene->GetMesh(i);
			if (mc)
			{
				check(mc->model);
				const cModel* model = mc->model;// scene->GetModel(mc->MeshIndex);
				const glm::mat4& transform = worldTransforms[i];

				// Coarse culling
				bool visible = false;
				AABB_t aabb = model->GetAABB().ApplyTransform(transform);
				for (uint32_t pass = 0; pass < m_sceneRenderer.m_creationInfo.GetSize(); ++pass)
				{
					Frustum f = m_sceneRenderer.m_creationInfo[pass].cameraData.GetFrustum();
					if (m_sceneRenderer.IsVisible(aabb, f))
					{
						visible = true;
						break;
					}
				}

				if (visible)
				{
					uint16_t materialRoot = ProcessMaterials(model);
					ProcessModelNode(model, model->GetRoot(), transform, materialRoot);
				}
			}
		}

		if (m_sceneRenderer.m_enableSorting)
		{
			for (uint32_t passIndex = 0; passIndex < m_sceneRenderer.m_renderPasses.GetSize(); ++passIndex)
			{
				RenderPass& pass = m_sceneRenderer.m_renderPasses[passIndex];
				qsort_s(pass.m_sortIds.GetData(), pass.m_sortIds.GetSize(), sizeof(RenderSortId), &CompareRenderSortId, nullptr);
			}
		}

		//DoCulling();
	}

	void SceneRenderer::RenderListBuilder::ProcessModelNode(const cModel* model, index_t nodeIndex, const glm::mat4& parentTransform, uint16_t materialRoot)
	{
		check(model && nodeIndex != index_invalid);
		glm::mat4 transform = parentTransform * model->GetTransform(nodeIndex);
		const cModel::Node& node = *model->GetNode(nodeIndex);
		if (node.meshInfoIndex != index_invalid)
			ProcessMesh(*model, *model->GetMeshFromNode(nodeIndex), transform, materialRoot);
		if (node.Sibling != index_invalid)
			ProcessModelNode(model, node.Sibling, parentTransform, materialRoot);
		if (node.Child != index_invalid)
			ProcessModelNode(model, node.Child, transform, materialRoot);
	}

	void SceneRenderer::RenderListBuilder::ProcessMesh(const cModel& model, const cMesh& mesh, const glm::mat4& nodeWorldTransform, uint16_t materialRoot)
	{
		if (!SceneFilter(mesh.GetName()))
			return;

		m_sceneRenderer.m_transforms.Push(nodeWorldTransform);
		uint16_t transformIndex = u32_to_u16(m_sceneRenderer.m_transforms.GetSize() - 1);

		m_sceneRenderer.m_meshes.Push(&mesh);
		uint16_t meshIndex = u32_to_u16(m_sceneRenderer.m_meshes.GetSize() - 1);

		static constexpr uint32_t maxCount = 8;
		check(m_sceneRenderer.m_creationInfo.GetSize() <= maxCount);
		struct
		{
			uint32_t indices[maxCount];
			uint32_t index = 0;
		} primitiveRenderPasses;

		AABB_t meshWorldAABB = mesh.GetAABB().ApplyTransform(nodeWorldTransform);
		for (uint32_t passIndex = 0; passIndex < m_sceneRenderer.m_creationInfo.GetSize(); ++passIndex)
		{
			const RenderPassInfo& passInfo = m_sceneRenderer.m_creationInfo[passIndex];
			if (passInfo.pass & mesh.GetRenderPassMask()
				&& m_sceneRenderer.IsVisible(meshWorldAABB, m_sceneRenderer.m_creationInfo[passIndex].cameraData.GetFrustum()))
			{
				// Only push mesh geometry in geometry pass. All the VB and IB in one draw call.
				if (IsGeometryPass(m_sceneRenderer.m_creationInfo[passIndex].pass))
				{
					RenderPass& pass = m_sceneRenderer.m_renderPasses[passIndex];
					pass.m_items.Push();
					pass.m_items.Back().primitive = 0;
					pass.m_sortIds.Push();
					pass.m_sortIds.Back().Set(0, meshIndex, 0, pass.m_items.GetSize() - 1);
				}
				else
				{
					// Mark current render pass to process primitives
					check(primitiveRenderPasses.index < Mist::CountOf(primitiveRenderPasses.indices));
					primitiveRenderPasses.indices[primitiveRenderPasses.index++] = passIndex;
				}
			}
		}

		// Iterate primitives
		for (uint32_t primitiveIndex = 0; primitiveIndex < mesh.GetPrimitiveCount(); ++primitiveIndex)
		{
			const PrimitiveMeshData& primitive = mesh.GetPrimitiveArray()[primitiveIndex];
			AABB_t primitiveWorldAABB = primitive.aabb.ApplyTransform(nodeWorldTransform);
			m_sceneRenderer.m_cullingData.Push(primitiveWorldAABB);

			for (uint32_t passIndex = 0; passIndex < primitiveRenderPasses.index; ++passIndex)
			{
				const RenderPassInfo& passInfo = m_sceneRenderer.m_creationInfo[primitiveRenderPasses.indices[passIndex]];
				RenderPass& pass = m_sceneRenderer.m_renderPasses[primitiveRenderPasses.indices[passIndex]];
				if (passInfo.pass & primitive.renderPassMask && m_sceneRenderer.IsVisible(primitiveWorldAABB, passInfo.cameraData.GetFrustum()))
				{
					// Push item with primitive info
					pass.m_items.Push();
					pass.m_items.Back().primitive = primitiveIndex;

					// Push sort id to do the sort of the list
					uint16_t materialIndex = u64_to_u16(size_t(primitive.material - &model.GetMaterial(0)));
					check(materialIndex < u32_to_u16(model.GetMaterialCount()));
					materialIndex += materialRoot;

					pass.m_sortIds.Push();
					pass.m_sortIds.Back().Set(materialIndex, meshIndex, 0, pass.m_items.GetSize() - 1);
					break;
				}
			}
		}
	}

	uint16_t SceneRenderer::RenderListBuilder::ProcessMaterials(const cModel* model)
	{
		check(m_sceneRenderer.m_materials.GetSize() == m_sceneRenderer.m_materialRenderData.GetSize());
		uint16_t currentRoot = m_sceneRenderer.m_materials.GetSize();
		for (uint32_t mtlIndex = 0; mtlIndex < model->GetMaterialCount(); ++mtlIndex)
		{
			const cMaterial& mtl = model->GetMaterial(mtlIndex);
			m_sceneRenderer.m_materialRenderData.Push(mtl.GetRenderData());
			m_sceneRenderer.m_materials.Push(&mtl);
		}
		return currentRoot;
	}
}